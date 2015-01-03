/*
 * sparse memory mappings.
 */
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/mmzone.h>
#include <linux/bootmem.h>
#include <linux/highmem.h>
#include <linux/export.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include "internal.h"
#include <asm/dma.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>

/*
 * Permanent SPARSEMEM data:
 *
 * 1) mem_section	- memory sections, mem_map's for valid memory
 */
#ifdef CONFIG_SPARSEMEM_EXTREME	// CONFIG_SPARSEMEM_EXTREME=y
struct mem_section *mem_section[NR_SECTION_ROOTS] // NR_SECTION_ROOTS = 1
	____cacheline_internodealigned_in_smp;
#else
struct mem_section mem_section[NR_SECTION_ROOTS][SECTIONS_PER_ROOT]
	____cacheline_internodealigned_in_smp;
#endif
EXPORT_SYMBOL(mem_section);

#ifdef NODE_NOT_IN_PAGE_FLAGS
/*
 * If we did not store the node number in the page then we have to
 * do a lookup in the section_to_node_table in order to find which
 * node the page belongs to.
 */
#if MAX_NUMNODES <= 256
static u8 section_to_node_table[NR_MEM_SECTIONS] __cacheline_aligned;
#else
static u16 section_to_node_table[NR_MEM_SECTIONS] __cacheline_aligned;
#endif

int page_to_nid(const struct page *page)
{
	return section_to_node_table[page_to_section(page)];
}
EXPORT_SYMBOL(page_to_nid);

static void set_section_nid(unsigned long section_nr, int nid)
{
	section_to_node_table[section_nr] = nid;
}
#else /* !NODE_NOT_IN_PAGE_FLAGS */
// 2014-12-27
static inline void set_section_nid(unsigned long section_nr, int nid)
{
}
#endif

#ifdef CONFIG_SPARSEMEM_EXTREME	// CONFIG_SPARSEMEM_EXTREME=y
// 2014-12-13, 여기까지, To do 차주
// 2014-12-20 분석중;
// 2014-12-27 종료; 
static struct mem_section noinline __init_refok *sparse_index_alloc(int nid)
{
	struct mem_section *section = NULL;
	unsigned long array_size = SECTIONS_PER_ROOT *
				   sizeof(struct mem_section);
                                   // 4096 = 512 * sizeof(struct mem_section);

	// 2014-12-20;
	// slab_is_available() 함수에서 slab_state의 상태를 조사하며
	// UP 또는 FULL 일 때만 사용(이용)가능함
	// 
	// slab_state의 상태를 변경은 아래의 두 함수에서 이루어짐
	//  mm/slub.c:void kmem_cache_init()
	//  mm/slab_common.c:void create_kmalloc_caches(int)
	// 
	// kmem_cache_init() 함수에서 'PARTIAL'로 설정 후 
	// create_kmalloc_caches() 함수를 호출하며
	// 이 함수에서 slab_state의 상태를 'UP'으로 변경
	//
	// kmem_cache_init() 함수는 init/main.c mm_init(),
	// mm_init() 함수는 init/main.c start_kernel() 에서 
	// 호출됨
	//
	// 그런데 아직 init/main.c에서 mm_init() 함수를 호출하기 전이므로 
	// slab_state의 상태가 확정되지 않아 조건을 만족하지 못함
	//
	if (slab_is_available()) {
		if (node_state(nid, N_HIGH_MEMORY))
			section = kzalloc_node(array_size, GFP_KERNEL, nid);
		else
			section = kzalloc(array_size, GFP_KERNEL);
	} else {
		// 2014-12-20 시작;
		// struct pglist_data의 전역변수인 contig_page_data의 주소를
		// 첫 번째 인자로 전달
		section = alloc_bootmem_node(NODE_DATA(nid), array_size);
		// 2014-12-27 종료;
	}

	return section;
}

// 2014-12-13
// nid = 0
static int __meminit sparse_index_init(unsigned long section_nr, int nid)
{
	unsigned long root = SECTION_NR_TO_ROOT(section_nr);
	struct mem_section *section;

	if (mem_section[root])
		return -EEXIST;

	// 2014-12-13, start
	section = sparse_index_alloc(nid);
	if (!section)
		return -ENOMEM;

	mem_section[root] = section;

	return 0;
}
#else /* !SPARSEMEM_EXTREME */
static inline int sparse_index_init(unsigned long section_nr, int nid)
{
	return 0;
}
#endif

