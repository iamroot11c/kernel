/*
 *  linux/arch/arm/kernel/devtree.c
 *
 *  Copyright (C) 2009 Canonical Ltd. <jeremy.kerr@canonical.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/export.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/bootmem.h>
#include <linux/memblock.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>

#include <asm/cputype.h>
#include <asm/setup.h>
#include <asm/page.h>
#include <asm/smp_plat.h>
#include <asm/mach/arch.h>
#include <asm/mach-types.h>

void __init early_init_dt_add_memory_arch(u64 base, u64 size)
{
	arm_add_memory(base, size);
}

void * __init early_init_dt_alloc_memory_arch(u64 size, u64 align)
{
	return alloc_bootmem_align(size, align);
}

void __init arm_dt_memblock_reserve(void)
{
	u64 *reserve_map, base, size;

	// 우리는 dtb 를 사용하므로  initial_boot_params이 있다고 가정
	if (!initial_boot_params)
		return;

	/* Reserve the dtb region */
	//시스템이 big-endian 일 경우 #define __be32_to_cpu(x) ((__force __u32)(__be32)(x))
	//시스템이little-endian 일 경우#define __be32_to_cpu(x) __swab32((__force __u32)(__be32)(x))	
	// initial_boot_params 는 device tree의 시작주소
	// memory의 device tree 영역을 reserved 영역으로 add 
	memblock_reserve(virt_to_phys(initial_boot_params),
			 be32_to_cpu(initial_boot_params->totalsize));

	/*
	 * Process the reserve map.  This will probably overlap the initrd
	 * and dtb locations which are already reserved, but overlaping
	 * doesn't hurt anything
	 */
	// off_mem_rsvmap: offset to memory reserve map
	// reserve_map: reserve map의 시작주소
	// dtb의 reserve_map에서 base, size정보를 읽어와서 memblock에 add 
	reserve_map = ((void*)initial_boot_params) +
			be32_to_cpu(initial_boot_params->off_mem_rsvmap);
	while (1) {
		base = be64_to_cpup(reserve_map++);
		size = be64_to_cpup(reserve_map++);
		if (!size)
			break;
		memblock_reserve(base, size);
	}
}

/*
 * arm_dt_init_cpu_maps - Function retrieves cpu nodes from the device tree
 * and builds the cpu logical map array containing MPIDR values related to
 * logical cpus
 *
 * Updates the cpu possible mask with the number of parsed cpu nodes
 */
