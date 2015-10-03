/*
 *  linux/arch/arm/mm/cache-v7.S
 *
 *  Copyright (C) 2001 Deep Blue Solutions Ltd.
 *  Copyright (C) 2005 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  This is the "shell" of the ARMv7 processor support.
 */
#include <linux/linkage.h>
#include <linux/init.h>
#include <asm/assembler.h>
#include <asm/errno.h>
#include <asm/unwind.h>

#include "proc-macros.S"

/*
 * The secondary kernel init calls v7_flush_dcache_all before it enables
 * the L1; however, the L1 comes out of reset in an undefined state, so
 * the clean + invalidate performed by v7_flush_dcache_all causes a bunch
 * of cache lines with uninitialized data and uninitialized tags to get
 * written out to memory, which does really unpleasant things to the main
 * processor.  We fix this by performing an invalidate, rather than a
 * clean + invalidate, before jumping into the kernel.
 *
 * This function is cloned from arch/arm/mach-tegra/headsmp.S, and needs
 * to be called for both secondary cores startup and primary core resume
 * procedures.
 */
ENTRY(v7_invalidate_l1)
       mov     r0, #0
       mcr     p15, 2, r0, c0, c0, 0
       mrc     p15, 1, r0, c0, c0, 0

       ldr     r1, =0x7fff
       and     r2, r1, r0, lsr #13

       ldr     r1, =0x3ff

       and     r3, r1, r0, lsr #3      @ NumWays - 1
       add     r2, r2, #1              @ NumSets

       and     r0, r0, #0x7
       add     r0, r0, #4      @ SetShift

       clz     r1, r3          @ WayShift
       add     r4, r3, #1      @ NumWays
1:     sub     r2, r2, #1      @ NumSets--
       mov     r3, r4          @ Temp = NumWays
2:     subs    r3, r3, #1      @ Temp--
       mov     r5, r3, lsl r1
       mov     r6, r2, lsl r0
       orr     r5, r5, r6      @ Reg = (Temp<<WayShift)|(NumSets<<SetShift)
       mcr     p15, 0, r5, c7, c6, 2
       bgt     2b
       cmp     r2, #0
       bgt     1b
       dsb
       isb
       mov     pc, lr
ENDPROC(v7_invalidate_l1)

/*
 *	v7_flush_icache_all()
 *
 *	Flush the whole I-cache.
 *
 *	Registers:
 *	r0 - set to 0
 */
ENTRY(v7_flush_icache_all)
	mov	r0, #0
	ALT_SMP(mcr	p15, 0, r0, c7, c1, 0)		@ invalidate I-cache inner shareable
	ALT_UP(mcr	p15, 0, r0, c7, c5, 0)		@ I+BTB cache invalidate
	mov	pc, lr
ENDPROC(v7_flush_icache_all)

 /*
 *     v7_flush_dcache_louis()
 *
 *     Flush the D-cache up to the Level of Unification Inner Shareable
 *
 *     Corrupted registers: r0-r7, r9-r11 (r6 only in Thumb mode)
 */

