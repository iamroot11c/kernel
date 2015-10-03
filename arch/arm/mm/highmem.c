/*
 * arch/arm/mm/highmem.c -- ARM highmem support
 *
 * Author:	Nicolas Pitre
 * Created:	september 8, 2008
 * Copyright:	Marvell Semiconductors Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/highmem.h>
#include <linux/interrupt.h>
#include <asm/fixmap.h>
#include <asm/cacheflush.h>
#include <asm/tlbflush.h>
#include "mm.h"

void *kmap(struct page *page)
{
	might_sleep();
	if (!PageHighMem(page))
		return page_address(page);
	return kmap_high(page);
}
EXPORT_SYMBOL(kmap);

void kunmap(struct page *page)
{
	BUG_ON(in_interrupt());
	if (!PageHighMem(page))
		return;
	kunmap_high(page);
}
EXPORT_SYMBOL(kunmap);
// 2015-06-06
// 2015-01-24, 시작
// 2015-09-19;
// page *에 대한 가상 주소를 구해준다
void *kmap_atomic(struct page *page)
{
	unsigned int idx;
	unsigned long vaddr;
	void *kmap;
	int type;

	pagefault_disable();
	// highmem이 아닌 경우 4kb단위 align된 주소를 리턴(직접 매핑된 주소)
	if (!PageHighMem(page))
		return page_address(page);

#ifdef CONFIG_DEBUG_HIGHMEM // not define
	/*
	 * There is no cache coherency issue when non VIVT, so force the
	 * dedicated kmap usage for better debugging purposes in that case.
	 */
	// Booting_kernel_exynos5420.log 참고하여 캐시가
	// PIPT / VIPT nonaliasing data cache로 확인
	// 2015-01-24, 여기까지
	if (!cache_is_vivt())
		kmap = NULL;
	else
#endif
		// 2015-01-31, start
		// page주소와 매핑되는 가상 주소를 리턴
		kmap = kmap_high_get(page);
	if (kmap)
		return kmap;

	// 리턴받은 가상 주소가 널 포인터인 경우에 대한 후처리
	type = kmap_atomic_idx_push();

	// idx값 구하는 과정은 의문이다.
	idx = type + KM_TYPE_NR * smp_processor_id();	// KM_TYPE_NR(16)	 
	// ex) fixmap  : 0xfff00000 - 0xfffe0000   ( 896 kB)
	// memory layout에 fixmap영역이 있다.
	// 그곳의 메모리를 할당받는다.
	vaddr = __fix_to_virt(FIX_KMAP_BEGIN/* 0 */ + idx);
#ifdef CONFIG_DEBUG_HIGHMEM // not define
	/*
	 * With debugging enabled, kunmap_atomic forces that entry to 0.
	 * Make sure it was indeed properly unmapped.
	 */
	BUG_ON(!pte_none(get_top_pte(vaddr)));
#endif
	/*
	 * When debugging is off, kunmap_atomic leaves the previous mapping
	 * in place, so the contained TLB flush ensures the TLB is updated
	 * with the new mapping.
	 */
	set_top_pte(vaddr, mk_pte(page, kmap_prot/* PAGE_KERNEL */));

	return (void *)vaddr;
}
EXPORT_SYMBOL(kmap_atomic);
// 2015-06-06
// 2015-01-31
// 2015-02-07, 끝
void __kunmap_atomic(void *kvaddr)
{
	unsigned long vaddr = (unsigned long) kvaddr & PAGE_MASK;
	int idx, type;

	// fixmap 영역이라면,
	if (kvaddr >= (void *)FIXADDR_START) {
		type = kmap_atomic_idx();
		idx = type + KM_TYPE_NR * smp_processor_id();

		if (cache_is_vivt())
			__cpuc_flush_dcache_area((void *)vaddr, PAGE_SIZE);
#ifdef CONFIG_DEBUG_HIGHMEM // not define
		BUG_ON(vaddr != __fix_to_virt(FIX_KMAP_BEGIN + idx));
		set_top_pte(vaddr, __pte(0));
#else
		(void) idx;  /* to kill a warning */
#endif
		kmap_atomic_idx_pop();
	} else if (vaddr >= PKMAP_ADDR(0) && vaddr < PKMAP_ADDR(LAST_PKMAP)) {
		/* this address was obtained through kmap_high_get() */
		kunmap_high(pte_page(pkmap_page_table[PKMAP_NR(vaddr)]));
	}
	pagefault_enable();
}
EXPORT_SYMBOL(__kunmap_atomic);

void *kmap_atomic_pfn(unsigned long pfn)
{
	unsigned long vaddr;
	int idx, type;

	pagefault_disable();

	type = kmap_atomic_idx_push();
	idx = type + KM_TYPE_NR * smp_processor_id();
	vaddr = __fix_to_virt(FIX_KMAP_BEGIN + idx);
#ifdef CONFIG_DEBUG_HIGHMEM
	BUG_ON(!pte_none(get_top_pte(vaddr)));
#endif
	set_top_pte(vaddr, pfn_pte(pfn, kmap_prot));

	return (void *)vaddr;
}

struct page *kmap_atomic_to_page(const void *ptr)
{
	unsigned long vaddr = (unsigned long)ptr;

	if (vaddr < FIXADDR_START)
		return virt_to_page(ptr);

	return pte_page(get_top_pte(vaddr));
}