/*
 * Although written for the SPARSEMEM_EXTREME case, this happens
 * to also work for the flat array case because
 * NR_SECTION_ROOTS==NR_MEM_SECTIONS.
 */
int __section_nr(struct mem_section* ms)
{
	unsigned long root_nr;
	struct mem_section* root;

	for (root_nr = 0; root_nr < NR_SECTION_ROOTS; root_nr++) {
		root = __nr_to_section(root_nr * SECTIONS_PER_ROOT);
		if (!root)
			continue;

		if ((ms >= root) && (ms < (root + SECTIONS_PER_ROOT)))
		     break;
	}

	VM_BUG_ON(root_nr == NR_SECTION_ROOTS);

	return (root_nr * SECTIONS_PER_ROOT) + (ms - root);
}

/*
 * During early boot, before section_mem_map is used for an actual
 * mem_map, we use section_mem_map to store the section's NUMA
 * node.  This keeps us from having to use another data structure.  The
 * node information is cleared just before we store the real mem_map.
 */
// 2014-12-27;
static inline unsigned long sparse_encode_early_nid(int nid)
{
	return (nid << SECTION_NID_SHIFT/*2*/); 
}

// 2014-12-27;
static inline int sparse_early_nid(struct mem_section *section)
{
	return (section->section_mem_map >> SECTION_NID_SHIFT);
}

/* Validate the physical addressing limitations of the model */
// 2014-11-08, 어떤 경우라도 max_sparsemem_pfn보다 클 수 없다.
void __meminit mminit_validate_memmodel_limits(unsigned long *start_pfn,
						unsigned long *end_pfn)
{
	unsigned long max_sparsemem_pfn = 1UL << (MAX_PHYSMEM_BITS-PAGE_SHIFT);	// 32 - 12 = 20, 1MB(0x0010_0000)

	/*
	 * Sanity checks - do not allow an architecture to pass
	 * in larger pfns than the maximum scope of sparsemem:
	 */
	if (*start_pfn > max_sparsemem_pfn) {
		mminit_dprintk(MMINIT_WARNING, "pfnvalidation",
			"Start of range %lu -> %lu exceeds SPARSEMEM max %lu\n",
			*start_pfn, *end_pfn, max_sparsemem_pfn);
		WARN_ON_ONCE(1);
		*start_pfn = max_sparsemem_pfn;
		*end_pfn = max_sparsemem_pfn;
	} else if (*end_pfn > max_sparsemem_pfn) {
		mminit_dprintk(MMINIT_WARNING, "pfnvalidation",
			"End of range %lu -> %lu exceeds SPARSEMEM max %lu\n",
			*start_pfn, *end_pfn, max_sparsemem_pfn);
		WARN_ON_ONCE(1);
		*end_pfn = max_sparsemem_pfn;
	}
}

// 2014-12-13 시작;
// 2014-12-27 종료;
/* Record a memory area against a node. */
// memory_present(0, memblock_region_memory_base_pfn(reg),
//          memblock_region_memory_end_pfn(reg));
void __init memory_present(int nid, unsigned long start, unsigned long end)
{
	unsigned long pfn;

	start &= PAGE_SECTION_MASK;		// PAGE_SECTION_MASK(0xFFFF_0000)
	// 2014-12-13
	// 오류가 있을 경우, start, end에 대한 값보정이 이루어진다.
	mminit_validate_memmodel_limits(&start, &end);
	// PAGES_PER_SECTION(0x0001_0000)
	for (pfn = start; pfn < end; pfn += PAGES_PER_SECTION) {
		unsigned long section = pfn_to_section_nr(pfn);
		struct mem_section *ms;

		// 2014-12-13, start
		sparse_index_init(section, nid);
		set_section_nid(section, nid);

		ms = __nr_to_section(section);
		if (!ms->section_mem_map)
			ms->section_mem_map = sparse_encode_early_nid(nid) |
							SECTION_MARKED_PRESENT;
                                              // (nid << 2) | 1
	}
}

