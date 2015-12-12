#ifndef _LINUX_SWAPOPS_H
#define _LINUX_SWAPOPS_H

#include <linux/radix-tree.h>
#include <linux/bug.h>

/*
 * swapcache pages are stored in the swapper_space radix tree.  We want to
 * get good packing density in that tree, so the index should be dense in
 * the low-order bits.
 *
 * We arrange the `type' and `offset' fields so that `type' is at the seven
 * high-order bits of the swp_entry_t and `offset' is right-aligned in the
 * remaining bits.  Although `type' itself needs only five bits, we allow for
 * shmem/tmpfs to shift it all up a further two bits: see swp_to_radix_entry().
 *
 * swp_entry_t's are *never* stored anywhere in their arch-dependent format.
 */
// 2015-07-04;
// 2015-10-03
#define SWP_TYPE_SHIFT(e)	((sizeof(e.val) * 8) - \
			(MAX_SWAPFILES_SHIFT/*5*/ + RADIX_TREE_EXCEPTIONAL_SHIFT/*2*/)) // 25
// 2015-09-19;
#define SWP_OFFSET_MASK(e)	((1UL << SWP_TYPE_SHIFT(e)/*25*/) - 1) // 0x01FF_FFFF

/*
 * Store a type+offset into a swp_entry_t in an arch-independent format
 */
// 2015-10-10;
// swp_entry(write ? SWP_MIGRATION_WRITE : SWP_MIGRATION_READ,
//                                                  page_to_pfn(page));
//
// 2015-10-24;
// swp_entry(__swp_type(arch_entry), __swp_offset(arch_entry))
//
// 2015-12-12
// swp_entry(si->type, offset);
// type : 2, offset : 7
// val = 0x0400_0000 | 0x07
static inline swp_entry_t swp_entry(unsigned long type, pgoff_t offset)
{
	swp_entry_t ret;

	ret.val = (type << SWP_TYPE_SHIFT(ret)/*25*/) |
			(offset & SWP_OFFSET_MASK(ret));
	return ret;
}

/*
 * Extract the `type' field from a swp_entry_t.  The swp_entry_t is in
 * arch-independent format
 */
// 2015-07-04;
// 2015-10-03
static inline unsigned swp_type(swp_entry_t entry)
{
	return (entry.val >> SWP_TYPE_SHIFT(entry)/*25*/);
}

/*
 * Extract the `offset' field from a swp_entry_t.  The swp_entry_t is in
 * arch-independent format
 */
// 2015-09-19;
// 2015-10-03
static inline pgoff_t swp_offset(swp_entry_t entry)
{
    // entry.val & ((1UL<<25)-1);
	return entry.val & SWP_OFFSET_MASK(entry);
}

#ifdef CONFIG_MMU // defined
/* check whether a pte points to a swap entry */
static inline int is_swap_pte(pte_t pte)
{
	return !pte_none(pte) && !pte_present(pte) && !pte_file(pte);
}
#endif

/*
 * Convert the arch-dependent pte representation of a swp_entry_t into an
 * arch-independent swp_entry_t.
 */
// 2015-10-24;
static inline swp_entry_t pte_to_swp_entry(pte_t pte)
{
	swp_entry_t arch_entry;

	BUG_ON(pte_file(pte));
	if (pte_swp_soft_dirty(pte))
		pte = pte_swp_clear_soft_dirty(pte);
	arch_entry = __pte_to_swp_entry(pte); // pte를 가지고 
                                          // swp_entry_t 인스턴스를 생성
	return swp_entry(__swp_type(arch_entry), __swp_offset(arch_entry));
}

/*
 * Convert the arch-independent representation of a swp_entry_t into the
 * arch-dependent pte representation.
 */
// 2015-10-10;
// swp_entry_to_pte(entry);
static inline pte_t swp_entry_to_pte(swp_entry_t entry)
{
	swp_entry_t arch_entry;

    // ((swp_type(entry)) << __SWP_TYPE_SHIFT/*3*/) |
    //              ((swp_offset(entry)) << __SWP_OFFSET_SHIFT/*8*/)
	arch_entry = __swp_entry(swp_type(entry), swp_offset(entry));
    // 파일이면 오류 출력
	BUG_ON(pte_file(__swp_entry_to_pte(arch_entry)));
    // swp_entry_t 구조체의 val 값을 리턴
	return __swp_entry_to_pte(arch_entry);
}

