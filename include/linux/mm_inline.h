#ifndef LINUX_MM_INLINE_H
#define LINUX_MM_INLINE_H

#include <linux/huge_mm.h>
#include <linux/swap.h>

/**
 * page_is_file_cache - should the page be on a file LRU or anon LRU?
 * @page: the page to test
 *
 * Returns 1 if @page is page cache page backed by a regular filesystem,
 * or 0 if @page is anonymous, tmpfs or otherwise ram or swap backed.
 * Used by functions that manipulate the LRU lists, to sort a page
 * onto the right LRU list.
 *
 * We would like to get this info without a page flag, but the state
 * needs to survive until the page is last deleted from the LRU, which
 * could be as far down as __page_cache_release.
 */
// 2015-04-11
// 2015-06-20
// 2015-07-04;
// 2015-10-10;
// 2015-12-05;
// http://www.programering.com/q/MTN5EDNwATg.html
//  PG_swapbacked said a page is to exchange / exchange files as reserve space, 
//  analogous to the page cache page, it is stored in the medium of the file as a reserve space. 
static inline int page_is_file_cache(struct page *page)
{
	return !PageSwapBacked(page);
}
// 2015-04-11
// 2015-06-20
// add_page_to_lru_list(page, lruvec, lru);
// 2015-07-11
// add_page_to_lru_list(page, lruvec, LRU_UNEVICTABLE);
// 2015-10-10
// add_page_to_lru_list(page, lruvec, LRU_UNEVICTABLE);
// 2016-01-09
// add_page_to_lru_list(page, lruvec, lru);
static __always_inline void add_page_to_lru_list(struct page *page,
				struct lruvec *lruvec, enum lru_list lru)
{
    // 1 return
	int nr_pages = hpage_nr_pages(page);
	// NOP
    mem_cgroup_update_lru_size(lruvec, lru, nr_pages);
	// linked list 항목에 추가
    list_add(&page->lru, &lruvec->lists[lru]);
    // vm_stat에 lru type의 index에 +1 시켜준다.
	__mod_zone_page_state(lruvec_zone(lruvec), NR_LRU_BASE + lru, nr_pages);
}

// 2015-04-11
// del_page_from_lru_list(page, lruvec, lru + active);
// __always_inline : inline할 것을 명시해 주는 지시어
// 2015-04-18
// 2015-06-20
// 2015-07-04;
// del_page_from_lru_list(page, lruvec, page_lru(page));
// 2015-07-11
// 2015-10-10;
// del_page_from_lru_list(page, lruvec, lru)
static __always_inline void del_page_from_lru_list(struct page *page,
				struct lruvec *lruvec, enum lru_list lru)
{
    // config 미설정으로 1 리턴
	int nr_pages = hpage_nr_pages(page);
    // config 미설정으로 빈 static함수 호출, NOP
	mem_cgroup_update_lru_size(lruvec, lru, -nr_pages);
	// 링크드리스트에서 자신의 항목 삭제
    list_del(&page->lru);
    // zone내의 페이지 개수 상태 플래그(percpu 내 존재)에 페이지 수만큼 값을 빼줌.
	__mod_zone_page_state(lruvec_zone(lruvec), NR_LRU_BASE + lru, -nr_pages);
}

/**
 * page_lru_base_type - which LRU list type should a page be on?
 * @page: the page to test
 *
 * Used for LRU list index arithmetic.
 *
 * Returns the base LRU type - file or anon - @page should be on.
 */
// 2015-04-18;
// 2015-06-27;
// swapback 인 경우 INACTIVE_ANON, 아닌 경우 INACTIVE_FILE반환
// 2015-10-10;
static inline enum lru_list page_lru_base_type(struct page *page)
{
	if (page_is_file_cache(page))
		return LRU_INACTIVE_FILE; // SwapBacked 비 활성화 
	return LRU_INACTIVE_ANON;
}

/**
 * page_off_lru - which LRU list was page on? clearing its lru flags.
 * @page: the page to test
 *
 * Returns the LRU list a page was on, as an index into the array of LRU
 * lists; and clears its Unevictable or Active flags, ready for freeing.
 */
// 2015-04-18;
static __always_inline enum lru_list page_off_lru(struct page *page)
{
	enum lru_list lru;

    // PG_unevictable 플래그 검사
	if (PageUnevictable(page)) {
        // PG_unevictable 플래그를 클리어
		__ClearPageUnevictable(page);
		lru = LRU_UNEVICTABLE;
	} else {
		lru = page_lru_base_type(page);
		if (PageActive(page)) {
			__ClearPageActive(page);
			lru += LRU_ACTIVE; // 활동상태로 변경
		}
	}
	return lru;
}

/**
 * page_lru - which LRU list should a page be on?
 * @page: the page to test
 *
 * Returns the LRU list a page should be on, as an index
 * into the array of LRU lists.
 */
// 2015-10-10;
// 2016-01-09
static __always_inline enum lru_list page_lru(struct page *page)
{
	enum lru_list lru;

	if (PageUnevictable(page))
		lru = LRU_UNEVICTABLE;
	else {
		lru = page_lru_base_type(page);
		if (PageActive(page))
			lru += LRU_ACTIVE;
	}
	return lru;
}

#endif
