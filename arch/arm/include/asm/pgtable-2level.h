/*
 *  arch/arm/include/asm/pgtable-2level.h
 *
 *  Copyright (C) 1995-2002 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _ASM_PGTABLE_2LEVEL_H
#define _ASM_PGTABLE_2LEVEL_H

/*
 * Hardware-wise, we have a two level page table structure, where the first
 * level has 4096 entries, and the second level has 256 entries.  Each entry
 * is one 32-bit word.  Most of the bits in the second level entry are used
 * by hardware, and there aren't any "accessed" and "dirty" bits.
 *
 * Linux on the other hand has a three level page table structure, which can
 * be wrapped to fit a two level page table structure easily - using the PGD
 * and PTE only.  However, Linux also expects one "PTE" table per page, and
 * at least a "dirty" bit.
 *
 * Therefore, we tweak the implementation slightly - we tell Linux that we
 * have 2048 entries in the first level, each of which is 8 bytes (iow, two
 * hardware pointers to the second level.)  The second level contains two
 * hardware PTE tables arranged contiguously, preceded by Linux versions
 * which contain the state information Linux needs.  We, therefore, end up
 * with 512 entries in the "PTE" level.
 *
 * This leads to the page tables having the following layout:
 *
 *    pgd             pte
 * |        |
 * +--------+
 * |        |       +------------+ +0
 * +- - - - +       | Linux pt 0 |
 * |        |       +------------+ +1024
 * +--------+ +0    | Linux pt 1 |
 * |        |-----> +------------+ +2048
 * +- - - - + +4    |  h/w pt 0  |
 * |        |-----> +------------+ +3072
 * +--------+ +8    |  h/w pt 1  |
 * |        |       +------------+ +4096
 *
 * See L_PTE_xxx below for definitions of bits in the "Linux pt", and
 * PTE_xxx for definitions of bits appearing in the "h/w pt".
 *
 * PMD_xxx definitions refer to bits in the first level page table.
 *
 * The "dirty" bit is emulated by only granting hardware write permission
 * iff the page is marked "writable" and "dirty" in the Linux PTE.  This
 * means that a write to a clean page will cause a permission fault, and
 * the Linux MM layer will mark the page dirty via handle_pte_fault().
 * For the hardware to notice the permission change, the TLB entry must
 * be flushed, and ptep_set_access_flags() does that for us.
 *
 * The "accessed" or "young" bit is emulated by a similar method; we only
 * allow accesses to the page if the "young" bit is set.  Accesses to the
 * page will cause a fault, and handle_pte_fault() will set the young bit
 * for us as long as the page is marked present in the corresponding Linux
 * PTE entry.  Again, ptep_set_access_flags() will ensure that the TLB is
 * up to date.
 *
 * However, when the "young" bit is cleared, we deny access to the page
 * by clearing the hardware PTE.  Currently Linux does not flush the TLB
 * for us in this case, which means the TLB will retain the transation
 * until either the TLB entry is evicted under pressure, or a context
 * switch which changes the user space mapping occurs.
 */
// 2016-04-16
#define PTRS_PER_PTE		512
#define PTRS_PER_PMD		1
// 2017-08-12
#define PTRS_PER_PGD		2048

// 2016-04-16
#define PTE_HWTABLE_PTRS	(PTRS_PER_PTE)
#define PTE_HWTABLE_OFF		(PTE_HWTABLE_PTRS * sizeof(pte_t)) // 2KB = 2048
#define PTE_HWTABLE_SIZE	(PTRS_PER_PTE * sizeof(u32)) // 2048 = 512 * 4
                                                         // 2KB

/*
 * PMD_SHIFT determines the size of the area a second-level page table can map
 * PGDIR_SHIFT determines what a third-level page table entry can map
 */
#define PMD_SHIFT		21
#define PGDIR_SHIFT		21

#define PMD_SIZE		(1UL << PMD_SHIFT)   // 0x0020_0000, 2MB
#define PMD_MASK		(~(PMD_SIZE-1))      // 0xFFE0_0000 = !0x1F_FFFF
#define PGDIR_SIZE		(1UL << PGDIR_SHIFT) // 0x0020_0000, 2MB 
#define PGDIR_MASK		(~(PGDIR_SIZE-1))    // 0xFFE0_0000 = !0x001F_FFFF

/*
 * section address mask and size definitions.
 */
#define SECTION_SHIFT		20
#define SECTION_SIZE		(1UL << SECTION_SHIFT) // 0x0010_0000, 1MB
#define SECTION_MASK		(~(SECTION_SIZE-1))    // 0xFFF0_0000 = !0x000F_FFFF

/*
 * ARMv6 supersection address mask and size definitions.
 */
#define SUPERSECTION_SHIFT	24
#define SUPERSECTION_SIZE	(1UL << SUPERSECTION_SHIFT)
#define SUPERSECTION_MASK	(~(SUPERSECTION_SIZE-1))

#define USER_PTRS_PER_PGD	(TASK_SIZE/*3056M*/ / PGDIR_SIZE/*2M*/) /*1528 NR*/

/*
 * "Linux" PTE definitions.
 *
 * We keep two sets of PTEs - the hardware and the linux version.
 * This allows greater flexibility in the way we map the Linux bits
 * onto the hardware tables, and allows us to have YOUNG and DIRTY
 * bits.
 *
 * The PTE table pointer refers to the hardware entries; the "Linux"
 * entries are stored 1024 bytes below.
 */
