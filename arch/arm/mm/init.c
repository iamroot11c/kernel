/*
 *  linux/arch/arm/mm/init.c
 *
 *  Copyright (C) 1995-2005 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/swap.h>
#include <linux/init.h>
#include <linux/bootmem.h>
#include <linux/mman.h>
#include <linux/export.h>
#include <linux/nodemask.h>
#include <linux/initrd.h>
#include <linux/of_fdt.h>
#include <linux/highmem.h>
#include <linux/gfp.h>
#include <linux/memblock.h>
#include <linux/dma-contiguous.h>
#include <linux/sizes.h>

#include <asm/mach-types.h>
#include <asm/memblock.h>
#include <asm/prom.h>
#include <asm/sections.h>
#include <asm/setup.h>
#include <asm/tlb.h>
#include <asm/fixmap.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include "mm.h"

static phys_addr_t phys_initrd_start __initdata = 0;
static unsigned long phys_initrd_size __initdata = 0;

static int __init early_initrd(char *p)
{
	phys_addr_t start;
	unsigned long size;
	char *endp;

	start = memparse(p, &endp);
	if (*endp == ',') {
		size = memparse(endp + 1, NULL);

		phys_initrd_start = start;
		phys_initrd_size = size;
	}
	return 0;
}
early_param("initrd", early_initrd);

static int __init parse_tag_initrd(const struct tag *tag)
{
	printk(KERN_WARNING "ATAG_INITRD is deprecated; "
		"please update your bootloader.\n");
	phys_initrd_start = __virt_to_phys(tag->u.initrd.start);
	phys_initrd_size = tag->u.initrd.size;
	return 0;
}

__tagtable(ATAG_INITRD, parse_tag_initrd);

static int __init parse_tag_initrd2(const struct tag *tag)
{
	phys_initrd_start = tag->u.initrd.start;
	phys_initrd_size = tag->u.initrd.size;
	return 0;
}

__tagtable(ATAG_INITRD2, parse_tag_initrd2);

#ifdef CONFIG_OF_FLATTREE
void __init early_init_dt_setup_initrd_arch(u64 start, u64 end)
{
	phys_initrd_start = start;
	phys_initrd_size = end - start;
}
#endif /* CONFIG_OF_FLATTREE */

/*
 * This keeps memory configuration data used by a couple memory
 * initialization functions, as well as show_mem() for the skipping
 * of holes in the memory map.  It is populated by arm_add_memory().
 */
struct meminfo meminfo;

void show_mem(unsigned int filter)
{
	int free = 0, total = 0, reserved = 0;
	int shared = 0, cached = 0, slab = 0, i;
	struct meminfo * mi = &meminfo;

	printk("Mem-info:\n");
	show_free_areas(filter);

	if (filter & SHOW_MEM_FILTER_PAGE_COUNT)
		return;

	for_each_bank (i, mi) {
		struct membank *bank = &mi->bank[i];
		unsigned int pfn1, pfn2;
		struct page *page, *end;

		pfn1 = bank_pfn_start(bank);
		pfn2 = bank_pfn_end(bank);

		page = pfn_to_page(pfn1);
		end  = pfn_to_page(pfn2 - 1) + 1;

		do {
			total++;
			if (PageReserved(page))
				reserved++;
			else if (PageSwapCache(page))
				cached++;
			else if (PageSlab(page))
				slab++;
			else if (!page_count(page))
				free++;
			else
				shared += page_count(page) - 1;
			page++;
		} while (page < end);
	}

	printk("%d pages of RAM\n", total);
	printk("%d free pages\n", free);
	printk("%d reserved pages\n", reserved);
	printk("%d slab pages\n", slab);
	printk("%d pages shared\n", shared);
	printk("%d pages swap cached\n", cached);
}

