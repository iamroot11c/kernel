/*
 *  linux/mm/swap.c
 *
 *  Copyright (C) 1991, 1992, 1993, 1994  Linus Torvalds
 */

/*
 * This file contains the default values for the operation of the
 * Linux VM subsystem. Fine-tuning documentation can be found in
 * Documentation/sysctl/vm.txt.
 * Started 18.12.91
 * Swap aging added 23.2.95, Stephen Tweedie.
 * Buffermem limits added 12.3.98, Rik van Riel.
 */

#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/kernel_stat.h>
#include <linux/swap.h>
#include <linux/mman.h>
#include <linux/pagemap.h>
#include <linux/pagevec.h>
#include <linux/init.h>
#include <linux/export.h>
#include <linux/mm_inline.h>
#include <linux/percpu_counter.h>
#include <linux/percpu.h>
#include <linux/cpu.h>
#include <linux/notifier.h>
#include <linux/backing-dev.h>
#include <linux/memcontrol.h>
#include <linux/gfp.h>
#include <linux/uio.h>
#include <linux/hugetlb.h>

#include "internal.h"

#define CREATE_TRACE_POINTS
#include <trace/events/pagemap.h>

/* How many pages do we try to swap or page in/out together? */
int page_cluster;

// 2015-07-11
static DEFINE_PER_CPU(struct pagevec, lru_add_pvec);
static DEFINE_PER_CPU(struct pagevec, lru_rotate_pvecs);
static DEFINE_PER_CPU(struct pagevec, lru_deactivate_pvecs);

/*
 * This path almost never happens for VM activity - pages are normally
 * freed via pagevecs.  But it gets used by networking.
 */
// 2015-04-18;
// 2015-07-11
// page를 lru list에서 제거하는 기능 from zone->lruvec
// page는 lru page여야하는 필요조건이 성립
static void __page_cache_release(struct page *page)
{
	// page가 기본적으로 LRU 페이지여야 한다.
	if (PageLRU(page)) {
		struct zone *zone = page_zone(page);
		struct lruvec *lruvec;
		unsigned long flags;

		spin_lock_irqsave(&zone->lru_lock, flags);
		// zone->lruvec
		lruvec = mem_cgroup_page_lruvec(page, zone);
		VM_BUG_ON(!PageLRU(page));
		__ClearPageLRU(page);

		del_page_from_lru_list(page, lruvec, page_off_lru(page));
		spin_unlock_irqrestore(&zone->lru_lock, flags);
	}
}

// 2015-04-25
// 2015-07-11
// 버디 할당자에서 order 0인 경우에 관리하는 list에 추가하는 기능
// 2015-08-15
static void __put_single_page(struct page *page)
{
	// page를 LRU list에서 삭제
	__page_cache_release(page);
	// 2015-04-25 start, end
	free_hot_cold_page(page, 0);
}

// 2015-04-18;
// 2015-07-11
static void __put_compound_page(struct page *page)
{
	compound_page_dtor *dtor;

	__page_cache_release(page);
	// (compound_page_dtor *)page[1].lru.next
	// free_compound_page가 할당 될 것으로 예상함
	dtor = get_compound_page_dtor(page);
	(*dtor)(page);
}