/*
 * Only used by the i386 NUMA architecures, but relatively
 * generic code.
 */
unsigned long __init node_memmap_size_bytes(int nid, unsigned long start_pfn,
						     unsigned long end_pfn)
{
	unsigned long pfn;
	unsigned long nr_pages = 0;

	mminit_validate_memmodel_limits(&start_pfn, &end_pfn);
	for (pfn = start_pfn; pfn < end_pfn; pfn += PAGES_PER_SECTION) {
		if (nid != early_pfn_to_nid(pfn))
			continue;

		if (pfn_present(pfn))
			nr_pages += PAGES_PER_SECTION;
	}

	return nr_pages * sizeof(struct page);
}

/*
 * Subtle, we encode the real pfn into the mem_map such that
 * the identity pfn - section_mem_map will return the actual
 * physical page frame number.
 */
// 2015-01-03
/*
 * sparse_encode_mem_map함수와 sparse_decode_mem_map함수를 같이 보도록한다.
 * mem_section strcut에서 section_mem_map에 대한 주석을 보면
 * 주소를 보호하기위 한 수단으로 아래와 같은 방법을 사용했다고한다.
 * (예를 들어서 해킹과같은 이유로 멤버를 참조해서 직접 접근해서
 * 알아낼 수 있으므로 이를 방지)
 * */
static unsigned long sparse_encode_mem_map(struct page *mem_map, unsigned long pnum)
{
	//sec : 0 1 2 3 , << 16  ==> 0x0 0x10000 0x20000 0x30000
	return (unsigned long)(mem_map - (section_nr_to_pfn(pnum)));
}

/*
 * Decode mem_map from the coded memmap
 */
struct page *sparse_decode_mem_map(unsigned long coded_mem_map, unsigned long pnum)
{
	/* mask off the extra low bits of information */
	coded_mem_map &= SECTION_MAP_MASK;
	return ((struct page *)coded_mem_map) + section_nr_to_pfn(pnum);
}

// 2015-01-03
static int __meminit sparse_init_one_section(struct mem_section *ms,
		unsigned long pnum, struct page *mem_map,
		unsigned long *pageblock_bitmap)
{
	if (!present_section(ms))
		return -EINVAL;

	//하위 2비트만 가져옴
	ms->section_mem_map &= ~SECTION_MAP_MASK;
	ms->section_mem_map |= sparse_encode_mem_map(mem_map, pnum) |
							SECTION_HAS_MEM_MAP;
 	ms->pageblock_flags = pageblock_bitmap;

	return 1;
}

unsigned long usemap_size(void)
{
	unsigned long size_bytes;
	// (256) /8 = 32
	size_bytes = roundup(SECTION_BLOCKFLAGS_BITS, 8) / 8;
	size_bytes = roundup(size_bytes, sizeof(unsigned long));
	return size_bytes;
}

#ifdef CONFIG_MEMORY_HOTPLUG
static unsigned long *__kmalloc_section_usemap(void)
{
	return kmalloc(usemap_size(), GFP_KERNEL);
}
#endif /* CONFIG_MEMORY_HOTPLUG */

#ifdef CONFIG_MEMORY_HOTREMOVE // not set
static unsigned long * __init
sparse_early_usemaps_alloc_pgdat_section(struct pglist_data *pgdat,
					 unsigned long size)
{
	unsigned long goal, limit;
	unsigned long *p;
	int nid;
	/*
	 * A page may contain usemaps for other sections preventing the
	 * page being freed and making a section unremovable while
	 * other sections referencing the usemap retmain active. Similarly,
	 * a pgdat can prevent a section being removed. If section A
	 * contains a pgdat and section B contains the usemap, both
	 * sections become inter-dependent. This allocates usemaps
	 * from the same section as the pgdat where possible to avoid
	 * this problem.
	 */
	goal = __pa(pgdat) & (PAGE_SECTION_MASK << PAGE_SHIFT);
	limit = goal + (1UL << PA_SECTION_SHIFT);
	nid = early_pfn_to_nid(goal >> PAGE_SHIFT);
again:
	p = ___alloc_bootmem_node_nopanic(NODE_DATA(nid), size,
					  SMP_CACHE_BYTES, goal, limit);
	if (!p && limit) {
		limit = 0;
		goto again;
	}
	return p;
}

