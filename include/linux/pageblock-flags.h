/*
 * Macros for manipulating and testing flags related to a
 * pageblock_nr_pages number of pages.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation version 2 of the License
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) IBM Corporation, 2006
 *
 * Original author, Mel Gorman
 * Major cleanups and reduction of bit operations, Andy Whitcroft
 */
#ifndef PAGEBLOCK_FLAGS_H
#define PAGEBLOCK_FLAGS_H

#include <linux/types.h>

/* Bit indices that affect a whole block of pages */
// 2015-07-04;
enum pageblock_bits {
	PB_migrate,
	PB_migrate_end = PB_migrate + 3 - 1, 
			/* 3 bits required for migrate types */
#ifdef CONFIG_COMPACTION // CONFIG_COMPACTION = y 
	PB_migrate_skip,/* If set the block is skipped by compaction */
#endif /* CONFIG_COMPACTION */
	NR_PAGEBLOCK_BITS // 4
};

#ifdef CONFIG_HUGETLB_PAGE  // not set

#ifdef CONFIG_HUGETLB_PAGE_SIZE_VARIABLE // not set

/* Huge page sizes are variable */
extern int pageblock_order;

#else /* CONFIG_HUGETLB_PAGE_SIZE_VARIABLE */

/* Huge pages are a constant size */
#define pageblock_order		HUGETLB_PAGE_ORDER

#endif /* CONFIG_HUGETLB_PAGE_SIZE_VARIABLE */

#else /* CONFIG_HUGETLB_PAGE */

/* If huge pages are not used, group by MAX_ORDER_NR_PAGES */
// 2015-07-04;
#define pageblock_order		(MAX_ORDER-1) // 11 - 1 = 10

#endif /* CONFIG_HUGETLB_PAGE */

#define pageblock_nr_pages	(1UL << pageblock_order) // 1 << 10, 1024

/* Forward declaration */
struct page;

/* Declarations for getting and setting flags. See mm/page_alloc.c */
unsigned long get_pageblock_flags_group(struct page *page,
					int start_bitidx, int end_bitidx);
void set_pageblock_flags_group(struct page *page, unsigned long flags,
					int start_bitidx, int end_bitidx);

#ifdef CONFIG_COMPACTION
// 2015-7-04;
#define get_pageblock_skip(page) \
			get_pageblock_flags_group(page, PB_migrate_skip,     \
							PB_migrate_skip)

// 2015-06-20
#define clear_pageblock_skip(page) \
			set_pageblock_flags_group(page, 0, PB_migrate_skip,  \
							PB_migrate_skip)

// 2015-07-04;
// 2015-07-25;
#define set_pageblock_skip(page) \
			set_pageblock_flags_group(page, 1, PB_migrate_skip,  \
							PB_migrate_skip)
#endif /* CONFIG_COMPACTION */

#endif	/* PAGEBLOCK_FLAGS_H */