// 2015-04-18;
// 2015-07-11
static void put_compound_page(struct page *page)
{
	// page의 flags에서 PG_head_tail_mask가 있는지 확인
	if (unlikely(PageTail(page))) {
		/* __split_huge_page_refcount can run under us */
		// 인자로 주어진 page가 끝이라면 첫 번째 page를 구하며,
		// 마지막이 아니라면 바로 리턴함 
		struct page *page_head = compound_head(page);

		if (likely(page != page_head &&
			   get_page_unless_zero(page_head))) {
			unsigned long flags;

			/*
			 * THP can not break up slab pages so avoid taking
			 * compound_lock().  Slab performs non-atomic bit ops
			 * on page->flags for better performance.  In particular
			 * slab_unlock() in slub used to be a hot path.  It is
			 * still hot on arches that do not support
			 * this_cpu_cmpxchg_double().
			 */
			// CONFIG_HUGETLB_PAGE 미 정의로 PageHeadHuge() 함수는 항상 
			// 0을 리턴
			if (PageSlab(page_head) || PageHeadHuge(page_head)) {
				if (likely(PageTail(page))) {
					/*
					 * __split_huge_page_refcount
					 * cannot race here.
					 */
					VM_BUG_ON(!PageHead(page_head));
					atomic_dec(&page->_mapcount);
					
					// page 구조체의 _count 멤버를 감소한 값이 0이면 
					//  오류 메시지 출력
					if (put_page_testzero(page_head))
						VM_BUG_ON(1);
					if (put_page_testzero(page_head))
						__put_compound_page(page_head);
					return;
				} else
					/*
					 * __split_huge_page_refcount
					 * run before us, "page" was a
					 * THP tail. The split
					 * page_head has been freed
					 * and reallocated as slab or
					 * hugetlbfs page of smaller
					 * order (only possible if
					 * reallocated as slab on
					 * x86).
					 */
					goto skip_lock;
			}
			// 2015-04-18; 여기까지 진행
			
			// 2015-04-25 여기부터 진행
			/*
			 * page_head wasn't a dangling pointer but it
			 * may not be a head page anymore by the time
			 * we obtain the lock. That is ok as long as it
			 * can't be freed from under us.
			 */
			// CONFIG_TRANSPARENT_HUGEPAGE이 비활성화이고, flags 변수는 아마 0인것으로 추측됨.
			flags = compound_lock_irqsave(page_head); 
			if (unlikely(!PageTail(page))) {
				/* __split_huge_page_refcount run before us */
				compound_unlock_irqrestore(page_head, flags);
skip_lock:
				if (put_page_testzero(page_head)) {
					/*
					 * The head page may have been
					 * freed and reallocated as a
					 * compound page of smaller
					 * order and then freed again.
					 * All we know is that it
					 * cannot have become: a THP
					 * page, a compound page of
					 * higher order, a tail page.
					 * That is because we still
					 * hold the refcount of the
					 * split THP tail and
					 * page_head was the THP head
					 * before the split.
					 */
					if (PageHead(page_head))
						__put_compound_page(page_head);
					else
						__put_single_page(page_head);
					// 2015-04-25 식사전
				}
out_put_single:
				if (put_page_testzero(page))
					__put_single_page(page);
				return;
			}
			VM_BUG_ON(page_head != page->first_page);
			/*
			 * We can release the refcount taken by
			 * get_page_unless_zero() now that
			 * __split_huge_page_refcount() is blocked on
			 * the compound_lock.
			 */
			if (put_page_testzero(page_head))
				VM_BUG_ON(1);
			/* __split_huge_page_refcount will wait now */
			VM_BUG_ON(page_mapcount(page) <= 0);
			atomic_dec(&page->_mapcount);
			VM_BUG_ON(atomic_read(&page_head->_count) <= 0);
			VM_BUG_ON(atomic_read(&page->_count) != 0);
			compound_unlock_irqrestore(page_head, flags);

			if (put_page_testzero(page_head)) {
				if (PageHead(page_head))
					__put_compound_page(page_head);
				else
					__put_single_page(page_head);
			}
		} else {
			/* page_head is a dangling pointer */
			VM_BUG_ON(PageTail(page));
			goto out_put_single;
		}
	} else if (put_page_testzero(page)) {
		if (PageHead(page))
			__put_compound_page(page);
		else
			__put_single_page(page);
	}
}

// 2015-07-11
// 2015-08-15
// 2015-10-10;
// 2015-10-17
// 기본적으로 하는일이 없다.
// 그러나, 참조카운터가 0이라면, 0페이지 버디 리스트에 연결
void put_page(struct page *page)
{
	if (unlikely(PageCompound(page)))
		put_compound_page(page);
	else if (put_page_testzero(page))
		__put_single_page(page);
}
EXPORT_SYMBOL(put_page);

/*
 * This function is exported but must not be called by anything other
 * than get_page(). It implements the slow path of get_page().
 */
// 2015-07-11, glance
bool __get_page_tail(struct page *page)
{
	/*
	 * This takes care of get_page() if run on a tail page
	 * returned by one of the get_user_pages/follow_page variants.
	 * get_user_pages/follow_page itself doesn't need the compound
	 * lock because it runs __get_page_tail_foll() under the
	 * proper PT lock that already serializes against
	 * split_huge_page().
	 */
	unsigned long flags;
	bool got = false;
	struct page *page_head = compound_head(page);

	if (likely(page != page_head && get_page_unless_zero(page_head))) {
		/* Ref to put_compound_page() comment. */
		if (PageSlab(page_head) || PageHeadHuge(page_head)) {
			if (likely(PageTail(page))) {
				/*
				 * This is a hugetlbfs page or a slab
				 * page. __split_huge_page_refcount
				 * cannot race here.
				 */
				VM_BUG_ON(!PageHead(page_head));
				__get_page_tail_foll(page, false);
				return true;
			} else {
				/*
				 * __split_huge_page_refcount run
				 * before us, "page" was a THP
				 * tail. The split page_head has been
				 * freed and reallocated as slab or
				 * hugetlbfs page of smaller order
				 * (only possible if reallocated as
				 * slab on x86).
				 */
				put_page(page_head);
				return false;
			}
		}

		/*
		 * page_head wasn't a dangling pointer but it
		 * may not be a head page anymore by the time
		 * we obtain the lock. That is ok as long as it
		 * can't be freed from under us.
		 */
		flags = compound_lock_irqsave(page_head);
		/* here __split_huge_page_refcount won't run anymore */
		if (likely(PageTail(page))) {
			__get_page_tail_foll(page, false);
			got = true;
		}
		compound_unlock_irqrestore(page_head, flags);
		if (unlikely(!got))
			put_page(page_head);
	}
	return got;
}
EXPORT_SYMBOL(__get_page_tail);