static void __init check_usemap_section_nr(int nid, unsigned long *usemap)
{
	unsigned long usemap_snr, pgdat_snr;
	static unsigned long old_usemap_snr = NR_MEM_SECTIONS;
	static unsigned long old_pgdat_snr = NR_MEM_SECTIONS;
	struct pglist_data *pgdat = NODE_DATA(nid);
	int usemap_nid;

	usemap_snr = pfn_to_section_nr(__pa(usemap) >> PAGE_SHIFT);
	pgdat_snr = pfn_to_section_nr(__pa(pgdat) >> PAGE_SHIFT);
	if (usemap_snr == pgdat_snr)
		return;

	if (old_usemap_snr == usemap_snr && old_pgdat_snr == pgdat_snr)
		/* skip redundant message */
		return;

	old_usemap_snr = usemap_snr;
	old_pgdat_snr = pgdat_snr;

	usemap_nid = sparse_early_nid(__nr_to_section(usemap_snr));
	if (usemap_nid != nid) {
		printk(KERN_INFO
		       "node %d must be removed before remove section %ld\n",
		       nid, usemap_snr);
		return;
	}
	/*
	 * There is a circular dependency.
	 * Some platforms allow un-removable section because they will just
	 * gather other removable sections for dynamic partitioning.
	 * Just notify un-removable section's number here.
	 */
	printk(KERN_INFO "Section %ld and %ld (node %d)", usemap_snr,
	       pgdat_snr, nid);
	printk(KERN_CONT
	       " have a circular dependency on usemap and pgdat allocations\n");
}
#else
static unsigned long * __init
sparse_early_usemaps_alloc_pgdat_section(struct pglist_data *pgdat,
					 unsigned long size)
{
	return alloc_bootmem_node_nopanic(pgdat, size);
}

static void __init check_usemap_section_nr(int nid, unsigned long *usemap)
{
}
#endif /* CONFIG_MEMORY_HOTREMOVE */

/* 
 * data = usemap, pnum_begin = nodeid를 처음으로 가진 ms
 * pnum_end = nodeid를 마지막으로 가진 ms
 * usemap_count = nodeid를 가진 ms 개수
 */
/*
 * 1. usemap_map에 대한 전체 크기를 할당 (ms cnt * usemap_size)
 * 2. 검색해놓은 ms에 할당한 usemap을 mapping
 * */
static void __init sparse_early_usemaps_alloc_node(void *data,
				 unsigned long pnum_begin,
				 unsigned long pnum_end,
				 unsigned long usemap_count, int nodeid)
{
	void *usemap;
	unsigned long pnum;
	unsigned long **usemap_map = (unsigned long **)data;
	/*
	 * pageblock이란것은 하나당 1024개의 page를 관리한다.
	 * pageblock은 코드를 따라가면 4bit로 표현된다는 것을 알 수 있고
	 * section 1개가 256MB를 관리하므로 그에 대한 연산을 하면
	 * section 1개당 64개의 pageblock을 사용해야하므로 이 pageblock을
	 * 표현하기 위해산 pageblock1개의 크기 4bit * 64 = 32byte가 필요
	 * 하다는 것을 알 수 있다.
	 * */
	int size = usemap_size(); // 32

	usemap = sparse_early_usemaps_alloc_pgdat_section(NODE_DATA(nodeid),
							  size * usemap_count);
	if (!usemap) {
		printk(KERN_WARNING "%s: allocation failed\n", __func__);
		return;
	}

	for (pnum = pnum_begin; pnum < pnum_end; pnum++) {
		if (!present_section_nr(pnum))
			continue;
		usemap_map[pnum] = usemap;
		usemap += size;
		check_usemap_section_nr(nodeid, usemap_map[pnum]); //undef
	}
}

#ifndef CONFIG_SPARSEMEM_VMEMMAP // CONFIG_SPARSEMEM_VMEMMAP = n
/*
 * 섹션 한개가 관리하는 pages(64k 개)에 대한 정보들을 저장하기위한 
 * page를 할당한다.
 */