// 2014-11-01
// 페이지 프레임 번호를 찾음
//
static void __init find_limits(unsigned long *min, unsigned long *max_low,
			       unsigned long *max_high)
{
	struct meminfo *mi = &meminfo;
	int i;

	/* This assumes the meminfo array is properly sorted */
	// #define bank_pfn_start(bank)    __phys_to_pfn((bank)->start)
	//  membank 배열에서 첫 번째 요소의 시작 주소에 대한 
	//  페이지 프레임 번호를 min에 저장
	*min = bank_pfn_start(&mi->bank[0]);
	
	// membank 배열에서 하이메모리를 찾음
	for_each_bank (i, mi)
		if (mi->bank[i].highmem) 
				break;
	
	// membank 배열에서 하이메모리의 시작 주소에 대한 
	// 페이지 프레임 번호를 max_low에 저장
	*max_low = bank_pfn_end(&mi->bank[i - 1]);
	// membank 배열의 마지막 요소의 시작 주소에 대한
	// 페이지 프레임 번호를 max_high에 저장
	*max_high = bank_pfn_end(&mi->bank[mi->nr_banks - 1]);
}

// end_pft = max_low
static void __init arm_bootmem_init(unsigned long start_pfn,
	unsigned long end_pfn)
{
	struct memblock_region *reg;
	unsigned int boot_pages;
	phys_addr_t bitmap;
	pg_data_t *pgdat;

	/*
	 * Allocate the bootmem bitmap page.  This must be in a region
	 * of memory which has already been mapped.
	 */
	boot_pages = bootmem_bootmap_pages(end_pfn - start_pfn);
	// 2014-11-01 여기까지 함.

	// 2014-11-08 시작.
	// L1_CACHE_BYTES == 64
	bitmap = memblock_alloc_base(boot_pages << PAGE_SHIFT, L1_CACHE_BYTES,
				__pfn_to_phys(end_pfn));

	/*
	 * Initialise the bootmem allocator, handing the
	 * memory banks over to bootmem.
	 */

	// bootmam allocator 초기화, memory banks를 bootmem에게 양도한다(넘겨준다.)
	node_set_online(0);		// MAX_NUMNODES == 1임으로 특별히 하는 일이 없음.
	pgdat = NODE_DATA(0);	// (&contig_page_data)

	// 2014-11-08, Start
	init_bootmem_node(pgdat, __phys_to_pfn(bitmap), start_pfn, end_pfn);
	// 2014-11-08, end

	// 2014-11-08, Start, 여기까지
	/* Free the lowmem regions from memblock into bootmem. */
	for_each_memblock(memory, reg) {
		unsigned long start = memblock_region_memory_base_pfn(reg);
		unsigned long end = memblock_region_memory_end_pfn(reg);

		if (end >= end_pfn)
			end = end_pfn;
		if (start >= end)
			break;

		free_bootmem(__pfn_to_phys(start), (end - start) << PAGE_SHIFT);
		// 2014-12-13, free_bootmem
	}

	// 2014-12-13, start
	/* Reserve the lowmem memblock reserved regions in bootmem. */
	for_each_memblock(reserved, reg) {
		unsigned long start = memblock_region_reserved_base_pfn(reg);
		unsigned long end = memblock_region_reserved_end_pfn(reg);

		if (end >= end_pfn)
			end = end_pfn;
		if (start >= end)
			break;

		// 2014-12-13, start
		reserve_bootmem(__pfn_to_phys(start),
			        (end - start) << PAGE_SHIFT, BOOTMEM_DEFAULT /* 0 */);
		// 2014-12-13, end
	}
}

#ifdef CONFIG_ZONE_DMA

phys_addr_t arm_dma_zone_size __read_mostly;
EXPORT_SYMBOL(arm_dma_zone_size);

/*
 * The DMA mask corresponding to the maximum bus address allocatable
 * using GFP_DMA.  The default here places no restriction on DMA
 * allocations.  This must be the smallest DMA mask in the system,
 * so a successful GFP_DMA allocation will always satisfy this.
 */
phys_addr_t arm_dma_limit;

static void __init arm_adjust_dma_zone(unsigned long *size, unsigned long *hole,
	unsigned long dma_size)
{
	if (size[0] <= dma_size)
		return;

	size[ZONE_NORMAL] = size[0] - dma_size;
	size[ZONE_DMA] = dma_size;
	hole[ZONE_NORMAL] = hole[0];
	hole[ZONE_DMA] = 0;
}
#endif