/**
 * put_pages_list() - release a list of pages
 * @pages: list of pages threaded on page->lru
 *
 * Release a list of pages which are strung together on page.lru.  Currently
 * used by read_cache_pages() and related error recovery code.
 */
void put_pages_list(struct list_head *pages)
{
	while (!list_empty(pages)) {
		struct page *victim;

		victim = list_entry(pages->prev, struct page, lru);
		list_del(&victim->lru);
		page_cache_release(victim);
	}
}
EXPORT_SYMBOL(put_pages_list);

/*
 * get_kernel_pages() - pin kernel pages in memory
 * @kiov:	An array of struct kvec structures
 * @nr_segs:	number of segments to pin
 * @write:	pinning for read/write, currently ignored
 * @pages:	array that receives pointers to the pages pinned.
 *		Should be at least nr_segs long.
 *
 * Returns number of pages pinned. This may be fewer than the number
 * requested. If nr_pages is 0 or negative, returns 0. If no pages
 * were pinned, returns -errno. Each page returned must be released
 * with a put_page() call when it is finished with.
 */
int get_kernel_pages(const struct kvec *kiov, int nr_segs, int write,
		struct page **pages)
{
	int seg;

	for (seg = 0; seg < nr_segs; seg++) {
		if (WARN_ON(kiov[seg].iov_len != PAGE_SIZE))
			return seg;

		pages[seg] = kmap_to_page(kiov[seg].iov_base);
		page_cache_get(pages[seg]);
	}

	return seg;
}
EXPORT_SYMBOL_GPL(get_kernel_pages);

/*
 * get_kernel_page() - pin a kernel page in memory
 * @start:	starting kernel address
 * @write:	pinning for read/write, currently ignored
 * @pages:	array that receives pointer to the page pinned.
 *		Must be at least nr_segs long.
 *
 * Returns 1 if page is pinned. If the page was not pinned, returns
 * -errno. The page returned must be released with a put_page() call
 * when it is finished with.
 */
int get_kernel_page(unsigned long start, int write, struct page **pages)
{
	const struct kvec kiov = {
		.iov_base = (void *)start,
		.iov_len = PAGE_SIZE
	};

	return get_kernel_pages(&kiov, 1, write, pages);
}
EXPORT_SYMBOL_GPL(get_kernel_page);

// 2015-04-04
// 2015-04-04, 여기까지
// 2015-04-11 시작
// pagevec_lru_move_fn(pvec, lru_deactivate_fn, NULL);
// 2015-06-20
// pagevec_lru_move_fn(pvec, __pagevec_lru_add_fn, NULL);
// 2015-06-20
// pagevec_lru_move_fn(pvec, pagevec_move_tail_fn, &pgmoved);
// pvec 내의 모든 페이지 내 lruvec를 해당 페이지가 속한 zone->lruvec로 옮기고 페이지를 일괄삭제한다.
static void pagevec_lru_move_fn(struct pagevec *pvec,
	void (*move_fn)(struct page *page, struct lruvec *lruvec, void *arg),
	void *arg)
{
	int i;
	struct zone *zone = NULL;
	struct lruvec *lruvec;
	unsigned long flags = 0;
	
	// for (i = 0; i < pvec->nr; i++)
	for (i = 0; i < pagevec_count(pvec); i++) {
		struct page *page = pvec->pages[i];
		struct zone *pagezone = page_zone(page);

		if (pagezone != zone) {
			if (zone)
				spin_unlock_irqrestore(&zone->lru_lock, flags);
			zone = pagezone;
			spin_lock_irqsave(&zone->lru_lock, flags);
		}
		// 현 버전에서는 zone->lruvec를 반환.
		lruvec = mem_cgroup_page_lruvec(page, zone);
		// 이 함수 포인터 내에서 실질적으로 lruvec 값이 이동된다.
		(*move_fn)(page, lruvec, arg);
		// 2015-04-11 여기까지.
		// 함수포인터로 전달받은 lru_deactivate_fn동작 살펴봄.
	}
	if (zone)
		spin_unlock_irqrestore(&zone->lru_lock, flags);

	// 2015-04-18 시작
	release_pages(pvec->pages, pvec->nr, pvec->cold);
	// 2015-04-25 end
	pagevec_reinit(pvec);
}