struct page __init *sparse_mem_map_populate(unsigned long pnum, int nid)
{
	struct page *map;
	unsigned long size;

	//not set
	map = alloc_remap(nid, sizeof(struct page) * PAGES_PER_SECTION);
	if (map)
		return map;
	//
	size = PAGE_ALIGN(sizeof(struct page) * PAGES_PER_SECTION);
	map = __alloc_bootmem_node_high(NODE_DATA(nid), size,
					 PAGE_SIZE, __pa(MAX_DMA_ADDRESS));
	return map;
}
void __init sparse_mem_maps_populate_node(struct page **map_map,
					  unsigned long pnum_begin,
					  unsigned long pnum_end,
					  unsigned long map_count, int nodeid)
{
	void *map;
	unsigned long pnum;
	unsigned long size = sizeof(struct page) * PAGES_PER_SECTION;

	map = alloc_remap(nodeid, size * map_count);
	if (map) {
		for (pnum = pnum_begin; pnum < pnum_end; pnum++) {
			if (!present_section_nr(pnum))
				continue;
			map_map[pnum] = map;
			map += size;
		}
		return;
	}

	size = PAGE_ALIGN(size);
	map = __alloc_bootmem_node_high(NODE_DATA(nodeid), size * map_count,
					 PAGE_SIZE, __pa(MAX_DMA_ADDRESS));
	if (map) {
		for (pnum = pnum_begin; pnum < pnum_end; pnum++) {
			if (!present_section_nr(pnum))
				continue;
			map_map[pnum] = map;
			map += size;
		}
		return;
	}

	/* fallback */
	for (pnum = pnum_begin; pnum < pnum_end; pnum++) {
		struct mem_section *ms;

		if (!present_section_nr(pnum))
			continue;
		map_map[pnum] = sparse_mem_map_populate(pnum, nodeid);
		if (map_map[pnum])
			continue;
		ms = __nr_to_section(pnum);
		printk(KERN_ERR "%s: sparsemem memory map backing failed "
			"some memory will not be available.\n", __func__);
		ms->section_mem_map = 0;
	}
}
#endif /* !CONFIG_SPARSEMEM_VMEMMAP */

#ifdef CONFIG_SPARSEMEM_ALLOC_MEM_MAP_TOGETHER //not set
static void __init sparse_early_mem_maps_alloc_node(void *data,
				 unsigned long pnum_begin,
				 unsigned long pnum_end,
				 unsigned long map_count, int nodeid)
{
	struct page **map_map = (struct page **)data;
	sparse_mem_maps_populate_node(map_map, pnum_begin, pnum_end,
					 map_count, nodeid);
}
#else
static struct page __init *sparse_early_mem_map_alloc(unsigned long pnum)
{
	struct page *map;
	struct mem_section *ms = __nr_to_section(pnum);
	int nid = sparse_early_nid(ms);

	map = sparse_mem_map_populate(pnum, nid);
	if (map)
		return map;

	printk(KERN_ERR "%s: sparsemem memory map backing failed "
			"some memory will not be available.\n", __func__);
	ms->section_mem_map = 0;
	return NULL;
}
#endif

void __attribute__((weak)) __meminit vmemmap_populate_print_last(void)
{
}

/**
 *  alloc_usemap_and_memmap - memory alloction for pageblock flags and vmemmap
 *  @map: usemap_map for pageblock flags or mmap_map for vmemmap
 */
// 2014-12-27 흝어봄; map_count의 값과 nodeid_begin에 대해 특이사항이 있음;
// alloc_usemap_and_memmap(sparse_early_usemaps_alloc_node, (void *)usemap_map);
/*
 * 1. present된 section을 찾음, nodeid 저장
 * 2. present되잇으면서 같은 nodeid인것을 찾아
 *	nodeid가 같으면 => mem cnt ++,
 *	nodeid가 다르면 => 그 전에꺼까지의 메모리 할당, 1부터 다시시작
 * */