void __init setup_dma_zone(const struct machine_desc *mdesc)
{
#ifdef CONFIG_ZONE_DMA
	if (mdesc->dma_zone_size) {
		arm_dma_zone_size = mdesc->dma_zone_size;
		arm_dma_limit = PHYS_OFFSET + arm_dma_zone_size - 1;
	} else
		arm_dma_limit = 0xffffffff;
#endif
}

// 2015-01-03
static void __init arm_bootmem_free(unsigned long min, unsigned long max_low,
	unsigned long max_high)
{
	//ZONE_NORMAL, ZONE_HIGHMEM, ZONE_MOVABLE 이렇게 3개 존재
	unsigned long zone_size[MAX_NR_ZONES], zhole_size[MAX_NR_ZONES];
	struct memblock_region *reg;

	/*
	 * initialise the zones.
	 */
	memset(zone_size, 0, sizeof(zone_size));

	/*
	 * The memory size has already been determined.  If we need
	 * to do anything fancy with the allocation of this memory
	 * to the zones, now is the time to do it.
	 */
	zone_size[0] = max_low - min;
#ifdef CONFIG_HIGHMEM
	zone_size[ZONE_HIGHMEM] = max_high - max_low;
#endif

	/*
	 * Calculate the size of the holes.
	 *  holes = node_size - sum(bank_sizes)
	 */
	memcpy(zhole_size, zone_size, sizeof(zhole_size));
	for_each_memblock(memory, reg) {
		unsigned long start = memblock_region_memory_base_pfn(reg);
		unsigned long end = memblock_region_memory_end_pfn(reg);

		if (start < max_low) {
			unsigned long low_end = min(end, max_low);
			zhole_size[0] -= low_end - start;
		}
#ifdef CONFIG_HIGHMEM // set
		if (end > max_low) {
			unsigned long high_start = max(start, max_low);
			zhole_size[ZONE_HIGHMEM] -= end - high_start;
		}
#endif
	}

#ifdef CONFIG_ZONE_DMA // not set
	/*
	 * Adjust the sizes according to any special requirements for
	 * this machine type.
	 */
	if (arm_dma_zone_size)
		arm_adjust_dma_zone(zone_size, zhole_size,
			arm_dma_zone_size >> PAGE_SHIFT);
#endif

	free_area_init_node(0, zone_size, min, zhole_size);
}

#ifdef CONFIG_HAVE_ARCH_PFN_VALID // true
// 2015-06-20
// 2015-10-03
// 2017-06-17
// 미리 구성된 memblock에서 이진 탐색을 통해서 점검한다.
int pfn_valid(unsigned long pfn)
{
	return memblock_is_memory(__pfn_to_phys(pfn));
}
EXPORT_SYMBOL(pfn_valid);
#endif

#ifndef CONFIG_SPARSEMEM	// CONFIG_SPARSEMEM=y
static void __init arm_memory_present(void)
{
}
#else
// 2014-12-13, flat memory, discontiguous memory, sparse memory?
// http://www.iamroot.org/xe/QnA/13649 - 백창우, 김남형 참고.
static void __init arm_memory_present(void)
{
	struct memblock_region *reg;

	for_each_memblock(memory, reg)
		memory_present(0, memblock_region_memory_base_pfn(reg),
			       memblock_region_memory_end_pfn(reg));
}
#endif

static bool arm_memblock_steal_permitted = true;

phys_addr_t __init arm_memblock_steal(phys_addr_t size, phys_addr_t align)
{
	phys_addr_t phys;

	BUG_ON(!arm_memblock_steal_permitted);

	phys = memblock_alloc_base(size, align, MEMBLOCK_ALLOC_ANYWHERE);
	memblock_free(phys, size);
	memblock_remove(phys, size);

	return phys;
}