// 2015-06-20
// 2015-06-27
// zone->lruvec
// (*move_fn)(page, lruvec, arg);
// 자식이 속한 lruvec->lists에서 가장 마지막으로 이동하는 기능
static void pagevec_move_tail_fn(struct page *page, struct lruvec *lruvec,
				 void *arg)
{
	int *pgmoved = arg;

	if (PageLRU(page) && !PageActive(page) && !PageUnevictable(page)) {
		// LRU 상태이면서 페이지 비활성화, 꺼내기 가능한 경우
		// swapback상태 여부 확인, 이 값에 따라 관리되는 컨테이너가 다르다.
		enum lru_list lru = page_lru_base_type(page);
		// 파라미터로 넘어온 lruvec의 리스트에 page->lru값을 마지막 항목에 넣는다. 
		list_move_tail(&page->lru, &lruvec->lists[lru]);
		(*pgmoved)++;
	}
}

/*
 * pagevec_move_tail() must be called with IRQ disabled.
 * Otherwise this may cause nasty races.
 */
// 2015-06-20
// 2015-10-24
static void pagevec_move_tail(struct pagevec *pvec)
{
	int pgmoved = 0;

	pagevec_lru_move_fn(pvec, pagevec_move_tail_fn, &pgmoved);
	// vm_event_state에 더한다.
	__count_vm_events(PGROTATED, pgmoved);
}

/*
 * Writeback is about to end against a page which has been marked for immediate
 * reclaim.  If it still appears to be reclaimable, move it to the tail of the
 * inactive list.
 */
// 2015-10-24;
void rotate_reclaimable_page(struct page *page)
{
	if (!PageLocked(page) && !PageDirty(page) && !PageActive(page) &&
	    !PageUnevictable(page) && PageLRU(page)) {
		struct pagevec *pvec;
		unsigned long flags;

		page_cache_get(page);
		local_irq_save(flags);
		pvec = &__get_cpu_var(lru_rotate_pvecs);
		if (!pagevec_add(pvec, page))
			// pagevec에 공간이 없을 때
			pagevec_move_tail(pvec);
		local_irq_restore(flags);
	}
}
// 2015-04-11
// update_page_reclaim_stat(lruvec, file, 0);
// 2015-04-25
// update_page_reclaim_stat(lruvec, file, 1);
// 2015-06-20 - page로부터 추출한 정보를 기초로
// update_page_reclaim_stat(lruvec, file, active);
static void update_page_reclaim_stat(struct lruvec *lruvec,
				     int file, int rotated)
{
	struct zone_reclaim_stat *reclaim_stat = &lruvec->reclaim_stat;
	
	// index로 사용한 file은 동일하다.
	reclaim_stat->recent_scanned[file]++;
	if (rotated)
		reclaim_stat->recent_rotated[file]++;
}

// 2015-04-25
// 2015-06-20
// 2015-06-27
static void __activate_page(struct page *page, struct lruvec *lruvec,
			    void *arg)
{
	if (PageLRU(page) && !PageActive(page) && !PageUnevictable(page)) {
		int file = page_is_file_cache(page);
		int lru = page_lru_base_type(page);

		del_page_from_lru_list(page, lruvec, lru);
		SetPageActive(page);
		lru += LRU_ACTIVE;	// 중요, active상태 변환
		add_page_to_lru_list(page, lruvec, lru);
		trace_mm_lru_activate(page, page_to_pfn(page));		// debug

		// 전역 vm_event_state 업데이트
		__count_vm_event(PGACTIVATE);
		update_page_reclaim_stat(lruvec, file, 1);
	}
}

#ifdef CONFIG_SMP
static DEFINE_PER_CPU(struct pagevec, activate_page_pvecs);

// 2015-04-25
// 2015-06-20
// activate_page_pvecs에 있는 리스트 중 자신의 cpu에 해당하는 페이지에 대해
// 해당 페이지가 속한 존의 lruvec에 해당 페이지의 lru를 옮긴 후 
// 엑티브 플래그 세팅 후 페이지를 삭제 시켜준다.
// 2015-06-27
static void activate_page_drain(int cpu)
{
	struct pagevec *pvec = &per_cpu(activate_page_pvecs, cpu);

	if (pagevec_count(pvec))
		pagevec_lru_move_fn(pvec, __activate_page, NULL);
}

static bool need_activate_page_drain(int cpu)
{
	return pagevec_count(&per_cpu(activate_page_pvecs, cpu)) != 0;
}

void activate_page(struct page *page)
{
	if (PageLRU(page) && !PageActive(page) && !PageUnevictable(page)) {
		struct pagevec *pvec = &get_cpu_var(activate_page_pvecs);

		page_cache_get(page);
		if (!pagevec_add(pvec, page))
			pagevec_lru_move_fn(pvec, __activate_page, NULL);
		put_cpu_var(activate_page_pvecs);
	}
}

#else
static inline void activate_page_drain(int cpu)
{
}

static bool need_activate_page_drain(int cpu)
{
	return false;
}

void activate_page(struct page *page)
{
	struct zone *zone = page_zone(page);

	spin_lock_irq(&zone->lru_lock);
	__activate_page(page, mem_cgroup_page_lruvec(page, zone), NULL);
	spin_unlock_irq(&zone->lru_lock);
}
#endif