static void __init alloc_usemap_and_memmap(void (*alloc_func)
					(void *, unsigned long, unsigned long,
					unsigned long, int), void *data)
{
	unsigned long pnum;
	unsigned long map_count;
	int nodeid_begin = 0;
	unsigned long pnum_begin = 0;

	for (pnum = 0; pnum < NR_MEM_SECTIONS/*16*/; pnum++) {
		struct mem_section *ms;

		// SECTION_MARKED_PRESENT 상태인것을 찾음
		if (!present_section_nr(pnum))
			continue;
		ms = __nr_to_section(pnum);
		nodeid_begin = sparse_early_nid(ms);
		//             ms->section_mem_map >> SECTION_NID_SHIFT
		/*
		 * 하위 2비트는 section_map에대한 속성을 나타내고, 3번비트는
		 * nid를 의미한다.
		 */
		pnum_begin = pnum;
		break;
	}
	map_count = 1;
	/*
	 * 1. present 된 ms를 먼저 찾는다.
	 * 2. 같은 nodeid끼리를 묶어서 alloc_func를 실행한다. 
	 * */
	for (pnum = pnum_begin + 1; pnum < NR_MEM_SECTIONS; pnum++) {
		struct mem_section *ms;
		int nodeid;

		if (!present_section_nr(pnum))
			continue;
		ms = __nr_to_section(pnum);
		nodeid = sparse_early_nid(ms);
		if (nodeid == nodeid_begin) {
			map_count++;
			continue;
		}
		/* ok, we need to take cake of from pnum_begin to pnum - 1*/
		alloc_func(data, pnum_begin, pnum,
						map_count, nodeid_begin);
		/* new start, update count etc*/
		nodeid_begin = nodeid;
		pnum_begin = pnum;
		map_count = 1;
	}
	/* ok, last chunk */
	alloc_func(data, pnum_begin, NR_MEM_SECTIONS,
						map_count, nodeid_begin);
}

/*
 * Allocate the accumulated non-linear sections, allocate a mem_map
 * for each and record the physical to section mapping.
 */
// 2014-12-27 시작;
/*
 * 1. section갯수만큼의 필요한  usemap_map 생성
 * 2. present된 ms에 대한 메모리 생성 및 usemap_map에 등록
 * 3. section에 대한 usemap_map을
 *	 각 section의 pageblock_flags에 등록 및 초기화
 * 4. usemap_map 해제
 * */
void __init sparse_init(void)
{
	unsigned long pnum;
	struct page *map;
	unsigned long *usemap;
	unsigned long **usemap_map;
	int size;
#ifdef CONFIG_SPARSEMEM_ALLOC_MEM_MAP_TOGETHER // 비활성화
	int size2;
	struct page **map_map;
#endif

	/* see include/linux/mmzone.h 'struct mem_section' definition */
	BUILD_BUG_ON(!is_power_of_2(sizeof(struct mem_section)));

	/* Setup pageblock_order for HUGETLB_PAGE_SIZE_VARIABLE */
	set_pageblock_order();//not define

	/*
	 * map is using big page (aka 2M in x86 64 bit)
	 * usemap is less one page (aka 24 bytes)
	 * so alloc 2M (with 2M align) and 24 bytes in turn will
	 * make next 2M slip to one more 2M later.
	 * then in big system, the memory will have a lot of holes...
	 * here try to allocate 2M pages continuously.
	 *
	 * powerpc need to call sparse_init_one_section right after each
	 * sparse_early_mem_map_alloc, so allocate usemap_map at first.
	 */
	//section 개수는 총 16개.
	size = sizeof(unsigned long *) * NR_MEM_SECTIONS; // 64 = 4 * 16;
	usemap_map = alloc_bootmem(size);
	if (!usemap_map)
		panic("can not allocate usemap_map\n");
	alloc_usemap_and_memmap(sparse_early_usemaps_alloc_node,
							(void *)usemap_map);

#ifdef CONFIG_SPARSEMEM_ALLOC_MEM_MAP_TOGETHER //not set
	size2 = sizeof(struct page *) * NR_MEM_SECTIONS;
	map_map = alloc_bootmem(size2);
	if (!map_map)
		panic("can not allocate map_map\n");
	alloc_usemap_and_memmap(sparse_early_mem_maps_alloc_node,
							(void *)map_map);
#endif

	for (pnum = 0; pnum < NR_MEM_SECTIONS; pnum++) {

		if (!present_section_nr(pnum))
			continue;
		//present section은 null인지를 검사
		usemap = usemap_map[pnum];
		if (!usemap)
			continue;
#ifdef CONFIG_SPARSEMEM_ALLOC_MEM_MAP_TOGETHER //not set
		map = map_map[pnum];
#else
		//null아니면 pum에 대한 section의 pages를 관리하기 위한
		//page를 할당
		map = sparse_early_mem_map_alloc(pnum);
#endif
		if (!map)
			continue;

		sparse_init_one_section(__nr_to_section(pnum), pnum, map,
								usemap);
	}

	vmemmap_populate_print_last();

#ifdef CONFIG_SPARSEMEM_ALLOC_MEM_MAP_TOGETHER //not set
	free_bootmem(__pa(map_map), size2);
#endif
	free_bootmem(__pa(usemap_map), size);
}

