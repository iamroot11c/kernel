/*
 * linux/mm/mmzone.c
 *
 * management codes for pgdats, zones and page flags
 */


#include <linux/stddef.h>
#include <linux/mm.h>
#include <linux/mmzone.h>

// 2015-04-25
struct pglist_data *first_online_pgdat(void)
{
	return NODE_DATA(first_online_node);
}

// 2015-04-25
struct pglist_data *next_online_pgdat(struct pglist_data *pgdat)
{
	// nid = MAX_NUMNODES/*1*/;
	// MAX_NUMNODES가 1로 되어있어 next_online_node() 함수는
	// 항상 MAX_NUMNODES을 리턴
	int nid = next_online_node(pgdat->node_id);

	// next_online_node() 함수가 MAX_NUMNODES를 리턴해서
	// 이 함수는 항상 NULL을 리턴할것으로 예상됨
	if (nid == MAX_NUMNODES)
		return NULL;
	// (&contig_page_data)
	return NODE_DATA(nid);
}

/*
 * next_zone - helper magic for for_each_zone()
 */
// 2015-04-25
struct zone *next_zone(struct zone *zone)
{
	pg_data_t *pgdat = zone->zone_pgdat;

	if (zone < pgdat->node_zones + MAX_NR_ZONES/*3*/ - 1)
		zone++;
	else {
		pgdat = next_online_pgdat(pgdat);
		if (pgdat)
			zone = pgdat->node_zones;
		else
			zone = NULL;
	}
	return zone;
}

static inline int zref_in_nodemask(struct zoneref *zref, nodemask_t *nodes)
{
#ifdef CONFIG_NUMA
	return node_isset(zonelist_node_idx(zref), *nodes);
#else
	return 1;
#endif /* CONFIG_NUMA */
}

/* Returns the next zone at or below highest_zoneidx in a zonelist */
// 2015-03-28; nodes가 NULL 일 때는 첫 번째 zoneref를 찾을 것으로 예측됨 
// next_zones_zonelist(zonelist->_zonerefs, offset, null, &zone);
struct zoneref *next_zones_zonelist(struct zoneref *z,
					enum zone_type highest_zoneidx,
					nodemask_t *nodes,
					struct zone **zone)
{
	/*
	 * Find the next suitable zone to use for the allocation.
	 * Only filter based on nodemask if it's set
	 */
	if (likely(nodes == NULL))
		while (zonelist_zone_idx(z) > highest_zoneidx)
			z++;
	else
		while (zonelist_zone_idx(z) > highest_zoneidx ||
				(z->zone && !zref_in_nodemask(z, nodes)))
			z++;

	*zone = zonelist_zone(z); // 목록의 첫 번째를 구할 것으로 예측됨
	return z;
}

#ifdef CONFIG_ARCH_HAS_HOLES_MEMORYMODEL
int memmap_valid_within(unsigned long pfn,
					struct page *page, struct zone *zone)
{
	if (page_to_pfn(page) != pfn)
		return 0;

	if (page_zone(page) != zone)
		return 0;

	return 1;
}
#endif /* CONFIG_ARCH_HAS_HOLES_MEMORYMODEL */

// 2015-01-17
// 구조체 0초기화, list-head 초기화
void lruvec_init(struct lruvec *lruvec)
{
	enum lru_list lru;

	memset(lruvec, 0, sizeof(struct lruvec));

	for_each_lru(lru)
		INIT_LIST_HEAD(&lruvec->lists[lru]);
}

#if defined(CONFIG_NUMA_BALANCING) && !defined(LAST_NID_NOT_IN_PAGE_FLAGS)
int page_nid_xchg_last(struct page *page, int nid)
{
	unsigned long old_flags, flags;
	int last_nid;

	do {
		old_flags = flags = page->flags;
		last_nid = page_nid_last(page);

		flags &= ~(LAST_NID_MASK << LAST_NID_PGSHIFT);
		flags |= (nid & LAST_NID_MASK) << LAST_NID_PGSHIFT;
	} while (unlikely(cmpxchg(&page->flags, old_flags, flags) != old_flags));

	return last_nid;
}
#endif
