/*
 * arch/arm/mm/proc-v7-2level.S
 *
 * Copyright (C) 2001 Deep Blue Solutions Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define TTB_S		(1 << 1) // Shareable 
#define TTB_RGN_NC	(0 << 3)
#define TTB_RGN_OC_WBWA	(1 << 3) // Normal memory, Outer Write-Back Write-Allocate Cacheable
#define TTB_RGN_OC_WT	(2 << 3)
#define TTB_RGN_OC_WB	(3 << 3)
#define TTB_NOS		(1 << 5) // Inner Shareable
#define TTB_IRGN_NC	((0 << 0) | (0 << 6))
#define TTB_IRGN_WBWA	((0 << 0) | (1 << 6)) // Normal memory, Inner Write-Through Cacheable
#define TTB_IRGN_WT	((1 << 0) | (0 << 6))
#define TTB_IRGN_WB	((1 << 0) | (1 << 6))

/* PTWs cacheable, inner WB not shareable, outer WB not shareable */
#define TTB_FLAGS_UP	TTB_IRGN_WB|TTB_RGN_OC_WB
#define PMD_FLAGS_UP	PMD_SECT_WB

/* PTWs cacheable, inner WBWA shareable, outer WBWA not shareable */
// 2016-12-17
// ((1 << 6) | (1 << 1) | (1 << 5) | (1 << 3))
#define TTB_FLAGS_SMP	TTB_IRGN_WBWA|TTB_S|TTB_NOS|TTB_RGN_OC_WBWA
#define PMD_FLAGS_SMP	PMD_SECT_WBWA|PMD_SECT_S

/*
 *	cpu_v7_switch_mm(pgd_phys, tsk)
 *
 *	Set the translation table base pointer to be pgd_phys
 *
 *	- pgd_phys - physical address of new TTB
 *
 *	It is assumed that:
 *	- we are not using split page tables
 */