#ifdef CONFIG_MEMORY_HOTPLUG
#ifdef CONFIG_SPARSEMEM_VMEMMAP
static inline struct page *kmalloc_section_memmap(unsigned long pnum, int nid,
						 unsigned long nr_pages)
{
	/* This will make the necessary allocations eventually. */
	return sparse_mem_map_populate(pnum, nid);
}
static void __kfree_section_memmap(struct page *memmap, unsigned long nr_pages)
{
	unsigned long start = (unsigned long)memmap;
	unsigned long end = (unsigned long)(memmap + nr_pages);

	vmemmap_free(start, end);
}
#ifdef CONFIG_MEMORY_HOTREMOVE
static void free_map_bootmem(struct page *memmap, unsigned long nr_pages)
{
	unsigned long start = (unsigned long)memmap;
	unsigned long end = (unsigned long)(memmap + nr_pages);

	vmemmap_free(start, end);
}
#endif /* CONFIG_MEMORY_HOTREMOVE */
#else
static struct page *__kmalloc_section_memmap(unsigned long nr_pages)
{
	struct page *page, *ret;
	unsigned long memmap_size = sizeof(struct page) * nr_pages;

	page = alloc_pages(GFP_KERNEL|__GFP_NOWARN, get_order(memmap_size));
	if (page)
		goto got_map_page;

	ret = vmalloc(memmap_size);
	if (ret)
		goto got_map_ptr;

	return NULL;
got_map_page:
	ret = (struct page *)pfn_to_kaddr(page_to_pfn(page));
got_map_ptr:

	return ret;
}

static inline struct page *kmalloc_section_memmap(unsigned long pnum, int nid,
						  unsigned long nr_pages)
{
	return __kmalloc_section_memmap(nr_pages);
}

static void __kfree_section_memmap(struct page *memmap, unsigned long nr_pages)
{
	if (is_vmalloc_addr(memmap))
		vfree(memmap);
	else
		free_pages((unsigned long)memmap,
			   get_order(sizeof(struct page) * nr_pages));
}

#ifdef CONFIG_MEMORY_HOTREMOVE
static void free_map_bootmem(struct page *memmap, unsigned long nr_pages)
{
	unsigned long maps_section_nr, removing_section_nr, i;
	unsigned long magic;
	struct page *page = virt_to_page(memmap);

	for (i = 0; i < nr_pages; i++, page++) {
		magic = (unsigned long) page->lru.next;

		BUG_ON(magic == NODE_INFO);

		maps_section_nr = pfn_to_section_nr(page_to_pfn(page));
		removing_section_nr = page->private;

		/*
		 * When this function is called, the removing section is
		 * logical offlined state. This means all pages are isolated
		 * from page allocator. If removing section's memmap is placed
		 * on the same section, it must not be freed.
		 * If it is freed, page allocator may allocate it which will
		 * be removed physically soon.
		 */
		if (maps_section_nr != removing_section_nr)
			put_page_bootmem(page);
	}
}
#endif /* CONFIG_MEMORY_HOTREMOVE */
#endif /* CONFIG_SPARSEMEM_VMEMMAP */