ENTRY(v7_flush_dcache_louis)
	dmb					@ ensure ordering with previous memory accesses
						//데이타 메모리 배리어. dmb전후의 명령어가 구분이 되도록?? dmb 이전의 명령어가 다 처리후에 dmb후 명령어가 처리되도록 하는것. 플러시를 하기전에 모든 메모리 엑세스를 완료하고 진행하기 위함
	mrc	p15, 1, r0, c0, c0, 1		@ read clidr, r0 = clidr
	ALT_SMP(ands	r3, r0, #(7 << 21))	@ extract LoUIS from clidr
	//SMP에서 L2 cache를 같이 쓰는지 에 대한 값을 가져옴. 001 (0*20)
	//23 22 21   r0: 0 0 1   & 1 1 1 = 001 => Z =0 => not eq    
	ALT_UP(ands	r3, r0, #(7 << 27))	@ extract LoUU from clidr
#ifdef CONFIG_ARM_ERRATA_643719 //not set
	ALT_SMP(mrceq	p15, 0, r2, c0, c0, 0)	@ read main ID register
	ALT_UP(moveq	pc, lr)			@ LoUU is zero, so nothing to do
	ldreq	r1, =0x410fc090                 @ ID of ARM Cortex A9 r0p?
	biceq	r2, r2, #0x0000000f             @ clear minor revision number
	teqeq	r2, r1                          @ test for errata affected core and if so...
	orreqs	r3, #(1 << 21)			@   fix LoUIS value (and set flags state to 'ne')
#endif
	ALT_SMP(mov	r3, r3, lsr #20)	@ r3 = LoUIS * 2
	//위에서 21번을 왼쪽으로 옴겨 21번비트부터 001을 구햇는데 그걸 다시
	//20번을 오른쪽으로 20번으로 옴겨 결과적으로 0010 이 되서 2배가 된다는 주석
	ALT_UP(mov	r3, r3, lsr #26)	@ r3 = LoUU * 2
	moveq	pc, lr				@ return if level == 0 
	// 94에서 0의 결과가 나왔으면 실행, 아니면 넘어감
	mov	r10, #0				@ r10 (starting level) = 0
	b	flush_levels			@ start flushing cache levels
ENDPROC(v7_flush_dcache_louis)

/*
 *	v7_flush_dcache_all()
 *
 *	Flush the whole D-cache.
 *
 *	Corrupted registers: r0-r7, r9-r11 (r6 only in Thumb mode)
 *
 *	- mm    - mm_struct describing address space
 */
ENTRY(v7_flush_dcache_all)
	dmb					@ ensure ordering with previous memory accesses
	mrc	p15, 1, r0, c0, c0, 1		@ read clidr
	ands	r3, r0, #0x7000000		@ extract loc from clidr
	mov	r3, r3, lsr #23			@ left align loc bit field
	beq	finished			@ if loc is 0, then no need to clean
	mov	r10, #0				@ start clean at cache level 0
flush_levels:
// clidr의 각레벨의 CType을 구해 louis*2와 비교
	add	r2, r10, r10, lsr #1		@ work out 3x current cache level
	//skip 레이블에서 r10이 2씩증가. 0 -> 3 ->7 
	//첫번째 :r10 = 0, r2 = r10 + r10 >>1, r2 = 0 (Ctype 1  
	//두번째 :r10 = 2, r2 = r10 + r10 >>1, r2 = 3 (Ctype 2
	//세번째 :r10 = 4, r2 = r10 + r10 >>1, r2 = 6 (Ctype 3 ...
	/* clidr 의 하위비트는 3비트 단위로 레벨1~레벨7(Ctype)의 값이 있는데 이 3비트영역을 얻기위해 3을 곱하는 식으로 연산을 한것.
	*/
	mov	r1, r0, lsr r2			@ extract cache type bits from clidr
	//위에서 얻은 r2값으로 r0를 시프트(3비트단위)해서 Ctype을 구해 r1 에 적재
	and	r1, r1, #7			@ mask of the bits for current cache only
	//하위 3비트(Ctype)을 구해 r1에 저장
	cmp	r1, #2				@ see what cache we have at this level
	//Ctpe == 2 ?
	blt	skip				@ skip if no cache, or just i-cache
	// less than 즉 Ctype 이 1이나 0이면 skip레이블 실행
	// CortexA15에서는 Ctype1이 0011이므로 skip을 한번도 안함.
	// 이거는L1이 data cache랑 instruction cache 랑 나눠져 있기 때문에
	//skip을 안하고 바로 플러시를 하는것.
	/*
	DMB - data memory barrier operation
	(DMB 이후의 명시적인 메모리 액세스를 시작하기 전에, DMB 이전의 모든 명시적인 메모리 액세스를 완료하도록 한다.)

	DSB - data synchronization barrier operation
	(DSB 이전의 모든 명령들을 완료하도록 한다.)

	ISB - Instruction synchronization barrier operation
	(파이프라인을 비워서, ISB 이후의 모든 명령들이 캐시나 메모리로부터 새로 fetch되도록 한다.)
	*/
#ifdef CONFIG_PREEMPT //set
	save_and_disable_irqs_notrace r9	@ make cssr&csidr read atomic
	//include/asm/assembler.h
	//r9 에 cpsr 적재
	//cpsid(cpsr i disable) i 라는 명령어가 나옴 : 지정된 인터럽트(여기선 그냥 i)를 비활성화하고 선택적으로 모드 변경
	//cpsie (cpsr i enable) 란것도있음
#endif
	mcr	p15, 2, r10, c0, c0, 0		@ select current cache level in cssr
	//Cache Size Selection Register
	//0 -> cssr -> 0번비트 0: Data or unified cache. 1: instruction cache
	// 즉 data cache로 설정을 했고, level은(1번비트~3번비트) 000이므로 1이다.
	isb					@ isb to sych the new cssr&csidr
	mrc	p15, 1, r1, c0, c0, 0		@ read the new csidr Cache Size ID Register
#ifdef CONFIG_PREEMPT
	restore_irqs_notrace r9
	//r9의 값을 cpsr의 적재 cpsr 의 c 영역(mode) . irq가 처음으로 원복(i enable)
#endif
	and	r2, r1, #7			@ extract the length of the cache lines
	// cache line size(캐시 한줄의 사이즈. 한번에 읽을수 있는.) 를 r2에 적재
	// r2 = 010
	add	r2, r2, #4			@ add 4 (line length offset)
	// r2= 6;
	ldr	r4, =0x3ff //하위 10비트를 거르기 위한 준비
	ands	r4, r4, r1, lsr #3		@ find maximum number on the way size
	//number of ways 를 구해 r4에 저장. r4
	clz	r5, r4				@ find bit position of way size increment
	//number of ways 앞에 오는 0의 수를 계산해서 r5에 적재 ex:101000이면 3
	ldr	r7, =0x7fff
	ands	r7, r7, r1, lsr #13		@ extract max number of the index size
	//number of sets in cache의 값을 r7에 저장
	/*
	2way =   1000 000000 
	16way == 1111 000000 
	0의 갯수를 읽음으로 몇 way인지 알수 있다. 
	*/
	/*
	http://iamroot.org/wiki/doku.php?id=%EC%8A%A4%ED%84%B0%EB%94%94:arm_kernel_%EC%8A%A4%ED%84%B0%EB%94%94_10%EC%B0%A8_full_10-19#주차_20130629
	http://poohyhoh.blogspot.kr/2011/01/arm-sdg-ch12-caches-05-1224-set.html
	line : 한번에 읽는 데이터 단위
	way : 라인들의 집함. 한 way에 64개의 라인이 있
	set : 각 way의 line들을 동시에 가리킴
	*/
loop1:
	mov	r9, r4				@ create working copy of max way size
loop2:
 ARM(	orr	r11, r10, r9, lsl r5	)	@ factor way and cache number into r11
 /*r0 :clidr r1: csidr r2: cache line size+offset =6 r3:louis 
 	r4: number of ways, r5: number of ways 의 0의 갯수,
	 r7: number of set int cache 의 갯수 r9: numer of ways
	 r10: 0
	 r11: ??
 */
//r11 에 way 개수 구해서 적재
 THUMB(	lsl	r6, r9, r5		)
 THUMB(	orr	r11, r10, r6		)	@ factor way and cache number into r11
 ARM(	orr	r11, r11, r7, lsl r2	)	@ factor index number into r11
 // r11에 set개수 구해서 OR
 THUMB(	lsl	r6, r7, r2		)
 THUMB(	orr	r11, r11, r6		)	@ factor index number into r11
	mcr	p15, 0, r11, c7, c14, 2		@ clean & invalidate by set/way
	//Data Cache Clean and Invalidate by Set/Way, VMSA
	//r11에 구한 해당 라인을 클리어
	subs	r9, r9, #1			@ decrement the way
	bge	loop2
	subs	r7, r7, #1			@ decrement the index
	bge	loop1
	//set 과 way를 돌아가면서 라인클 클리어 하는것.
skip:
	add	r10, r10, #2			@ increment cache number
	// 그 다음 cache level을 초기화하기위해 준비.
	cmp	r3, r10
	//  data cache가 더이상 존재하는지 안하는지를 검사. 있으면 아래 레이블로 점프
	bgt	flush_levels
	//r3가 r10보다 더 크면. 즉 Ctypes이 0010보다 크면.실행
	//여기까지 코드가 data cache를 clean에 대한것.
finished:
	mov	r10, #0				@ swith back to cache level 0
	mcr	p15, 2, r10, c0, c0, 0		@ select current cache level in cssr
	//Cache Size Selection Register 
	// L1 data cache 로. 선택
	dsb
	isb
	mov	pc, lr//proc-v7.s 262로 복귀
ENDPROC(v7_flush_dcache_all)

/*
 *	v7_flush_cache_all()
 *
 *	Flush the entire cache system.
 *  The data cache flush is now achieved using atomic clean / invalidates
 *  working outwards from L1 cache. This is done using Set/Way based cache
 *  maintenance instructions.
 *  The instruction cache can still be invalidated back to the point of
 *  unification in a single instruction.
 *
 */
// 2014년 11월 01일 자세히 보지 않음
ENTRY(v7_flush_kern_cache_all)
 ARM(	stmfd	sp!, {r4-r5, r7, r9-r11, lr}	)
 THUMB(	stmfd	sp!, {r4-r7, r9-r11, lr}	)
	bl	v7_flush_dcache_all
	mov	r0, #0
	ALT_SMP(mcr	p15, 0, r0, c7, c1, 0)	@ invalidate I-cache inner shareable
	ALT_UP(mcr	p15, 0, r0, c7, c5, 0)	@ I+BTB cache invalidate
 ARM(	ldmfd	sp!, {r4-r5, r7, r9-r11, lr}	)
 THUMB(	ldmfd	sp!, {r4-r7, r9-r11, lr}	)
	mov	pc, lr
ENDPROC(v7_flush_kern_cache_all)

 /*
 *     v7_flush_kern_cache_louis(void)
 *
 *     Flush the data cache up to Level of Unification Inner Shareable.
 *     Invalidate the I-cache to the point of unification.
 */
ENTRY(v7_flush_kern_cache_louis)
 ARM(	stmfd	sp!, {r4-r5, r7, r9-r11, lr}	)
 THUMB(	stmfd	sp!, {r4-r7, r9-r11, lr}	)
	bl	v7_flush_dcache_louis
	mov	r0, #0
	ALT_SMP(mcr	p15, 0, r0, c7, c1, 0)	@ invalidate I-cache inner shareable
	ALT_UP(mcr	p15, 0, r0, c7, c5, 0)	@ I+BTB cache invalidate
 ARM(	ldmfd	sp!, {r4-r5, r7, r9-r11, lr}	)
 THUMB(	ldmfd	sp!, {r4-r7, r9-r11, lr}	)
	mov	pc, lr
ENDPROC(v7_flush_kern_cache_louis)

/*
 *	v7_flush_cache_all()
 *
 *	Flush all TLB entries in a particular address space
 *
 *	- mm    - mm_struct describing address space
 */
ENTRY(v7_flush_user_cache_all)
	/*FALLTHROUGH*/

/*
 *	v7_flush_cache_range(start, end, flags)
 *
 *	Flush a range of TLB entries in the specified address space.
 *
 *	- start - start address (may not be aligned)
 *	- end   - end address (exclusive, may not be aligned)
 *	- flags	- vm_area_struct flags describing address space
 *
 *	It is assumed that:
 *	- we have a VIPT cache.
 */
ENTRY(v7_flush_user_cache_range)
	mov	pc, lr
ENDPROC(v7_flush_user_cache_all)
ENDPROC(v7_flush_user_cache_range)

/*
 *	v7_coherent_kern_range(start,end)
 *
 *	Ensure that the I and D caches are coherent within specified
 *	region.  This is typically used when code has been written to
 *	a memory region, and will be executed.
 *
 *	- start   - virtual start address of region
 *	- end     - virtual end address of region
 *
 *	It is assumed that:
 *	- the Icache does not read data from the write buffer
 */
ENTRY(v7_coherent_kern_range)
	/* FALLTHROUGH */

/*
 *	v7_coherent_user_range(start,end)
 *
 *	Ensure that the I and D caches are coherent within specified
 *	region.  This is typically used when code has been written to
 *	a memory region, and will be executed.
 *
 *	- start   - virtual start address of region
 *	- end     - virtual end address of region
 *
 *	It is assumed that:
 *	- the Icache does not read data from the write buffer
 */
ENTRY(v7_coherent_user_range)
 UNWIND(.fnstart		)
	dcache_line_size r2, r3
	sub	r3, r2, #1
	bic	r12, r0, r3
#ifdef CONFIG_ARM_ERRATA_764369
	ALT_SMP(W(dsb))
	ALT_UP(W(nop))
#endif
1:
 USER(	mcr	p15, 0, r12, c7, c11, 1	)	@ clean D line to the point of unification
	add	r12, r12, r2
	cmp	r12, r1
	blo	1b
	dsb	ishst
	icache_line_size r2, r3
	sub	r3, r2, #1
	bic	r12, r0, r3
2:
 USER(	mcr	p15, 0, r12, c7, c5, 1	)	@ invalidate I line
	add	r12, r12, r2
	cmp	r12, r1
	blo	2b
	mov	r0, #0
	ALT_SMP(mcr	p15, 0, r0, c7, c1, 6)	@ invalidate BTB Inner Shareable
	ALT_UP(mcr	p15, 0, r0, c7, c5, 6)	@ invalidate BTB
	dsb	ishst
	isb
	mov	pc, lr

/*
 * Fault handling for the cache operation above. If the virtual address in r0
 * isn't mapped, fail with -EFAULT.
 */
9001:
#ifdef CONFIG_ARM_ERRATA_775420
	dsb
#endif
	mov	r0, #-EFAULT
	mov	pc, lr
 UNWIND(.fnend		)
ENDPROC(v7_coherent_kern_range)
ENDPROC(v7_coherent_user_range)

/*
 *	v7_flush_kern_dcache_area(void *addr, size_t size)
 *
 *	Ensure that the data held in the page kaddr is written back
 *	to the page in question.
 *
 *	- addr	- kernel address
 *	- size	- region size
 */
// 2015-10-03
ENTRY(v7_flush_kern_dcache_area)
	dcache_line_size r2, r3  /* r2 = 64, r3 = 4 */
	add	r1, r0, r1 /* r1 += r0*/
	sub	r3, r2, #1 /* r3 = 64 - 1 */
	bic	r0, r0, r3 /* r0 = r0 & (!r3) */
#ifdef CONFIG_ARM_ERRATA_764369
	ALT_SMP(W(dsb))
	ALT_UP(W(nop))
#endif
1:
	mcr	p15, 0, r0, c7, c14, 1		@ clean & invalidate D line / unified line
	add	r0, r0, r2             /* r0 += r2*/
	cmp	r0, r1                 /* if (r0 > r1) goto 1; */
	blo	1b
	dsb
	mov	pc, lr
ENDPROC(v7_flush_kern_dcache_area)

/*
 *	v7_dma_inv_range(start,end)
 *
 *	Invalidate the data cache within the specified region; we will
 *	be performing a DMA operation in this region and we want to
 *	purge old data in the cache.
 *
 *	- start   - virtual start address of region
 *	- end     - virtual end address of region
 */
v7_dma_inv_range:
	dcache_line_size r2, r3
	sub	r3, r2, #1
	tst	r0, r3
	bic	r0, r0, r3
#ifdef CONFIG_ARM_ERRATA_764369
	ALT_SMP(W(dsb))
	ALT_UP(W(nop))
#endif
	mcrne	p15, 0, r0, c7, c14, 1		@ clean & invalidate D / U line

	tst	r1, r3
	bic	r1, r1, r3
	mcrne	p15, 0, r1, c7, c14, 1		@ clean & invalidate D / U line
1:
	mcr	p15, 0, r0, c7, c6, 1		@ invalidate D / U line
	add	r0, r0, r2
	cmp	r0, r1
	blo	1b
	dsb
	mov	pc, lr
ENDPROC(v7_dma_inv_range)

/*
 *	v7_dma_clean_range(start,end)
 *	- start   - virtual start address of region
 *	- end     - virtual end address of region
 */
v7_dma_clean_range:
	dcache_line_size r2, r3
	sub	r3, r2, #1
	bic	r0, r0, r3
#ifdef CONFIG_ARM_ERRATA_764369
	ALT_SMP(W(dsb))
	ALT_UP(W(nop))
#endif
1:
	mcr	p15, 0, r0, c7, c10, 1		@ clean D / U line
	add	r0, r0, r2
	cmp	r0, r1
	blo	1b
	dsb
	mov	pc, lr
ENDPROC(v7_dma_clean_range)

/*
 *	v7_dma_flush_range(start,end)
 *	- start   - virtual start address of region
 *	- end     - virtual end address of region
 */
ENTRY(v7_dma_flush_range)
	dcache_line_size r2, r3
	sub	r3, r2, #1
	bic	r0, r0, r3
#ifdef CONFIG_ARM_ERRATA_764369
	ALT_SMP(W(dsb))
	ALT_UP(W(nop))
#endif
1:
	mcr	p15, 0, r0, c7, c14, 1		@ clean & invalidate D / U line
	add	r0, r0, r2
	cmp	r0, r1
	blo	1b
	dsb
	mov	pc, lr
ENDPROC(v7_dma_flush_range)

/*
 *	dma_map_area(start, size, dir)
 *	- start	- kernel virtual start address
 *	- size	- size of region
 *	- dir	- DMA direction
 */
ENTRY(v7_dma_map_area)
	add	r1, r1, r0
	teq	r2, #DMA_FROM_DEVICE
	beq	v7_dma_inv_range
	b	v7_dma_clean_range
ENDPROC(v7_dma_map_area)

/*
 *	dma_unmap_area(start, size, dir)
 *	- start	- kernel virtual start address
 *	- size	- size of region
 *	- dir	- DMA direction
 */
ENTRY(v7_dma_unmap_area)
	add	r1, r1, r0
	teq	r2, #DMA_TO_DEVICE
	bne	v7_dma_inv_range
	mov	pc, lr
ENDPROC(v7_dma_unmap_area)

	__INITDATA

	@ define struct cpu_cache_fns (see <asm/cacheflush.h> and proc-macros.S)
	define_cache_functions v7