ENTRY(cpu_v7_switch_mm)
// 2016-12-17
// cpu_v7_switch_mm(virt_to_phys(mm->pgd), mm);
#ifdef CONFIG_MMU
	mov	r2, #0
	mmid	r1, r1				@ get mm->context.id
	                                        @ // .macro mmid, rd, rn
	ALT_SMP(orr	r0, r0, #TTB_FLAGS_SMP)
	ALT_UP(orr	r0, r0, #TTB_FLAGS_UP)
#ifdef CONFIG_ARM_ERRATA_430973 // not define
	mcr	p15, 0, r2, c7, c5, 6		@ flush BTAC/BTB
#endif
#ifdef CONFIG_PID_IN_CONTEXTIDR // not define
	mrc	p15, 0, r2, c13, c0, 1		@ read current context ID
	lsr	r2, r2, #8			@ extract the PID
	bfi	r1, r2, #8, #24			@ insert into new context ID
#endif
#ifdef CONFIG_ARM_ERRATA_754322 // not define
	dsb
#endif
	mcr	p15, 0, r1, c13, c0, 1		@ set context ID
	                                        @ // CONTEXTIDR(Context ID Register)
	isb
	mcr	p15, 0, r0, c2, c0, 0		@ set TTB 0
	                                        @ // TTBR0(Translation Table 
						@ //          Base Register 0 and Register
	isb
#endif
	mov	pc, lr
ENDPROC(cpu_v7_switch_mm)

/*
 *	cpu_v7_set_pte_ext(ptep, pte)
 *
 *	Set a level 2 translation table entry.
 *
 *	- ptep  - pointer to level 2 translation table entry
 *		  (hardware version is stored at +2048 bytes)
 *	- pte   - PTE value to store
 *	- ext	- value for extended PTE bits
 */
// 2015-01-31
// 2015-08-22
// 2015-10-03
// cpu_set_pte_ext(ptep,pte,ext)
// 2015-10-10; cpu_v7_set_pte_ext(pte, swp_pte, ext)
// 2015-12-26
// // 2015-12-26, cpu_set_pte_ext(ptep, __pte(0), 0)
ENTRY(cpu_v7_set_pte_ext)
#ifdef CONFIG_MMU
	str	r1, [r0]			@ linux version
						// *r0 = r1;

	bic	r3, r1, #0x000003f0
	bic	r3, r3, #PTE_TYPE_MASK		// (_AT(pteval_t, 3) << 0), 하위 2비트
	orr	r3, r3, r2
	orr	r3, r3, #PTE_EXT_AP0 | 2	// PTE_EXT_AP0     (_AT(pteval_t, 1) << 4)

	tst	r1, #1 << 4			// PTE_EXT_AP0 설정 유무 체크
	orrne	r3, r3, #PTE_EXT_TEX(1)		// PTE_EXT_TEX(x)      (_AT(pteval_t, (x)) << 6) /* v5 */
						// 설정이 안되어 있으면, PTE_EXT_TEX라도 설정하는 것

	eor	r1, r1, #L_PTE_DIRTY		// L_PTE_DIRTY     (_AT(pteval_t, 1) << 6)
	tst	r1, #L_PTE_RDONLY | L_PTE_DIRTY	// L_PTE_RDONLY        (_AT(pteval_t, 1) << 7)
	orrne	r3, r3, #PTE_EXT_APX		// PTE_EXT_APX     (_AT(pteval_t, 1) << 9) /* v6 */

	tst	r1, #L_PTE_USER			// L_PTE_USER      (_AT(pteval_t, 1) << 8)
	orrne	r3, r3, #PTE_EXT_AP1            // PTE_EXT_AP1     (_AT(pteval_t, 2) << 4)

	tst	r1, #L_PTE_XN			// L_PTE_XN        (_AT(pteval_t, 1) << 9)
	orrne	r3, r3, #PTE_EXT_XN             // PTE_EXT_XN      (_AT(pteval_t, 1) << 0)     /* v6 */

	tst	r1, #L_PTE_YOUNG		// L_PTE_YOUNG     (_AT(pteval_t, 1) << 1)
	tstne	r1, #L_PTE_VALID		// L_PTE_VALID     (_AT(pteval_t, 1) << 0)     /* Valid */
	eorne	r1, r1, #L_PTE_NONE		// L_PTE_NONE      (_AT(pteval_t, 1) << 11)
	tstne	r1, #L_PTE_NONE
	moveq	r3, #0

 ARM(	str	r3, [r0, #2048]! )		// *(r0 + 2048) = r3; r0 += 2048;
 THUMB(	add	r0, r0, #2048 )
 THUMB(	str	r3, [r0] )
	ALT_SMP(W(nop))
	// 2015-08-22, DCCMVAC 
	// Clean data cache line by MVA to PoC,
	// PoC = to Point of Coherence
	// MVA = Modified virtual address
	ALT_UP (mcr	p15, 0, r0, c7, c10, 1)		@ flush_pte
							// DCCMVAC, clean data cache line 
#endif
	mov	pc, lr
ENDPROC(cpu_v7_set_pte_ext)

	/*
	 * Memory region attributes with SCTLR.TRE=1
	 *
	 *   n = TEX[0],C,B
	 *   TR = PRRR[2n+1:2n]		- memory type
	 *   IR = NMRR[2n+1:2n]		- inner cacheable property
	 *   OR = NMRR[2n+17:2n+16]	- outer cacheable property
	 *
	 *			n	TR	IR	OR
	 *   UNCACHED		000	00
	 *   BUFFERABLE		001	10	00	00
	 *   WRITETHROUGH	010	10	10	10
	 *   WRITEBACK		011	10	11	11
	 *   reserved		110
	 *   WRITEALLOC		111	10	01	01
	 *   DEV_SHARED		100	01
	 *   DEV_NONSHARED	100	01
	 *   DEV_WC		001	10
	 *   DEV_CACHED		011	10
	 *
	 * Other attributes:
	 *
	 *   DS0 = PRRR[16] = 0		- device shareable property
	 *   DS1 = PRRR[17] = 1		- device shareable property
	 *   NS0 = PRRR[18] = 0		- normal shareable property
	 *   NS1 = PRRR[19] = 1		- normal shareable property
	 *   NOS = PRRR[24+n] = 1	- not outer shareable
	 */
.equ	PRRR,	0xff0a81a8
.equ	NMRR,	0x40e040e0

	/*
	 * Macro for setting up the TTBRx and TTBCR registers.
	 * - \ttb0 and \ttb1 updated with the corresponding flags.
	 */
	.macro	v7_ttb_setup, zero, ttbr0, ttbr1, tmp
	mcr	p15, 0, \zero, c2, c0, 2	@ TTB control register
	ALT_SMP(orr	\ttbr0, \ttbr0, #TTB_FLAGS_SMP)
	ALT_UP(orr	\ttbr0, \ttbr0, #TTB_FLAGS_UP)
	ALT_SMP(orr	\ttbr1, \ttbr1, #TTB_FLAGS_SMP)
	ALT_UP(orr	\ttbr1, \ttbr1, #TTB_FLAGS_UP)
	mcr	p15, 0, \ttbr1, c2, c0, 1	@ load TTB1
	.endm

	/*   AT
	 *  TFR   EV X F   I D LR    S
	 * .EEE ..EE PUI. .T.T 4RVI ZWRS BLDP WCAM
	 * rxxx rrxx xxx0 0101 xxxx xxxx x111 xxxx < forced
	 *   01    0 110       0011 1100 .111 1101 < we want
	 */
	.align	2
	.type	v7_crval, #object
v7_crval:
	crval	clear=0x2120c302, mmuset=0x10c03c7d, ucset=0x00c01c7c