/*
 * returns the number of sections whose mem_maps were properly
 * set.  If this is <=0, then that means that the passed-in
 * map was not consumed and must be freed.
 */
int __meminit sparse_add_one_section(struct zone *zone, unsigned long start_pfn,
			   int nr_pages)
{
	unsigned long section_nr = pfn_to_section_nr(start_pfn);
	struct pglist_data *pgdat = zone->zone_pgdat;
	struct mem_section *ms;
	struct page *memmap;
	unsigned long *usemap;
	unsigned long flags;
	int ret;

	/*
	 * no locking for this, because it does its own
	 * plus, it does a kmalloc
	 */
	ret = sparse_index_init(section_nr, pgdat->node_id);
	if (ret < 0 && ret != -EEXIST)
		return ret;
	memmap = kmalloc_section_memmap(section_nr, pgdat->node_id, nr_pages);
	if (!memmap)
		return -ENOMEM;
	usemap = __kmalloc_section_usemap();
	if (!usemap) {
		__kfree_section_memmap(memmap, nr_pages);
		return -ENOMEM;
	}

	pgdat_resize_lock(pgdat, &flags);

	ms = __pfn_to_section(start_pfn);
	if (ms->section_mem_map & SECTION_MARKED_PRESENT) {
		ret = -EEXIST;
		goto out;
	}

	memset(memmap, 0, sizeof(struct page) * nr_pages);

	ms->section_mem_map |= SECTION_MARKED_PRESENT;

	ret = sparse_init_one_section(ms, section_nr, memmap, usemap);

out:
	pgdat_resize_unlock(pgdat, &flags);
	if (ret <= 0) {
		kfree(usemap);
		__kfree_section_memmap(memmap, nr_pages);
	}
	return ret;
}

#ifdef CONFIG_MEMORY_HOTREMOVE
#ifdef CONFIG_MEMORY_FAILURE
static void clear_hwpoisoned_pages(struct page *memmap, int nr_pages)
{
	int i;

	if (!memmap)
		return;

	for (i = 0; i < PAGES_PER_SECTION; i++) {
		if (PageHWPoison(&memmap[i])) {
			atomic_long_sub(1, &num_poisoned_pages);
			ClearPageHWPoison(&memmap[i]);
		}
	}
}
#else
static inline void clear_hwpoisoned_pages(struct page *memmap, int nr_pages)
{
}
#endif

static void free_section_usemap(struct page *memmap, unsigned long *usemap)
{
	struct page *usemap_page;
	unsigned long nr_pages;

	if (!usemap)
		return;

	usemap_page = virt_to_page(usemap);
	/*
	 * Check to see if allocation came from hot-plug-add
	 */
	if (PageSlab(usemap_page) || PageCompound(usemap_page)) {
		kfree(usemap);
		if (memmap)
			__kfree_section_memmap(memmap, PAGES_PER_SECTION);
		return;
	}

	/*
	 * The usemap came from bootmem. This is packed with other usemaps
	 * on the section which has pgdat at boot time. Just keep it as is now.
	 */

	if (memmap) {
		nr_pages = PAGE_ALIGN(PAGES_PER_SECTION * sizeof(struct page))
			>> PAGE_SHIFT;

		free_map_bootmem(memmap, nr_pages);
	}
}

void sparse_remove_one_section(struct zone *zone, struct mem_section *ms)
{
	struct page *memmap = NULL;
	unsigned long *usemap = NULL, flags;
	struct pglist_data *pgdat = zone->zone_pgdat;

	pgdat_resize_lock(pgdat, &flags);
	if (ms->section_mem_map) {
		usemap = ms->pageblock_flags;
		memmap = sparse_decode_mem_map(ms->section_mem_map,
						__section_nr(ms));
		ms->section_mem_map = 0;
		ms->pageblock_flags = NULL;
	}
	pgdat_resize_unlock(pgdat, &flags);

	clear_hwpoisoned_pages(memmap, PAGES_PER_SECTION);
	free_section_usemap(memmap, usemap);
}
#endif /* CONFIG_MEMORY_HOTREMOVE */
#endif /* CONFIG_MEMORY_HOTPLUG */