// 2014.08.09 - 함수 분석해야 됨
// initrd_start, initrd_end,
// phys_initrd_start, phys_initrd_size 조사
// 메모리 추가 및 예약(reserve) 확인
void __init arm_memblock_init(struct meminfo *mi,
	const struct machine_desc *mdesc)
{
	int i;

	for (i = 0; i < mi->nr_banks; i++)
		memblock_add(mi->bank[i].start, mi->bank[i].size);

	/* Register the kernel text, kernel data and initrd with memblock. */
#ifdef CONFIG_XIP_KERNEL // 비 메핑되어 있지 않은 영역을 reserve로 부르며
	                 // 메핑되는 영역을 region으로 부름
	memblock_reserve(__pa(_sdata), _end - _sdata);
#else
	// text 영역을 reserve영역에 add 함.
	memblock_reserve(__pa(_stext), _end - _stext);
#endif
#ifdef CONFIG_BLK_DEV_INITRD		// defined
	if (phys_initrd_size &&
	    !memblock_is_region_memory(phys_initrd_start, phys_initrd_size)) {
		pr_err("INITRD: 0x%08llx+0x%08lx is not a memory region - disabling initrd\n",
		       (u64)phys_initrd_start, phys_initrd_size);
		phys_initrd_start = phys_initrd_size = 0;
	}
	if (phys_initrd_size &&
	    memblock_is_region_reserved(phys_initrd_start, phys_initrd_size)) {
		pr_err("INITRD: 0x%08llx+0x%08lx overlaps in-use memory region - disabling initrd\n",
		       (u64)phys_initrd_start, phys_initrd_size);
		phys_initrd_start = phys_initrd_size = 0;
	}
	if (phys_initrd_size) {
		memblock_reserve(phys_initrd_start, phys_initrd_size);

		/* Now convert initrd to virtual addresses */
		initrd_start = __phys_to_virt(phys_initrd_start);
		initrd_end = initrd_start + phys_initrd_size;
	}
#endif
// 2014-08-16, 여기까지 함

	// page global directory 영역을 reserved로 add
	arm_mm_memblock_reserve();
	// DTB의 reserved_map을 읽어서 reserved로 add
	arm_dt_memblock_reserve();

	/* reserve any platform specific memblock areas */
	// MFC(multi format codec)에 대한 reserve 영역을  s5p_mfc_mem 에 등록
	// 만약 겹치는 memory 영역이 있으면 remove함
	//  s5p-dev-mfc.c
	//   77 static struct s5p_mfc_reserved_mem s5p_mfc_mem[2] __initdata;
	if (mdesc->reserve)
		mdesc->reserve();

	/*
	 * reserve memory for DMA contigouos allocations,
	 * must come from DMA area inside low memory
	 */
	//arm_dma_limit: #define arm_dma_limit ((phys_addr_t)~0) =0xffffffff = 4Gbyte
	//arm_lowmem_limit: mmu.c에 void __init sanity_check_meminfo(void)에서 설정 
	//ZONE_NORMAL (low memory)
	dma_contiguous_reserve(min(arm_dma_limit, arm_lowmem_limit));

	arm_memblock_steal_permitted = false;
	memblock_allow_resize();
	memblock_dump_all();
}

void __init bootmem_init(void)
{
	unsigned long min, max_low, max_high;

	max_low = max_high = 0;

	// 하이메모리의 시작과 끝을 찾음
	// 페이지 프레임 번호를 찾음
	// 2015-02-12 추가
	// min은 bank 배열의 첫 번째 주소, max_low는 하이메모리 주소, 
	// max_high는 nf_banks를 인덱스로 bank에서 찾은 주소에 대한
	// 각 각의 PFN을 구함
	find_limits(&min, &max_low, &max_high);

	// 2014-11-01 시작
	arm_bootmem_init(min, max_low);
	// 2014-12-13, end

	/*
	 * Sparsemem tries to allocate bootmem in memory_present(),
	 * so must be done after the fixed reservations
	 */
	// 2014-12-13, start
	arm_memory_present();
	// 2014-12-27 종료;

	/*
	 * sparse_init() needs the bootmem allocator up and running.
	 */
	// 2014-12-27, start
	sparse_init();
	// 2015-01-03, end

	/*
	 * Now free the memory - free_area_init_node needs
	 * the sparse mem_map arrays initialized by sparse_init()
	 * for memmap_init_zone(), otherwise all PFNs are invalid.
	 */
	// 2015-01-03, start
	arm_bootmem_free(min, max_low, max_high);
	// 2015-01-24, 완료

	/*
	 * This doesn't seem to be used by the Linux memory manager any
	 * more, but is used by ll_rw_block.  If we can get rid of it, we
	 * also get rid of some of the stuff above as well.
	 *
	 * Note: max_low_pfn and max_pfn reflect the number of _pages_ in
	 * the system, not the maximum PFN.
	 */
	max_low_pfn = max_low - PHYS_PFN_OFFSET;
	max_pfn = max_high - PHYS_PFN_OFFSET;
	// 2015-01-24, 완료
}