static void __lru_cache_activate_page(struct page *page)
{
	struct pagevec *pvec = &get_cpu_var(lru_add_pvec);
	int i;

	/*
	 * Search backwards on the optimistic assumption that the page being
	 * activated has just been added to this pagevec. Note that only
	 * the local pagevec is examined as a !PageLRU page could be in the
	 * process of being released, reclaimed, migrated or on a remote
	 * pagevec that is currently being drained. Furthermore, marking
	 * a remote pagevec's page PageActive potentially hits a race where
	 * a page is marked PageActive just after it is added to the inactive
	 * list causing accounting errors and BUG_ON checks to trigger.
	 */
	for (i = pagevec_count(pvec) - 1; i >= 0; i--) {
		struct page *pagevec_page = pvec->pages[i];

		if (pagevec_page == page) {
			SetPageActive(page);
			break;
		}
	}

	put_cpu_var(lru_add_pvec);
}

/*
 * Mark a page as having seen activity.
 *
 * inactive,unreferenced	->	inactive,referenced
 * inactive,referenced		->	active,unreferenced
 * active,unreferenced		->	active,referenced
 */
void mark_page_accessed(struct page *page)
{
	if (!PageActive(page) && !PageUnevictable(page) &&
			PageReferenced(page)) {

		/*
		 * If the page is on the LRU, queue it for activation via
		 * activate_page_pvecs. Otherwise, assume the page is on a
		 * pagevec, mark it active and it'll be moved to the active
		 * LRU on the next drain.
		 */
		if (PageLRU(page))
			activate_page(page);
		else
			__lru_cache_activate_page(page);
		ClearPageReferenced(page);
	} else if (!PageReferenced(page)) {
		SetPageReferenced(page);
	}
}
EXPORT_SYMBOL(mark_page_accessed);

/*
 * Queue the page for addition to the LRU via pagevec. The decision on whether
 * to add the page to the [in]active [file|anon] list is deferred until the
 * pagevec is drained. This gives a chance for the caller of __lru_cache_add()
 * have the page added to the active list using mark_page_accessed().
 */
// 2015-07-11
// lru_add_pvec에 page를 추가하는 기능
// 2015-10-10;
void __lru_cache_add(struct page *page)
{
	struct pagevec *pvec = &get_cpu_var(lru_add_pvec);

	page_cache_get(page);
	// 공간이 없는 경우, pvec의 page들을 zone의 vec으로 옮긴다.
	if (!pagevec_space(pvec))
		__pagevec_lru_add(pvec);
	pagevec_add(pvec, page);
	put_cpu_var(lru_add_pvec);
}
EXPORT_SYMBOL(__lru_cache_add);

/**
 * lru_cache_add - add a page to a page list
 * @page: the page to be added to the LRU.
 */
// 2015-07-11
// 2015-10-10;
void lru_cache_add(struct page *page)
{
	VM_BUG_ON(PageActive(page) && PageUnevictable(page));
	VM_BUG_ON(PageLRU(page));
	__lru_cache_add(page);
}

/**
 * add_page_to_unevictable_list - add a page to the unevictable list
 * @page:  the page to be added to the unevictable list
 *
 * Add page directly to its zone's unevictable list.  To avoid races with
 * tasks that might be making the page evictable, through eg. munlock,
 * munmap or exit, while it's not on the lru, we want to add the page
 * while it's locked or otherwise "invisible" to other tasks.  This is
 * difficult to do when using the pagevec cache, so bypass that.
 */
// 2015-07-11
// page를 zone->lruvec의 LRU_UNEVICTABLE에 추가함
// 추가하려는 page는 not Active, unevictable, LRU 형태이다.
void add_page_to_unevictable_list(struct page *page)
{
	struct zone *zone = page_zone(page);
	struct lruvec *lruvec;

	spin_lock_irq(&zone->lru_lock);
	// zone->lruvec을 리턴
	lruvec = mem_cgroup_page_lruvec(page, zone); 
	ClearPageActive(page);
	SetPageUnevictable(page);
	SetPageLRU(page);
	add_page_to_lru_list(page, lruvec, LRU_UNEVICTABLE);
	spin_unlock_irq(&zone->lru_lock);
}

/*
 * If the page can not be invalidated, it is moved to the
 * inactive list to speed up its reclaim.  It is moved to the
 * head of the list, rather than the tail, to give the flusher
 * threads some time to write it out, as this is much more
 * effective than the single-page writeout from reclaim.
 *
 * If the page isn't page_mapped and dirty/writeback, the page
 * could reclaim asap using PG_reclaim.
 *
 * 1. active, mapped page -> none
 * 2. active, dirty/writeback page -> inactive, head, PG_reclaim
 * 3. inactive, mapped page -> none
 * 4. inactive, dirty/writeback page -> inactive, head, PG_reclaim
 * 5. inactive, clean -> inactive, tail
 * 6. Others -> none
 *
 * In 4, why it moves inactive's head, the VM expects the page would
 * be write it out by flusher threads as this is much more effective
 * than the single-page writeout from reclaim.
 */