// 2015-02-14; 시작
void __init arm_dt_init_cpu_maps(void)
{
	/*
	 * Temp logical map is initialized with UINT_MAX values that are
	 * considered invalid logical map entries since the logical map must
	 * contain a list of MPIDR[23:0] values where MPIDR[31:24] must
	 * read as 0.
	 */
	struct device_node *cpu, *cpus;
	u32 i, j, cpuidx = 1;
	u32 mpidr = is_smp() ? read_cpuid_mpidr() & MPIDR_HWID_BITMASK : 0;

	// 배열의 각 각의 원소를 MPIDR_INVALID 값으로 초기화
	// GNU C language document 2.5.2 Initializing Arrays
	// (http://www.gnu.org/software/gnu-c-manual/gnu-c-manual.html#Initializing-Arrays)
	// GNU 확장기능으로 배열 초기목록에서만 사용
	u32 tmp_map[NR_CPUS] = { [0 ... NR_CPUS-1] = MPIDR_INVALID };
	bool bootcpu_valid = false;
	cpus = of_find_node_by_path("/cpus");

	if (!cpus)
		return;

	for_each_child_of_node(cpus, cpu) {
		u32 hwid;

		if (of_node_cmp(cpu->type, "cpu"))
			continue;

		pr_debug(" * %s...\n", cpu->full_name);
		/*
		 * A device tree containing CPU nodes with missing "reg"
		 * properties is considered invalid to build the
		 * cpu_logical_map.
		 */
		if (of_property_read_u32(cpu, "reg", &hwid)) {
			pr_debug(" * %s missing reg property\n",
				     cpu->full_name);
			return;
		}

		/*
		 * 8 MSBs must be set to 0 in the DT since the reg property
		 * defines the MPIDR[23:0].
		 */
		if (hwid & ~MPIDR_HWID_BITMASK)
			return;

		/*
		 * Duplicate MPIDRs are a recipe for disaster.
		 * Scan all initialized entries and check for
		 * duplicates. If any is found just bail out.
		 * temp values were initialized to UINT_MAX
		 * to avoid matching valid MPIDR[23:0] values.
		 */
		for (j = 0; j < cpuidx; j++)
			if (WARN(tmp_map[j] == hwid, "Duplicate /cpu reg "
						     "properties in the DT\n"))
				return;

		/*
		 * Build a stashed array of MPIDR values. Numbering scheme
		 * requires that if detected the boot CPU must be assigned
		 * logical id 0. Other CPUs get sequential indexes starting
		 * from 1. If a CPU node with a reg property matching the
		 * boot CPU MPIDR is detected, this is recorded so that the
		 * logical map built from DT is validated and can be used
		 * to override the map created in smp_setup_processor_id().
		 */
		if (hwid == mpidr) {
			i = 0;
			bootcpu_valid = true;
		} else {
			i = cpuidx++;
		}

		if (WARN(cpuidx > nr_cpu_ids, "DT /cpu %u nodes greater than "
					       "max cores %u, capping them\n",
					       cpuidx, nr_cpu_ids)) {
			cpuidx = nr_cpu_ids;
			break;
		}

		tmp_map[i] = hwid;
	}

	if (!bootcpu_valid) {
		pr_warn("DT missing boot CPU MPIDR[23:0], fall back to default cpu_logical_map\n");
		return;
	}

	/*
	 * Since the boot CPU node contains proper data, and all nodes have
	 * a reg property, the DT CPU list can be considered valid and the
	 * logical map created in smp_setup_processor_id() can be overridden
	 */
	for (i = 0; i < cpuidx; i++) {
		set_cpu_possible(i, true);
		cpu_logical_map(i) = tmp_map[i];
		pr_debug("cpu logical map 0x%x\n", cpu_logical_map(i));
	}
}

bool arch_match_cpu_phys_id(int cpu, u64 phys_id)
{
	return phys_id == cpu_logical_map(cpu);
}

/**
 * setup_machine_fdt - Machine setup when an dtb was passed to the kernel
 * @dt_phys: physical address of dt blob
 *
 * If a dtb was passed to the kernel in r2, then use it to choose the
 * correct machine_desc and to setup the system.
 */