/*
 * Poison init memory with an undefined instruction (ARM) or a branch to an
 * undefined instruction (Thumb).
 */
// 2018-03-10
static inline void poison_init_mem(void *s, size_t count)
{
	u32 *p = (u32 *)s;
	for (; count != 0; count -= 4)
		*p++ = 0xe7fddef0;
}

// 2015-05-09;
// free_memmap(prev_bank_end, bank_start);
static inline void
free_memmap(unsigned long start_pfn, unsigned long end_pfn)
{
	struct page *start_pg, *end_pg;
	phys_addr_t pg, pgend;

	/*
	 * Convert start_pfn/end_pfn to a struct page pointer.
	 */
	start_pg = pfn_to_page(start_pfn - 1) + 1;
	end_pg = pfn_to_page(end_pfn - 1) + 1;

	/*
	 * Convert to physical addresses, and
	 * round start upwards and end downwards.
	 */
	pg = PAGE_ALIGN(__pa(start_pg));
	pgend = __pa(end_pg) & PAGE_MASK;

	/*
	 * If there are free pages between these,
	 * free the section of the memmap array.
	 */
	if (pg < pgend)
		free_bootmem(pg, pgend - pg);
	// pg부터 pgend에서 pg를 빼서 구한 영역을 해제함
}

/*
 * The mem_map array can get very big.  Free the unused area of the memory map.
 */
// 2015-05-09; 
// free_unused_memmap(&meminfo);
static void __init free_unused_memmap(struct meminfo *mi)
{
	unsigned long bank_start, prev_bank_end = 0;
	unsigned int i;

	/*
	 * This relies on each bank being in address order.
	 * The banks are sorted previously in bootmem_init().
	 */
	// #define for_each_bank(iter,mi)              \
	//     for (iter = 0; iter < (mi)->nr_banks; iter++)
	for_each_bank(i, mi) {
		struct membank *bank = &mi->bank[i];

		bank_start = bank_pfn_start(bank);
		//         = (bank->start) >> PAGE_SHIFT/*12*/;

#ifdef CONFIG_SPARSEMEM
		/*
		 * Take care not to free memmap entries that don't exist
		 * due to SPARSEMEM sections which aren't present.
		 */
		// 최초의 값은 0
		bank_start = min(bank_start,
				 ALIGN(prev_bank_end, PAGES_PER_SECTION /*4KB, 0x0001_0000*/));
#else
		/*
		 * Align down here since the VM subsystem insists that the
		 * memmap entries are valid from the bank start aligned to
		 * MAX_ORDER_NR_PAGES.
		 */
		bank_start = round_down(bank_start, MAX_ORDER_NR_PAGES);
#endif
		/*
		 * If we had a previous bank, and there is a space
		 * between the current bank and the previous, free it.
		 */
		// 이전의 뱅크가 있을때 현재 뱅크와 이전 뱅크 사이에
		// 공백이 있으면 해제를 함
		if (prev_bank_end && prev_bank_end < bank_start)
			free_memmap(prev_bank_end, bank_start);

		/*
		 * Align up here since the VM subsystem insists that the
		 * memmap entries are valid from the bank end aligned to
		 * MAX_ORDER_NR_PAGES.
		 */
		prev_bank_end = ALIGN(bank_pfn_end(bank) , MAX_ORDER_NR_PAGES /*1024*/);
		//            = ALIGN((bank->start + bank->size) >> PAGE_SHIFT/*12*/,
		//                    1024);
	}

#ifdef CONFIG_SPARSEMEM
	// prev_bank_end가 PAGES_PER_SECTION으로 정렬되어있지 않다면 
	// free_memmap() 함수를 호출
	if (!IS_ALIGNED(prev_bank_end, PAGES_PER_SECTION))
		free_memmap(prev_bank_end,
			    ALIGN(prev_bank_end, PAGES_PER_SECTION));
#endif
}