// 2015-04-11
// 2015-06-20
static void lru_deactivate_fn(struct page *page, struct lruvec *lruvec,
			      void *arg)
{
	int lru, file;
	bool active;
	// PG_LRU플래그 설정 여부 확인
	if (!PageLRU(page))
		return;
	// PG_Unevictable 플래그 설정 여부 확인 	
	if (PageUnevictable(page))
		return;

	/* Some processes are using the page */
	// 페이지 테이블에 참조 중인 것으로 판단.`
	if (page_mapped(page))
		return;
	
	// PG_Active 플래그 설정 여부 확인
	active = PageActive(page);
	// PG_SwapBacked플래그 설정 여부 확인
	// 참인 경우 파일, 아닌 경우 익명의 페이지(메모리 상주, 임시 페이지 ...)
	file = page_is_file_cache(page);
	// 파일 타입인지 아닌지 검사
	lru = page_lru_base_type(page);
	// 페이지 링크드 리스트 내에서 제거 및 존 내 플래그 재세팅
	del_page_from_lru_list(page, lruvec, lru + active);
	// active flag 꺼줌
	ClearPageActive(page);
	// referenced flag 꺼줌
	ClearPageReferenced(page);
	// 페이지 링크드 리스트 내에 추가 및 zone 플래그 재세팅
	add_page_to_lru_list(page, lruvec, lru);

	if (PageWriteback(page) || PageDirty(page)) {
		/*
		 * PG_reclaim could be raced with end_page_writeback
		 * It can make readahead confusing.  But race window
		 * is _really_ small and  it's non-critical problem.
		 */
		// readahead 관련 링크 : https://en.wikipedia.org/wiki/Readahead
		SetPageReclaim(page);
	} else {
		/*
		 * The page's writeback ends up during pagevec
		 * We moves tha page into tail of inactive.
		 */
		list_move_tail(&page->lru, &lruvec->lists[lru]);
		// 전역, zone의 vm state가 아니다.
		// vm_event[PGROTATED] += 1;
		__count_vm_event(PGROTATED);
	}

	if (active)
		// vm_event[PGDEACTIVATE] += 1;
		__count_vm_event(PGDEACTIVATE);
	update_page_reclaim_stat(lruvec, file, 0);
}

/*
 * Drain pages out of the cpu's pagevecs.
 * Either "cpu" is the current CPU, and preemption has already been
 * disabled; or "cpu" is being hot-unplugged, and is already dead.
 */
// 2015-04-04
// 2015-06-20
// 2015-06-27
// 전역변수에 저장된 페이지의 lru 정보를 일괄적으로 해당 페이지가 속한 존으로 옮긴 후
// 저장된 페이지를 일괄삭제한다. 이에 대한 후처리는 함수포인터를 통해 처리된다.
void lru_add_drain_cpu(int cpu)
{
	// lru_add_pvec은 전역변수 형태로 저장되어 있는 링크드리스트이다.
	struct pagevec *pvec = &per_cpu(lru_add_pvec, cpu);

	if (pagevec_count(pvec))
		__pagevec_lru_add(pvec);

	pvec = &per_cpu(lru_rotate_pvecs, cpu);
	if (pagevec_count(pvec)) {
		// lru_rotate_pvecs내에 1개 이상의 페이지가 존재하는 경우
		// 동기화를 건 후 pvec의 제일 마지막 리스트에 각 페이지의 lru리스트를 삽입한다.
		unsigned long flags;

		/* No harm done if a racing interrupt already did this */
		local_irq_save(flags);
		pagevec_move_tail(pvec);
		local_irq_restore(flags);
	}

	pvec = &per_cpu(lru_deactivate_pvecs, cpu);
	if (pagevec_count(pvec))
		pagevec_lru_move_fn(pvec, lru_deactivate_fn, NULL);
	// 2015-04-25

	// 2015-04-25
	activate_page_drain(cpu);
	// 2015-04-25
	// 2015-06-20
	// 2015-06-27
}

/**
 * deactivate_page - forcefully deactivate a page
 * @page: page to deactivate
 *
 * This function hints the VM that @page is a good reclaim candidate,
 * for example if its invalidation fails due to the page being dirty
 * or under writeback.
 */
void deactivate_page(struct page *page)
{
	/*
	 * In a workload with many unevictable page such as mprotect, unevictable
	 * page deactivation for accelerating reclaim is pointless.
	 */
	if (PageUnevictable(page))
		return;

	if (likely(get_page_unless_zero(page))) {
		struct pagevec *pvec = &get_cpu_var(lru_deactivate_pvecs);

		if (!pagevec_add(pvec, page))
			pagevec_lru_move_fn(pvec, lru_deactivate_fn, NULL);
		put_cpu_var(lru_deactivate_pvecs);
	}
}

