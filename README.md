## IAMROOT.ORG Kernel 분석 스터디 `11차 C조` (ARM) ##

## Study History(일부 기록이 맞지 않을 수 있음) ##
+ [76주차](http://bit.ly/1RVgP8I) `2015.11.14`
 - [kmem_cache_init() 진행중]()
 - page_alloc.c:__alloc_pages_slowpath() 진행중
 - page_alloc.c:__alloc_pages_direct_reclaim() 진행중
 - page_alloc.c:__perform_reclaim() 진행중
 - vmscan.c:try_to_free_pages() 분석 중
 - page_alloc.c:__alloc_pages_direct_compact() 분석 완료
 - compaction.c:try_to_compact_pages() 분석 완료
 - compaction.c:compact_zone_order() 분석 완료
 - compaction.c:compact_zone() 분석 완료
 - mm/migrate.c:migrate_pages() 분석 완료
 - mm/migrate.c:unmap_and_move() 분석 완료


+ [75주차](https://goo.gl/0JCKNY) `2015.11.07`
 - [kmem_cache_init() 진행중]()
 - mm/rmap.c:__unmap_and_move() 진행중
 - mm/rmap.c:try_to_unmap() 분석 완료
 - mm/migrate.c:move_to_new_page() 분석중
 - mm/rmap.c:rmap_walk_anon() 분석중
 - mm/migrate.c:remove_migration_pte() 분석중

+ [73주차](http://bit.ly/1XpOmLF) `2015.10.24`
 - [kmem_cache_init() 진행중]()
 - mm/rmap.c:__unmap_and_move() 진행중
 - mm/rmap.c:try_to_unmap() 분석 완료
 - mm/migrate.c:move_to_new_page() 분석중
 - mm/rmap.c:rmap_walk_anon() 분석중
 - mm/migrate.c:remove_migration_pte() 분석중

+ [72주차]() `2015.10.17`
 - [kmem_cache_init() 진행중]()
 - mm/rmap.c:try_to_unmap_one() 진행중
 - mm/rmap.c:try_to_unmap_one() 분석 완료
 - mm/rmap.c:try_to_unmap_anon() 분석 완료
 - mm/rmap.c:try_to_unmap_file() 분석 료완


+ + [71주차](http://bit.ly/1VKDp4A) `2015.10.10`
 - [kmem_cache_init() 진행중]()
 - mm/rmap.c:try_to_unmap_one() 진행중
 - mm/rmap.c:try_to_unmap_one() 분석 완료
 - mm/rmap.c:try_to_unmap_anon() 분석 완료
 - mm/rmap.c:try_to_unmap_file() 분석중

+ [70주차](http://bit.ly/1Oe25Dk) `2015.10.03`
 - [kmem_cache_init() 진행중]()
 - mm/rmap.c:try_to_unmap_one() 진행중
 - mm/swapfile.c:swap_duplicate() 분석 완료
 - mm/swapfile.c:add_swap_count_continuation() 분석 완료 
 - include/linux/gfp.h:alloc_page() 함수를 통해 page 할당 리뷰
 - mm/rmap.c:set_pte_at() 완료
 - pge, pte, memsection, page를 통해 물리주소 구하는 방식 분석

+ [69주차](https://goo.gl/YcS9pK) `2015.09.19`
 - [kmem_cache_init() 진행중]()
 - mm/rmap.c:try_to_unmap_one() 진행중
 - mm/swapfile.c:swap_duplicate() 진행중
 - mm/swapfile.c:add_swap_count_continuation() 분석중
 - include/linux/gfp.h:alloc_page() 함수를 통해 page 할당 리뷰
 
+ [68주차](http://bit.ly/1Q8FUM5) `2015.09.12`
 - [kmem_cache_init() 진행중]()
 - mm/migrate.c:__unmap_and_move() 진행중
 - mod_timer 동작 분석 완료
 - include/linux/workqueue.h:queue_delayed_work() 진행완료

+ [67주차](http://bit.ly/1FpdBmU) `2015.09.05`
 - [kmem_cache_init() 진행중]()
 - mm/migrate.c:__unmap_and_move() 진행중
 - include/linux/workqueue.h:queue_delayed_work() 진행중

+ [66주차](http://bit.ly/1KsZanU) `2015.08.29`
 - [kmem_cache_init() 진행중]()
 - mm/migrate.c:__unmap_and_move() 진행중
 - __flush_tlb_page() 완료

+ [65주차](http://bit.ly/1LBKIIM) `2015.08.22`
 - [kmem_cache_init() 진행중]()
 - mm/migrate.c:__unmap_and_move() 진행중

+ [64주차]() `2015.08.15`
 - [kmem_cache_init() 진행중](http://bit.ly/1IQSM7s)
 - mm/migrate.c:__unmap_and_move() 진행중
 - TODO

+ [63주차]() `2015.08.08`
 - [kmem_cache_init() 진행중](http://bit.ly/1EfFUDJ)
 - mm/migrate.c:__unmap_and_move() 진행중
 - mm/memcontrol.c:mem_cgroup_prepare_migration() 진행중
 - mm/slub.c:kmem_cache_free() 진행중
+ [62주차]() `2015.07.25`
 - [kmem_cache_init() 진행중](http://bit.ly/1enUphX)
 - mm/migrate.c:__unmap_and_move() 진행중
 - get_new_page() 함수 분석완료
 - lock_page() 함수 분석 완료
 - 금주 회식으로 7(19)시 종료 및 차주 휴가
+ [61차]() `2015.07.18`
 - [kmem_cache_init() 진행중](http://bit.ly/1fT7MIE)
 - compaction.c:compact_zone() 내부 진행
 - compact_finished() 함수의 리턴값이 COMPACT_CONTINUE 일 때 while문 실행
 - isolate_migratepages() 함수 내부 분석중
 - isolate_migratepageblock() 함수 분석 진행
+ [60주차]() `2015.07.11`
 - [kmem_cache_init() 진행중](http://bit.ly/1UNGhju)
 - compaction.c:compact_zone() 내부 진행
 - compact_finished() 함수의 리턴값이 COMPACT_CONTINUE 일 때 while문 실행
 - isolate_migratepages() 함수 내부 분석중
 - putback_movable_pages() 완료
+ [59주차]() `2015.07.04`
 - [kmem_cache_init() 진행중](http://bit.ly/1NXKzjv)
 - compaction.c:compact_zone() 내부 진행
 - compact_finished() 함수의 리턴값이 COMPACT_CONTINUE 일 때 while문 실행
 - isolate_migratepages() 함수 내부 분석중
+ [58주차]() `2015.06.27`
 - [kmem_cache_init() 진행중](http://bit.ly/1JcHISc)
 - __alloc_pages_slowpath() 진행중
+ [57주차]() `2015.06.20` 
 - [kmem_cache_init() 진행중](http://bit.ly/1GykIvK)
 - __alloc_pages_slowpath() 진행중
+ [56주차]() `2015.06.13`
 - [kmem_cache_init() 진행중](http://bit.ly/1TiYgxA)
 - __alloc_pages_slowpath() 진행중
+ [55주차]() `2015.06.06` 
 - [kmem_cache_init() 진행중](http://bit.ly/1GqTZob)
 - kmem_cache_init() 진행중
+ [54주차]() `2015.05.30` 
 - [kmem_cache_init() 진행중](http://bit.ly/1Qhjo2P)
 - kmem_cache_init() 진행중
+ [53주차]() `2015.05.23` 
 - [kmem_cache_init() 진행중](https://goo.gl/EyFOvb)
 - kmem_cache_init() 진행중
+ [52주차]() `2015.05.16` 
 - [kmem_cache_init() 진행중](https://goo.gl/lVgK8v)
 - kmem_cache_init() 진행중
+ [51주차]() `2015.05.09` 
 - [mem_init() 진행중](https://goo.gl/BIS8O2)
 - mem_init() 진행중
+ [50주차]() `2015.05.02` 
 - [init/main.c](https://goo.gl/CWZolG)
 - parse_args(..., &unknown_bootoption) 
 - sort_main_extable()
 - vfs_caches_init_early()
+ [49주차]() `2015.04.25` 
 - [init/main.c](http://goo.gl/BSNhP3)
 - page_alloc_init() 완료
+ [48주차]() `2015.04.18` 
 - [page_alloc_init() 진행중](http://goo.gl/F3H2T6)
 - page_alloc_cpu_notify() 분석중
 - page_alloc_init() 함수에서는page_alloc_cpu_notify() 함수를 등록하는데 등록된 함수는 CPU가 종료될 때 호출되어 부팅단계에서 분석하기에는 어려움이 있어 중단함
+ [47주차]() `2015.04.11` 
 - [page_alloc_init() 진행중](http://goo.gl/J6axwE)
 - page_alloc_cpu_notify() 분석중
+ [46주차]() `2015.04.06` 
 - [page_alloc_init() 진행중]( http://goo.gl/90fl06)
 -  page_alloc_init() 함수에서는 page_alloc_cpu_notify() 함수를 등록
+ [45주차]() `2015.03.28` 
 - mm/page_alloc.c build_all_zonelists() 분석중
 - __build_all_zonelists()
 - mminit_verify_zonelist()
+ [44주차]() `2015.03.21`
 - [Something]( http://goo.gl/0E3rBp)
+ [43주차]() `2015.03.14`
 - [mm/percpu.c pcpu_embed_first_chunk()](http://goo.gl/520gnX)
 - setup_per_cpu_areas() 분석 중
+ [42주차]() `2015.03.07`
 - [init/main.c kernel/smp.c setup_nr_cpu_ids()]( http://goo.gl/o4RuKP)
 - init_mm, init_task 분석
+ [41주차]() `2015.02.28`
 - [init/main.c, arch/arm/kernel/setup.c]( http://goo.gl/cc433Q)
 - setup_arch() 분석 완료
+ [40주차]() `2015.02.14`
 - [arch/arm/kernel/setup.c, arch/arm/kernel/devtree.c](http://goo.gl/psYle3)
 - arm_dt_init_cpu_maps() 분석 완료
+ [39주차]() `2015.02.07`
 - [mm/flush.c:drivers/of/fdt.c](http://goo.gl/2KBzee)
 -  paging_init() 분석 완료
 - __unflatten_device_tree() 진행중
+ [38주차]() `2015.01.31` 
 - [mm/flush.c]( http://goo.gl/NWj838)
 -  __flush_dcache_page()
 - __flush_dcache_page()
+ [37주차]() `2015.01.24` 
 - [page_alloc.c \\ arm/mm/page_alloc.c](http://goo.gl/vDJUm7)
 - bootmem_init() 분석 완료
 - __flush_dcache_page() 진행중
+ [36주차]() `2015.01.17`
 - [mm/page_alloc.c](http://goo.gl/sJ3pBR)
 - free_area_init_core()
 - memmap_init_zone()
+ [35주차]() `2015.01.10`
 - [mm/page_alloc.c](http://goo.gl/mqFj8S)
 - free_area_init_core()
+ [34주차]() `2014.01.03`
 - [arch/arm/mm/init.c](http://goo.gl/CjjZ7e)
 - arm_bootmem_free() 진행중
 - sparse_init() 완료
+ [33주차]() `2014.12.27` 
 - [arch/arm/mm/init.c](http://goo.gl/CjjZ7e)
 - bootmem_init() 진행중
 - arm_memory_present() 완료 
+ [32주차]() `2014.12.20` 
 - [arch/arm/mm/init.c](http://goo.gl/9GOgQt)
 - bootmem_init() 진행중]] 
 - arm_memory_present() 진행중 |
+ [31주차]() `2014.12.13` 
 - [arch/arm/mm/init.c](http://goo.gl/ei1qZ7)
 - bootmem_init() 진행중
 - arm_bootmem_init() 완료
+ [27주차]() `2014.11.15`
 - [kernel/arch/arm/mm/mmu.c](http://goo.gl/5R3NNV)
 - arm_bootmem_init() 진행 중
+ [26주차]() `2014.11.08` 
 - [kernel/arch/arm/mm/mmu.c](http://goo.gl/RrJpgJ)
 - arm_bootmem_init() 진행 중
+ [25주차]() `2014.11.01` | 
 - [kernel/arch/arm/mm/mmu.c](http://goo.gl/UwIPp9)
  - bootmem_init() 진행중
+ [24주차]() `2014.10.27`
 - [kernel/arch/arm/mm/mmu.c](http://goo.gl/UwIPp9)
 - devicemaps_init 진행중
+ [23주차]() `2014.10.18`
 - [kernel/arch/arm/mm/mmu.c](http://goo.gl/4VHMHV)
 - init alloc_init_pmd() 진행중
+ [22주차](http://goo.gl/vqkScg) `2014.10.11`
 - [kernel/arch/arm/mm/mmu.c]( http://goo.gl/FNzZBW)
 - `init alloc_init_pmd()` 진행중
+ [21주차](http://goo.gl/vqkScg) `2014.09.20` 
 - [kernel/arch/arm/mm/mmu.c]( http://goo.gl/FNzZBW)|
 - ??() 진행중|
+ [20주차](http://goo.gl/vqkScg) `2014.09.13` 
 - [kernel/arch/arm/mm/mmu.c]( http://goo.gl/FNzZBW)
 - build_mem_type_table() 진행완료
 - paging_init() 진행중
+ [19주차](http://goo.gl/vqkScg) `2014.08.30` 
 - [kernel/init/main.c](http://goo.gl/FNzZBW)
 - setup_arch()진행중
 - arm_memblock_init() 진행중
+ [18주차](http://goo.gl/vqkScg) `2014.08.16` 
 - [kernel/init/main.c](http://goo.gl/FNzZBW)
 - setup_arch()진행중
 - arm_memblock_init() 진행중
+ [17주차](http://goo.gl/6pJIdQ) `2014.08.09` 
 - [kernel/init/main.c]()
 - setup_arch()진행중
 - sanity_check_meminfo() 완료
+ [16주차](http://goo.gl/fJZ6Ru) `2014.08.02` 
 - [kernel/init/main.c](http://goo.gl/o9olRn)
 - setup_arch()진행중
+ [15주차](http://goo.gl/p4FgfN) `2014.07.26`
 - [kernel/init/main.c](http://goo.gl/BIyVOX)
 - setup_arch()진행중
+ [14주차](http://goo.gl/pXxnbi) `2014.07.19`
 - kernel/init/main.c]()
 - kernel/init/main.c start_kernel()진행중
+ [13주차](http://goo.gl/iqqBge) `2014.07.12`
 - [kernel/init/main.c](http://goo.gl/QOMzXQ)
 - kernel/init/main.c start_kernel()진행중
+ [12주차](http://goo.gl/PC7oKz) `2014.07.05`
 - [arch/arm/kernel/head.s]( http://goo.gl/QOMzXQ)
 - arch/arm/kenel/head.s 분석완료
+ [11주차](http://goo.gl/ucH5T5) `2014.06.29`
 - [arch/arm/kernel/head.s]( http://goo.gl/9kRxgC)
 - arch/arm/kenel/head.s 분석중
 - __v7_setup전까지
+ [10주차](http://goo.gl/yywmQP) `2014.06.21`
 - [arch/arm/kernel/head.s]( http://goo.gl/5QKsfz)
 - arch/arm/kenel/head.s 분석중
 - __v7_setup 진행중
+ [09주차](http://goo.gl/hKYnCi) `2014.06.14` 
 - [arch/arm/kernel/head.s](http://goo.gl/fKSvYh)
 - kenel/head.s 분석중
 - Create_page_table전까지
+ [08주차](http://goo.gl/WPNekV) `2014.06.07` 
 - [arch/arm/boot/compressed/head.s](http://goo.gl/Sg6T0G)
 - compressed/head.s 분석 완료
+ [07주차](http://goo.gl/JWoShf) `2014.05.31` 
 - [arch/arm/boot/compressed/head.s](http://goo.gl/jSJpQK)
 - compressed/head.s 분석중]]|
+ [06주차](http://goo.gl/cYMWF8) `2014.05.24` 
 - arch/arm/boot/compressed/head.s 진행중
+ [05주차](http://goo.gl/gBNIqi) `2014.05.17` 
 - ARM System Developer's Guide 완료 
+ [04주차](http://goo.gl/wSKdl7) `2014.05.09`
 - ARM System Developer's Guide p385~428, p454~499
+ [03주차](http://goo.gl/KYLf6C) `2014.04.26`
 - ARM System Developer's Guide ch1,2,3, 5.4, 5.5, ch9-p385까지 진행 

# The Linux Kernel review for ARMv7 3.13.0 (exynos 5420)
* Community name: IAMROOT.ORG ARM kernel study 11th C team
* Target Soc    : Samsung Exynos 5420 (ARMv7 A7&A15)

> $ make kernelversion
>> 3.12.20

#### [IAMROOT 10차 C조](https://github.com/arm10c/linux-stable) 참조 ####