// 2015-05-16
#ifdef CONFIG_HIGHMEM
static inline void free_area_high(unsigned long pfn, unsigned long end)
{
	for (; pfn < end; pfn++)
		free_highmem_page(pfn_to_page(pfn));
}
#endif

// 2015-05-16
static void __init free_highpages(void)
{
#ifdef CONFIG_HIGHMEM	// set
	unsigned long max_low = max_low_pfn + PHYS_PFN_OFFSET;
	struct memblock_region *mem, *res;

	/* set highmem page free */
	// mem = memblock.memory.regions
	for_each_memblock(memory, mem) {
		unsigned long start = memblock_region_memory_base_pfn(mem);
		unsigned long end = memblock_region_memory_end_pfn(mem);

		/* Ignore complete lowmem entries */
		if (end <= max_low)
			continue;

		/* Truncate partial highmem entries */
		if (start < max_low)
			start = max_low;

		/* Find and exclude any reserved regions */
		// res = memblock.reserved.regions
		for_each_memblock(reserved, res) {
			unsigned long res_start, res_end;

			// 위에서는 base를 up하고 있고, 
			// reserved 영역은 base를 down시키고 있다.
			// 어떤 차이인가?
			res_start = memblock_region_reserved_base_pfn(res);
			res_end = memblock_region_reserved_end_pfn(res);

			if (res_end < start)
				continue;
			if (res_start < start)
				res_start = start;
			if (res_start > end)
				res_start = end;
			if (res_end > end)
				res_end = end;
			if (res_start != start)
				free_area_high(start, res_start);
			start = res_end;
			if (start == end)
				break;
		}

		/* And now free anything which remains */
		if (start < end)
			free_area_high(start, end);
	}
#endif
}

/*
 * mem_init() marks the free areas in the mem_map and tells us how much
 * memory is free.  This is done after various parts of the system have
 * claimed their memory after the kernel image.
 */