static inline swp_entry_t radix_to_swp_entry(void *arg)
{
	swp_entry_t entry;

	entry.val = (unsigned long)arg >> RADIX_TREE_EXCEPTIONAL_SHIFT;
	return entry;
}

static inline void *swp_to_radix_entry(swp_entry_t entry)
{
	unsigned long value;

	value = entry.val << RADIX_TREE_EXCEPTIONAL_SHIFT;
	return (void *)(value | RADIX_TREE_EXCEPTIONAL_ENTRY);
}

#ifdef CONFIG_MIGRATION // defined
// 2015-10-10;
// make_migration_entry(page, pte_write(pteval));
static inline swp_entry_t make_migration_entry(struct page *page, int write)
{
	BUG_ON(!PageLocked(page));
	return swp_entry(write ? SWP_MIGRATION_WRITE : SWP_MIGRATION_READ,
			page_to_pfn(page));
}

// 2015-10-24;
static inline int is_migration_entry(swp_entry_t entry)
{
	return unlikely(swp_type(entry) == SWP_MIGRATION_READ ||
			swp_type(entry) == SWP_MIGRATION_WRITE);
}

// 2015-10-24;
static inline int is_write_migration_entry(swp_entry_t entry)
{
	return unlikely(swp_type(entry) == SWP_MIGRATION_WRITE);
}

// 2015-10-24;
static inline struct page *migration_entry_to_page(swp_entry_t entry)
{
	struct page *p = pfn_to_page(swp_offset(entry));
	/*
	 * Any use of migration entries may only occur while the
	 * corresponding page is locked
	 */
	BUG_ON(!PageLocked(p));
	return p;
}

static inline void make_migration_entry_read(swp_entry_t *entry)
{
	*entry = swp_entry(SWP_MIGRATION_READ, swp_offset(*entry));
}

extern void migration_entry_wait(struct mm_struct *mm, pmd_t *pmd,
					unsigned long address);
extern void migration_entry_wait_huge(struct mm_struct *mm, pte_t *pte);
#else

#define make_migration_entry(page, write) swp_entry(0, 0)
static inline int is_migration_entry(swp_entry_t swp)
{
	return 0;
}
#define migration_entry_to_page(swp) NULL
static inline void make_migration_entry_read(swp_entry_t *entryp) { }
static inline void migration_entry_wait(struct mm_struct *mm, pmd_t *pmd,
					 unsigned long address) { }
static inline void migration_entry_wait_huge(struct mm_struct *mm,
					pte_t *pte) { }
static inline int is_write_migration_entry(swp_entry_t entry)
{
	return 0;
}

#endif

#ifdef CONFIG_MEMORY_FAILURE
/*
 * Support for hardware poisoned pages
 */
static inline swp_entry_t make_hwpoison_entry(struct page *page)
{
	BUG_ON(!PageLocked(page));
	return swp_entry(SWP_HWPOISON, page_to_pfn(page));
}

static inline int is_hwpoison_entry(swp_entry_t entry)
{
	return swp_type(entry) == SWP_HWPOISON;
}
#else

static inline swp_entry_t make_hwpoison_entry(struct page *page)
{
	return swp_entry(0, 0);
}

static inline int is_hwpoison_entry(swp_entry_t swp)
{
	return 0;
}
#endif

// CONFIG_MEMORY_FAILURE not defined
// CONFIG_MIGRATION defined
#if defined(CONFIG_MEMORY_FAILURE) || defined(CONFIG_MIGRATION)
// 2015-09-19;
static inline int non_swap_entry(swp_entry_t entry)
{
	return swp_type(entry) >= MAX_SWAPFILES/*30*/;
}
#else
static inline int non_swap_entry(swp_entry_t entry)
{
	return 0;
}
#endif

#endif /* _LINUX_SWAPOPS_H */