// 2015-06-20
void lru_add_drain(void)
{
	lru_add_drain_cpu(get_cpu());
	put_cpu();	// preempt_enble();
}

static void lru_add_drain_per_cpu(struct work_struct *dummy)
{
	lru_add_drain();
}

static DEFINE_PER_CPU(struct work_struct, lru_add_drain_work);

void lru_add_drain_all(void)
{
	static DEFINE_MUTEX(lock);
	static struct cpumask has_work;
	int cpu;

	mutex_lock(&lock);
	get_online_cpus();
	cpumask_clear(&has_work);

	for_each_online_cpu(cpu) {
		struct work_struct *work = &per_cpu(lru_add_drain_work, cpu);

		if (pagevec_count(&per_cpu(lru_add_pvec, cpu)) ||
		    pagevec_count(&per_cpu(lru_rotate_pvecs, cpu)) ||
		    pagevec_count(&per_cpu(lru_deactivate_pvecs, cpu)) ||
		    need_activate_page_drain(cpu)) {
			INIT_WORK(work, lru_add_drain_per_cpu);
			schedule_work_on(cpu, work);
			cpumask_set_cpu(cpu, &has_work);
		}
	}

	for_each_cpu(cpu, &has_work)
		flush_work(&per_cpu(lru_add_drain_work, cpu));

	put_online_cpus();
	mutex_unlock(&lock);
}

/*
 * Batched page_cache_release().  Decrement the reference count on all the
 * passed pages.  If it fell to zero then remove the page from the LRU and
 * free it.
 *
 * Avoid taking zone->lru_lock if possible, but if it is taken, retain it
 * for the remainder of the operation.
 *
 * The locking in this function is against shrink_inactive_list(): we recheck
 * the page count inside the lock to see whether shrink_inactive_list()
 * grabbed the page via the LRU.  If it did, give up: shrink_inactive_list()
 * will free it.
 */
// 2015-04-18; 시작
// release_pages(pvec->pages, pvec->nr, pvec->cold);
void release_pages(struct page **pages, int nr, int cold)
{
	int i;
	// 헤더를 만듬
	LIST_HEAD(pages_to_free);
	struct zone *zone = NULL;
	struct lruvec *lruvec;
	unsigned long uninitialized_var(flags);

	for (i = 0; i < nr; i++) {
		struct page *page = pages[i];

		// page의 flags 멤버에 pageflags.PG_compound이 셋되어있는지 검사
		if (unlikely(PageCompound(page))) {
			// pageflags.PG_compound가 셋되어 있음
			if (zone) {
				// 첫 번째 루프에서는 zone이 null이어서 
				// 검사하지 않음
				spin_unlock_irqrestore(&zone->lru_lock, flags);
				zone = NULL;
			}
			// 2015-04-18 start
			put_compound_page(page);
			// 2015-04-25 end
			continue;
		}

		// release_pages()로 진입했다면, 기본 ref count는 감소될 것이다.
		if (!put_page_testzero(page))
			continue;

		if (PageLRU(page)) {
			struct zone *pagezone = page_zone(page);

			if (pagezone != zone) {
				if (zone)
					spin_unlock_irqrestore(&zone->lru_lock,
									flags);
				zone = pagezone;
				spin_lock_irqsave(&zone->lru_lock, flags);
			}

			lruvec = mem_cgroup_page_lruvec(page, zone);
			VM_BUG_ON(!PageLRU(page));
			__ClearPageLRU(page);
			del_page_from_lru_list(page, lruvec, page_off_lru(page));
		}

		/* Clear Active bit in case of parallel mark_page_accessed */
		ClearPageActive(page);

		list_add(&page->lru, &pages_to_free);
	}
	if (zone)
		spin_unlock_irqrestore(&zone->lru_lock, flags);

	free_hot_cold_page_list(&pages_to_free, cold);
}
EXPORT_SYMBOL(release_pages);

/*
 * The pages which we're about to release may be in the deferred lru-addition
 * queues.  That would prevent them from really being freed right now.  That's
 * OK from a correctness point of view but is inefficient - those pages may be
 * cache-warm and we want to give them back to the page allocator ASAP.
 *
 * So __pagevec_release() will drain those queues here.  __pagevec_lru_add()
 * and __pagevec_lru_add_active() call release_pages() directly to avoid
 * mutual recursion.
 */
void __pagevec_release(struct pagevec *pvec)
{
	lru_add_drain();
	release_pages(pvec->pages, pagevec_count(pvec), pvec->cold);
	pagevec_reinit(pvec);
}
EXPORT_SYMBOL(__pagevec_release);

#ifdef CONFIG_TRANSPARENT_HUGEPAGE
/* used by __split_huge_page_refcount() */
void lru_add_page_tail(struct page *page, struct page *page_tail,
		       struct lruvec *lruvec, struct list_head *list)
{
	const int file = 0;