//http://www.iamroot.org/xe/Kernel_10_ARM/184583
const struct machine_desc * __init setup_machine_fdt(unsigned int dt_phys)
{
	struct boot_param_header *devtree;
	const struct machine_desc *mdesc, *mdesc_best = NULL;
	unsigned int score, mdesc_score = ~1;
	unsigned long dt_root;
	const char *model;

#ifdef CONFIG_ARCH_MULTIPLATFORM	//not set
	DT_MACHINE_START(GENERIC_DT, "Generic DT based system")
	MACHINE_END

	mdesc_best = &__mach_desc_GENERIC_DT;
#endif

	if (!dt_phys)
		return NULL;

	//atgs_pointer를 가상주소로 변환해서 devtrr에 저장
	//devtree 는 atags_pointer의 가상주소임을 기억
	devtree = phys_to_virt(dt_phys);

	/*
	 * xxd arch/arm/boot/dts/exynos5420-smdk5420.dtb 로 열어보면
	 * 0번지에 0xd00dfeed로 저장되있고 이는 big endian형식이다.
	 * */
	/* check device tree validity */
	if (be32_to_cpu(devtree->magic) != OF_DT_HEADER)
		return NULL;

	/* Search the mdescs for the 'best' compatible value match */
	initial_boot_params = devtree;
	/*dt_root = 0x40(편의상 dtb파일의 offset 물리주소로 썻지만 커널상에서는 가상주소임을 기억)*/
	/*
	 * dt_root_node 의 어드레스를 가져옴.
	 * */
	dt_root = of_get_flat_dt_root();
	/*
	 * __arch_info_begin : vmlinux.lds.S에서 확인 가능. 해당 섹션은.

	#define DT_MACHINE_START(_name, _namestr)  
	에서 사용.
	DT_MACHINE_START는 mach-exynos5-dt.c에서 다음과 같이 사용된다.
	DT_MACHINE_START(EXYNOS5_DT, "SAMSUNG EXYNOS5 (Flattened Device Tree)")
	 * */
	for_each_machine_desc(mdesc) {
		/*
		 * 커널상에 등록된 machine compat들과 dtb root node의 compat 일치비교 검사. 존재하면
		 *  dtb에 등록된 compat의 offset(score)을 저장
		 * */
		score = of_flat_dt_match(dt_root, mdesc->dt_compat);
		/*
		 * 커널에 등록된 mdesc중에 score가 가장 낮게 책정된것을 best
		 * 로서 저장한다. 지금은은 mdesc_best = mach-exynos5-dt의 
		 * samsung,exynos5420을 기준으로하여 score = 2라고 가정
		 * */
		if (score > 0 && score < mdesc_score) {
			mdesc_best = mdesc;
			mdesc_score = score;
		}
	}
	if (!mdesc_best) {
		const char *prop;
		long size;

		early_print("\nError: unrecognized/unsupported "
			    "device tree compatible list:\n[ ");

		prop = of_get_flat_dt_prop(dt_root, "compatible", &size);
		while (size > 0) {
			early_print("'%s' ", prop);
			size -= strlen(prop) + 1;
			prop += strlen(prop) + 1;
		}
		early_print("]\n\n");

		dump_machine_table(); /* does not return */
	}

	// compatible일때와 동일하게 분석하면 다음과 같이 나옴을 알 수 있다.
	// model = 0xac
	model = of_get_flat_dt_prop(dt_root, "model", NULL);
	if (!model)
		model = of_get_flat_dt_prop(dt_root, "compatible", NULL);
	if (!model)
		model = "<unknown>";
	/*
	 * 찾아가보면 다음과 같이 나올것임을 예측할수 있다. 따로 등록은
	 * 안하고 부팅시 print만 함
	 * mdesc_best->name : SAMSUNG exynos5 (Flattened Device Tree)
	 * model : Samsung SMDK5420 Board Based on EXYNOS5420
	 */
	pr_info("Machine: %s, model: %s\n", mdesc_best->name, model);

	/* Retrieve various information from the /chosen node */
	/*
	 * 우리거는 bootargs 밖에없다
	 * dtb에 저장된 bootargs가 있는지 검사 및 kernel의 CMDLINE을 쓸지
	 * bootargs를 쓸지를 결정
	 */
	of_scan_flat_dt(early_init_dt_scan_chosen, boot_command_line);
	/* Initialize {size,address}-cells info */
	/*
	 * root node에 있는 machine 메모리영역의 base addr, size의 크기가
	 * 32bit인지 64bit인지 저장되있는데 그걸 가져옴
	 */
	of_scan_flat_dt(early_init_dt_scan_root, NULL);
	/* Setup memory, calling early_init_dt_add_memory_arch */
	/*
	 * 위에서 가져온 정보+memory 노드검색, memory base addr, size를 
	 * dtb에서 가져와 커널에 등록
	 * */
	of_scan_flat_dt(early_init_dt_scan_memory, NULL);

	/* Change machine number to match the mdesc we're using */
	//mdesc_best->nr = ~0
	__machine_arch_type = mdesc_best->nr;

	return mdesc_best;
}