// 2015-05-09; 시작
// TCM: Tightly-Coupled memory
// DTCM: Data TCM
// ITCM: Instruction TCM
void __init mem_init(void)
{
#ifdef CONFIG_HAVE_TCM // not define
	/* These pointers are filled in on TCM detection */
	extern u32 dtcm_end;
	extern u32 itcm_end;
#endif

	// 2015-05-09; 시작
	// max_low_pfn, max_pfn은 bootmem_init() 함수에서 값을 설정
	max_mapnr = pfn_to_page(max_pfn + PHYS_PFN_OFFSET) - mem_map;
	// 2015-05-09; 끝

	/* this will put all unused low memory onto the freelists */
	// 2015-05-09; 시작
	free_unused_memmap(&meminfo);
	// 2015-05-09; 끝
	// 2015-05-09; 시작
	free_all_bootmem();
	// 2015-05-09; 끝
	
	// 2015-05-09; 여기까지 진행

#ifdef CONFIG_SA1111 // not define
	/* now that our DMA memory is actually so designated, we can free it */
	free_reserved_area(__va(PHYS_OFFSET), swapper_pg_dir, -1, NULL);
#endif

	// 2015-05-16, start
	free_highpages();
	// 2015-05-16, end

	// 2015-05-16, start
	mem_init_print_info(NULL);
	// 2015-05-16, end

#define MLK(b, t) b, t, ((t) - (b)) >> 10
#define MLM(b, t) b, t, ((t) - (b)) >> 20
#define MLK_ROUNDUP(b, t) b, t, DIV_ROUND_UP(((t) - (b)), SZ_1K)

	printk(KERN_NOTICE "Virtual kernel memory layout:\n"
			"    vector  : 0x%08lx - 0x%08lx   (%4ld kB)\n"
#ifdef CONFIG_HAVE_TCM	// not set
			"    DTCM    : 0x%08lx - 0x%08lx   (%4ld kB)\n"
			"    ITCM    : 0x%08lx - 0x%08lx   (%4ld kB)\n"
#endif
			"    fixmap  : 0x%08lx - 0x%08lx   (%4ld kB)\n"
			"    vmalloc : 0x%08lx - 0x%08lx   (%4ld MB)\n"
			"    lowmem  : 0x%08lx - 0x%08lx   (%4ld MB)\n"
#ifdef CONFIG_HIGHMEM
			"    pkmap   : 0x%08lx - 0x%08lx   (%4ld MB)\n"
#endif
#ifdef CONFIG_MODULES
			"    modules : 0x%08lx - 0x%08lx   (%4ld MB)\n"
#endif
			"      .text : 0x%p" " - 0x%p" "   (%4d kB)\n"
			"      .init : 0x%p" " - 0x%p" "   (%4d kB)\n"
			"      .data : 0x%p" " - 0x%p" "   (%4d kB)\n"
			"       .bss : 0x%p" " - 0x%p" "   (%4d kB)\n",

			MLK(UL(CONFIG_VECTORS_BASE), UL(CONFIG_VECTORS_BASE) +
				(PAGE_SIZE)),
#ifdef CONFIG_HAVE_TCM
			MLK(DTCM_OFFSET, (unsigned long) dtcm_end),
			MLK(ITCM_OFFSET, (unsigned long) itcm_end),
#endif
			MLK(FIXADDR_START, FIXADDR_TOP),
			MLM(VMALLOC_START, VMALLOC_END),
			// high_memory는 lowmem의 끝을 가리킨다.
			MLM(PAGE_OFFSET, (unsigned long)high_memory),
#ifdef CONFIG_HIGHMEM
			MLM(PKMAP_BASE, (PKMAP_BASE) + (LAST_PKMAP) *
				(PAGE_SIZE)),
#endif
#ifdef CONFIG_MODULES
			MLM(MODULES_VADDR, MODULES_END),
#endif

			MLK_ROUNDUP(_text, _etext),
			MLK_ROUNDUP(__init_begin, __init_end),
			MLK_ROUNDUP(_sdata, _edata),
			MLK_ROUNDUP(__bss_start, __bss_stop));

#undef MLK
#undef MLM
#undef MLK_ROUNDUP

	/*
	 * Check boundaries twice: Some fundamental inconsistencies can
	 * be detected at build time already.
	 */
#ifdef CONFIG_MMU
	BUILD_BUG_ON(TASK_SIZE				> MODULES_VADDR);
	BUG_ON(TASK_SIZE 				> MODULES_VADDR);
#endif

#ifdef CONFIG_HIGHMEM
	BUILD_BUG_ON(PKMAP_BASE + LAST_PKMAP * PAGE_SIZE > PAGE_OFFSET);
	BUG_ON(PKMAP_BASE + LAST_PKMAP * PAGE_SIZE	> PAGE_OFFSET);
#endif

	if (PAGE_SIZE >= 16384 && get_num_physpages() <= 128) {
		extern int sysctl_overcommit_memory;
		/*
		 * On a machine this small we won't get
		 * anywhere without overcommit, so turn
		 * it on by default.
		 */
		sysctl_overcommit_memory = OVERCOMMIT_ALWAYS;
	}
}

// 2018-03-10 
void free_initmem(void)
{
#ifdef CONFIG_HAVE_TCM // n
	extern char __tcm_start, __tcm_end;

	poison_init_mem(&__tcm_start, &__tcm_end - &__tcm_start);
	free_reserved_area(&__tcm_start, &__tcm_end, -1, "TCM link");
#endif

	poison_init_mem(__init_begin, __init_end - __init_begin);
	if (!machine_is_integrator() && !machine_is_cintegrator())
		free_initmem_default(-1);
}

#ifdef CONFIG_BLK_DEV_INITRD

static int keep_initrd;

void free_initrd_mem(unsigned long start, unsigned long end)
{
	if (!keep_initrd) {
		poison_init_mem((void *)start, PAGE_ALIGN(end) - start);
		free_reserved_area((void *)start, (void *)end, -1, "initrd");
	}
}

static int __init keepinitrd_setup(char *__unused)
{
	keep_initrd = 1;
	return 1;
}

__setup("keepinitrd", keepinitrd_setup);
#endif