#define L_PTE_VALID		(_AT(pteval_t, 1) << 0)		/* Valid */
// 2015-8-22, (pteval_t)(1) << 0 == 1
#define L_PTE_PRESENT		(_AT(pteval_t, 1) << 0) 
#define L_PTE_YOUNG		(_AT(pteval_t, 1) << 1)
#define L_PTE_FILE		(_AT(pteval_t, 1) << 2)	/* only when !PRESENT */
// 2015-09-05;
#define L_PTE_DIRTY		(_AT(pteval_t, 1) << 6)
// 2015-10-10;
#define L_PTE_RDONLY		(_AT(pteval_t, 1) << 7)
// 2015-10-03
#define L_PTE_USER		(_AT(pteval_t, 1) << 8)
#define L_PTE_XN		(_AT(pteval_t, 1) << 9)
#define L_PTE_SHARED		(_AT(pteval_t, 1) << 10)	/* shared(v6), coherent(xsc3) */
#define L_PTE_NONE		(_AT(pteval_t, 1) << 11)

/*
 * These are the memory types, defined to be compatible with
 * pre-ARMv6 CPUs cacheable and bufferable bits:   XXCB
 */
#define L_PTE_MT_UNCACHED	(_AT(pteval_t, 0x00) << 2)	/* 0000 */
#define L_PTE_MT_BUFFERABLE	(_AT(pteval_t, 0x01) << 2)	/* 0001 */
#define L_PTE_MT_WRITETHROUGH	(_AT(pteval_t, 0x02) << 2)	/* 0010 */
#define L_PTE_MT_WRITEBACK	(_AT(pteval_t, 0x03) << 2)	/* 0011 */
#define L_PTE_MT_MINICACHE	(_AT(pteval_t, 0x06) << 2)	/* 0110 (sa1100, xscale) */
#define L_PTE_MT_WRITEALLOC	(_AT(pteval_t, 0x07) << 2)	/* 0111 */
#define L_PTE_MT_DEV_SHARED	(_AT(pteval_t, 0x04) << 2)	/* 0100 */
#define L_PTE_MT_DEV_NONSHARED	(_AT(pteval_t, 0x0c) << 2)	/* 1100 */
#define L_PTE_MT_DEV_WC		(_AT(pteval_t, 0x09) << 2)	/* 1001 */
#define L_PTE_MT_DEV_CACHED	(_AT(pteval_t, 0x0b) << 2)	/* 1011 */
#define L_PTE_MT_VECTORS	(_AT(pteval_t, 0x0f) << 2)	/* 1111 */
#define L_PTE_MT_MASK		(_AT(pteval_t, 0x0f) << 2)

#ifndef __ASSEMBLY__

/*
 * The "pud_xxx()" functions here are trivial when the pmd is folded into
 * the pud: the pud entry is never bad, always exists, and can't be set or
 * cleared.
 */
// 2015-09-19;
// 2016-12-24
#define pud_none(pud)		(0)
#define pud_bad(pud)		(0)
// 2015-08-22
#define pud_present(pud)	(1)
// 2016-12-24
#define pud_clear(pudp)		do { } while (0)
#define set_pud(pud,pudp)	do { } while (0)

// 2015-08-22, no pud라서 pud는 pgd값이다.
// 2016-12-24
static inline pmd_t *pmd_offset(pud_t *pud, unsigned long addr)
{
	// 전달받은 값을 그대로 type cast 후 :리턴
	return (pmd_t *)pud;
}
// SECTION인가 아닌가를 확인. l1 page table에서는 뒤의 2비트가 0x10로 설정되어 있다.
#define pmd_bad(pmd)		(pmd_val(pmd) & 2)

#define copy_pmd(pmdpd,pmdps)		\
	do {				\
		pmdpd[0] = pmdps[0];	\
		pmdpd[1] = pmdps[1];	\
		flush_pmd_entry(pmdpd);	\
	} while (0)

/*
 * pmd의 단위는 원래 1Mb인데 arm용은 2Mb이라서 배열 0,1을 써서 1Mb를
 * 2Mb로 변환
 *
 * 2014-11-01 추가
 * ARM에서는 PMD를 사용하지 않아 이 함수는 PGD(페이지 디렉토리)의
 * 각 엔트리를 초기화하며 엔트리 2개를 동시에 초기화하여 2MB 단위로
 * 처리되도록 함
 *
 * 초기화후 변환참조버퍼(TLB - Translation Lookaside Buffer)를 클리어
 *  >> 플러시(flush) - 캐시를 바로 비움
 *  >> 클리어(clear) - 더티 비트를 참조하여 메모리에 쓴 후 비움
 * */
#define pmd_clear(pmdp)			\
	do {				\
		pmdp[0] = __pmd(0);	\
		pmdp[1] = __pmd(0);	\
		clean_pmd_entry(pmdp);	\
	} while (0)

/* we don't need complex calculations here as the pmd is folded into the pgd */
#define pmd_addr_end(addr,end) (end)

// 2015-01-31
// set_pte_ext(ptep, pte, 0);
// 2015-08-22, set_pte_ext(ptep, __pte(0), 0)
// 2015-10-03
// 2015-10-10; set_pte_ext(pte, swp_pte, ext)
// 2015-12-26, set_pte_ext(ptep, __pte(0), 0)
// 2016-04-16 set_pte_ext(ptep, pteval, ext)
// 2017-04-14, set_pte_ext(ptep, __pte(0), 0)
#define set_pte_ext(ptep,pte,ext) cpu_set_pte_ext(ptep,pte,ext)

#endif /* __ASSEMBLY__ */

#endif /* _ASM_PGTABLE_2LEVEL_H */