	VM_BUG_ON(!PageHead(page));
	VM_BUG_ON(PageCompound(page_tail));
	VM_BUG_ON(PageLRU(page_tail));
	VM_BUG_ON(NR_CPUS != 1 &&
		  !spin_is_locked(&lruvec_zone(lruvec)->lru_lock));

	if (!list)
		SetPageLRU(page_tail);

	if (likely(PageLRU(page)))
		list_add_tail(&page_tail->lru, &page->lru);
	else if (list) {
		/* page reclaim is reclaiming a huge page */
		get_page(page_tail);
		list_add_tail(&page_tail->lru, list);
	} else {
		struct list_head *list_head;
		/*
		 * Head page has not yet been counted, as an hpage,
		 * so we must account for each subpage individually.
		 *
		 * Use the standard add function to put page_tail on the list,
		 * but then correct its position so they all end up in order.
		 */
		add_page_to_lru_list(page_tail, lruvec, page_lru(page_tail));
		list_head = page_tail->lru.prev;
		list_move_tail(&page_tail->lru, list_head);
	}

	if (!PageUnevictable(page))
		update_page_reclaim_stat(lruvec, file, PageActive(page_tail));
}
#endif /* CONFIG_TRANSPARENT_HUGEPAGE */

// 2015-06-20
// lruvec = zone->lruvec
// (*move_fn)(page, lruvec, NULL);
// page->lruvec를 파라미터로 넘어온 lruvec 링크드 리스트에 연결하고 연결 상태를 업데이트->
static void __pagevec_lru_add_fn(struct page *page, struct lruvec *lruvec,
				 void *arg)
{
	int file = page_is_file_cache(page);
	int active = PageActive(page);
	enum lru_list lru = page_lru(page);

	VM_BUG_ON(PageLRU(page));

	SetPageLRU(page);
	// lruvec에 page->lru를 추가
	add_page_to_lru_list(page, lruvec, lru);
	update_page_reclaim_stat(lruvec, file, active);
	trace_mm_lru_insertion(page, page_to_pfn(page), lru, trace_pagemap_flags(page));
}

/*
 * Add the passed pages to the LRU, then drop the caller's refcount
 * on them.  Reinitialises the caller's pagevec.
 */
// 2015-04-04
// 2015-06-20
// pvec 내의 모든 페이지에 대해 해당 페이지가 속한 존의 lruvec로 이동 후 페이지 삭제
// 2015-07-11
// pagevec_lru_move_fn에서 pvec의 page들을 해제하고 초기화하는 기능을 담당한다.
void __pagevec_lru_add(struct pagevec *pvec)
{
	pagevec_lru_move_fn(pvec, __pagevec_lru_add_fn, NULL);
}
EXPORT_SYMBOL(__pagevec_lru_add);

/**
 * pagevec_lookup - gang pagecache lookup
 * @pvec:	Where the resulting pages are placed
 * @mapping:	The address_space to search
 * @start:	The starting page index
 * @nr_pages:	The maximum number of pages
 *
 * pagevec_lookup() will search for and return a group of up to @nr_pages pages
 * in the mapping.  The pages are placed in @pvec.  pagevec_lookup() takes a
 * reference against the pages in @pvec.
 *
 * The search returns a group of mapping-contiguous pages with ascending
 * indexes.  There may be holes in the indices due to not-present pages.
 *
 * pagevec_lookup() returns the number of pages which were found.
 */
unsigned pagevec_lookup(struct pagevec *pvec, struct address_space *mapping,
		pgoff_t start, unsigned nr_pages)
{
	pvec->nr = find_get_pages(mapping, start, nr_pages, pvec->pages);
	return pagevec_count(pvec);
}
EXPORT_SYMBOL(pagevec_lookup);

unsigned pagevec_lookup_tag(struct pagevec *pvec, struct address_space *mapping,
		pgoff_t *index, int tag, unsigned nr_pages)
{
	pvec->nr = find_get_pages_tag(mapping, index, tag,
					nr_pages, pvec->pages);
	return pagevec_count(pvec);
}
EXPORT_SYMBOL(pagevec_lookup_tag);

/*
 * Perform any setup for the swap system
 */
void __init swap_setup(void)
{
	unsigned long megs = totalram_pages >> (20 - PAGE_SHIFT);
#ifdef CONFIG_SWAP
	int i;

	bdi_init(swapper_spaces[0].backing_dev_info);
	for (i = 0; i < MAX_SWAPFILES; i++) {
		spin_lock_init(&swapper_spaces[i].tree_lock);
		INIT_LIST_HEAD(&swapper_spaces[i].i_mmap_nonlinear);
	}
#endif

	/* Use a smaller cluster for small-memory machines */
	if (megs < 16)
		page_cluster = 2;
	else
		page_cluster = 3;
	/*
	 * Right now other parts of the system means that we
	 * _really_ don't want to cluster much more
	 */
}
