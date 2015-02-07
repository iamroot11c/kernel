/*
 * Functions for working with the Flattened Device Tree data format
 *
 * Copyright 2009 Benjamin Herrenschmidt, IBM Corp
 * benh@kernel.crashing.org
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/initrd.h>
#include <linux/memblock.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/slab.h>

#include <asm/setup.h>  /* for COMMAND_LINE_SIZE */
#ifdef CONFIG_PPC
#include <asm/machdep.h>
#endif /* CONFIG_PPC */

#include <asm/page.h>

char *of_fdt_get_string(struct boot_param_header *blob, u32 offset)
{
	return ((char *)blob) +
		be32_to_cpu(blob->off_dt_strings) + offset;
}

/**
 * of_fdt_get_property - Given a node in the given flat blob, return
 * the property ptr
 */
void *of_fdt_get_property(struct boot_param_header *blob,
		       unsigned long node, const char *name,
		       unsigned long *size)
{
	unsigned long p = node;

	do {
		//이 함수를 처음 사용하는 시점의 node는 0x40에서 시작
		u32 tag = be32_to_cpup((__be32 *)p);
		u32 sz, noff;
		const char *nstr;
		p += 4;
	/*
	 * tag가 NOP이면 +4bytes후 다시 검사,tag가 property(0x3)인지를 확인.
	 * dtb를 보면 0x40, 0x50, 0x60, 0x70이 property 값인것을 확인가능
	 * 앞으로 코드를 볼때 0x40~4f, 0x50~5f, 0x60~6f, 0x70~7f를 묶어서 본다
	*/
		if (tag == OF_DT_NOP)
			continue;
		if (tag != OF_DT_PROP)
			return NULL;
	 /*이전에 p+4를 했다. 이 4바이트는 sz를, 그 다음 4바이트는 name offset을  의미한다.
	  * sz는 0x44 = 4, 0x54 = 4, 0x64 = 4, 0x74 = 4
	  * name off는 0x48 = 0, 0x58 = f, 0x68 = 1b, 0x78 = 2c
	  * */
		sz = be32_to_cpup((__be32 *)p);
		noff = be32_to_cpup((__be32 *)(p + 4));
		/* 
		 * 여기까지 봤을때, root node이후로의 구조는 다음과 같다
		 * root node이후에 byte배치는
		 * 0x38 : begin -> root node(0x1) -> null ->
		 * 0x40 : tag(property) -> sz(0x4) -> noff(0x0) -> property(0x1) ->
		 * 0x50 : tag(property) -> sz(0x4) -> noff(0xf) -> property(0x1) ->
		 * 0x60 : tag(property) -> sz(0x4) -> noff(0x1b) -> property(0x1)->
		 * 0x70 : tag(property) -> sz(0x24) -> noff(0x1b) -> 
		 *		samsung,smdk5420.samsung,exynos5420(0x24크기의 문자열)
		 * 0xa0 : property -> sz(0x2b) -> noff(0x37) -> ....
		 * 추측컨데 property 이후의 sz는 해당 property의
		 * size를 나타내며, noff는 해당 property의 name이 저장되있는
		 * 곳을 의미함을 알수있고,
		 * property의 name은 base주소인 off_dt_strings에 noff를 
		 * 더함으로써 계산이 된다. 
		 * 즉 함수의 목적은 인자 name에 맞는 property를 찾아
		 * property가 저장된 pointer와 그 size(length)를 반환하는것이다.
		 */
		p += 8;
		//우리껀 0x11이다. 그전에껀 align해줘야되나 보다.
		if (be32_to_cpu(blob->version) < 0x10)
			p = ALIGN(p, sz >= 8 ? 8 : 4);
		/*
		 * off_dt_strings + noff 값을 반환한다.
		 * 0x4X 인경우 39b0. nstr = #address-cells
		 * 0x5X 는 39bf. nstr = #size-cells
		 * 0x6X 는 39cb. nstr = interrupt-parent
		 * 0x7X 는 39dc, nstr = compatilbe
		 * */
		nstr = of_fdt_get_string(blob, noff);
		if (nstr == NULL) {
			pr_warning("Can't find property index name !\n");
			return NULL;
		}
		if (strcmp(name, nstr) == 0) {
			if (size)
				*size = sz;
			/*size에 name size를 저장하고, 포인터를 넘긴다.
			 * 위치는 0x7c이며 samusng, smdk.. 가 시작하는부분
			 */
			return (void *)p;
		}
		p += sz;
		p = ALIGN(p, 4);
	} while (1);
}

/**
 * of_fdt_is_compatible - Return true if given node from the given blob has
 * compat in its compatible list
 * @blob: A device tree blob
 * @node: node to test
 * @compat: compatible string to compare with compatible list.
 *
 * On match, returns a non-zero value with smaller values returned for more
 * specific compatible values.
 */
int of_fdt_is_compatible(struct boot_param_header *blob,
		      unsigned long node, const char *compat)
{
	const char *cp;
	unsigned long cplen, l, score = 0;

	/*
	 * name(compatible)에 property와 property size(length) 찾아서 반환. 
	 * cplen : 24, cp : 7c
	 */
	cp = of_fdt_get_property(blob, node, "compatible", &cplen);
	if (cp == NULL)
		return 0;
	while (cplen > 0) {
		/*
		 * mdesc->dt_compat와 cp와 비교한다. 
		 * cp = "samsung,smdk5420(NULL)samsung,exynos5420"
		 * 이런 모양이므로 score = 2;
		 * (현재 mach-exynos5420-dt.c를 기준으로 하고있음.
		 * mdesc->dt_compat이 samsung,smdk5420이라면 score = 1일것이다.)
		 */
		score++;
		if (of_compat_cmp(cp, compat, strlen(compat)) == 0)
			return score;
		l = strlen(cp) + 1;
		cp += l;
		cplen -= l;
	}

	return 0;
}

/**
 * of_fdt_match - Return true if node matches a list of compatible values
 */
/*
 * blob = dtb시작주소, dtb파일기준으로는 0x0.(이제부터 여기서쓰는 dtb 주소는 다 파일기준으로 정의)
 * node = of_dt_begin_node바로 root node주소 0x40
 * compat = mdesc->compat에서 가져온 string
 * */
int of_fdt_match(struct boot_param_header *blob, unsigned long node,
                 const char *const *compat)
{
	unsigned int tmp, score = 0;

	if (!compat)
		return 0;

	while (*compat) {
		/*
		 * mdesc->dt_compat = { "samsung,exynos5250",
		 *	"samsung,exynos5420","samsung,exynos5440"}
		 *	에서 compat++하면서 위 3개가 있는지 검사.
		 *	반환된 score(tmp)중에 가장 작은것을 적용.
		 * */
		tmp = of_fdt_is_compatible(blob, node, *compat);
		if (tmp && (score == 0 || (tmp < score)))
			score = tmp;
		compat++;
	}

	return score;
}

static void *unflatten_dt_alloc(void **mem, unsigned long size,
				       unsigned long align)
{
	void *res;

	*mem = PTR_ALIGN(*mem, align);
	res = *mem;
	*mem += size;

	return res;
}

/**
 * unflatten_dt_node - Alloc and populate a device_node from the flat tree
 * @blob: The parent device tree blob
 * @mem: Memory chunk to use for allocating device nodes and properties
 * @p: pointer to node in flat tree
 * @dad: Parent struct device_node
 * @allnextpp: pointer to ->allnext from last allocated device_node
 * @fpsize: Size of the node path up at the current depth.
 */
// 2015-02-07, 흝어봄
// unflatten_dt_node(blob, 0, &start, NULL, NULL, 0)
static void * unflatten_dt_node(struct boot_param_header *blob,
				void *mem,
				void **p,
				struct device_node *dad,
				struct device_node ***allnextpp,
				unsigned long fpsize)
{
	struct device_node *np;
	struct property *pp, **prev_pp = NULL;
	char *pathp;
	u32 tag;
	unsigned int l, allocl;
	int has_name = 0;
	int new_format = 0;

	tag = be32_to_cpup(*p);
	if (tag != OF_DT_BEGIN_NODE) {
		pr_err("Weird tag at start of node: %x\n", tag);
		return mem;
	}
	*p += 4;
	pathp = *p;
	l = allocl = strlen(pathp) + 1;
	*p = PTR_ALIGN(*p + l, 4);

	/* version 0x10 has a more compact unit name here instead of the full
	 * path. we accumulate the full path size using "fpsize", we'll rebuild
	 * it later. We detect this because the first character of the name is
	 * not '/'.
	 */
	if ((*pathp) != '/') {
		new_format = 1;
		if (fpsize == 0) {
			/* root node: special case. fpsize accounts for path
			 * plus terminating zero. root node only has '/', so
			 * fpsize should be 2, but we want to avoid the first
			 * level nodes to have two '/' so we use fpsize 1 here
			 */
			fpsize = 1;
			allocl = 2;
			l = 1;
			*pathp = '\0';
		} else {
			/* account for '/' and path size minus terminal 0
			 * already in 'l'
			 */
			fpsize += l;
			allocl = fpsize;
		}
	}

	np = unflatten_dt_alloc(&mem, sizeof(struct device_node) + allocl,
				__alignof__(struct device_node));
	if (allnextpp) {
		char *fn;
		np->full_name = fn = ((char *)np) + sizeof(*np);
		if (new_format) {
			/* rebuild full path for new format */
			if (dad && dad->parent) {
				strcpy(fn, dad->full_name);
#ifdef DEBUG
				if ((strlen(fn) + l + 1) != allocl) {
					pr_debug("%s: p: %d, l: %d, a: %d\n",
						pathp, (int)strlen(fn),
						l, allocl);
				}
#endif
				fn += strlen(fn);
			}
			*(fn++) = '/';
		}
		memcpy(fn, pathp, l);

		prev_pp = &np->properties;
		**allnextpp = np;
		*allnextpp = &np->allnext;
		if (dad != NULL) {
			np->parent = dad;
			/* we temporarily use the next field as `last_child'*/
			if (dad->next == NULL)
				dad->child = np;
			else
				dad->next->sibling = np;
			dad->next = np;
		}
		kref_init(&np->kref);
	}
	/* process properties */
	while (1) {
		u32 sz, noff;
		char *pname;

		tag = be32_to_cpup(*p);
		if (tag == OF_DT_NOP) {
			*p += 4;
			continue;
		}
		if (tag != OF_DT_PROP)
			break;
		*p += 4;
		sz = be32_to_cpup(*p);
		noff = be32_to_cpup(*p + 4);
		*p += 8;
		if (be32_to_cpu(blob->version) < 0x10)
			*p = PTR_ALIGN(*p, sz >= 8 ? 8 : 4);

		pname = of_fdt_get_string(blob, noff);
		if (pname == NULL) {
			pr_info("Can't find property name in list !\n");
			break;
		}
		if (strcmp(pname, "name") == 0)
			has_name = 1;
		l = strlen(pname) + 1;
		pp = unflatten_dt_alloc(&mem, sizeof(struct property),
					__alignof__(struct property));
		if (allnextpp) {
			/* We accept flattened tree phandles either in
			 * ePAPR-style "phandle" properties, or the
			 * legacy "linux,phandle" properties.  If both
			 * appear and have different values, things
			 * will get weird.  Don't do that. */
			if ((strcmp(pname, "phandle") == 0) ||
			    (strcmp(pname, "linux,phandle") == 0)) {
				if (np->phandle == 0)
					np->phandle = be32_to_cpup((__be32*)*p);
			}
			/* And we process the "ibm,phandle" property
			 * used in pSeries dynamic device tree
			 * stuff */
			if (strcmp(pname, "ibm,phandle") == 0)
				np->phandle = be32_to_cpup((__be32 *)*p);
			pp->name = pname;
			pp->length = sz;
			pp->value = *p;
			*prev_pp = pp;
			prev_pp = &pp->next;
		}
		*p = PTR_ALIGN((*p) + sz, 4);
	}
	/* with version 0x10 we may not have the name property, recreate
	 * it here from the unit name if absent
	 */
	if (!has_name) {
		char *p1 = pathp, *ps = pathp, *pa = NULL;
		int sz;

		while (*p1) {
			if ((*p1) == '@')
				pa = p1;
			if ((*p1) == '/')
				ps = p1 + 1;
			p1++;
		}
		if (pa < ps)
			pa = p1;
		sz = (pa - ps) + 1;
		pp = unflatten_dt_alloc(&mem, sizeof(struct property) + sz,
					__alignof__(struct property));
		if (allnextpp) {
			pp->name = "name";
			pp->length = sz;
			pp->value = pp + 1;
			*prev_pp = pp;
			prev_pp = &pp->next;
			memcpy(pp->value, ps, sz - 1);
			((char *)pp->value)[sz - 1] = 0;
			pr_debug("fixed up name for %s -> %s\n", pathp,
				(char *)pp->value);
		}
	}
	if (allnextpp) {
		*prev_pp = NULL;
		np->name = of_get_property(np, "name", NULL);
		np->type = of_get_property(np, "device_type", NULL);

		if (!np->name)
			np->name = "<NULL>";
		if (!np->type)
			np->type = "<NULL>";
	}
	while (tag == OF_DT_BEGIN_NODE || tag == OF_DT_NOP) {
		if (tag == OF_DT_NOP)
			*p += 4;
		else
			mem = unflatten_dt_node(blob, mem, p, np, allnextpp,
						fpsize);
		tag = be32_to_cpup(*p);
	}
	if (tag != OF_DT_END_NODE) {
		pr_err("Weird tag at end of node: %x\n", tag);
		return mem;
	}
	*p += 4;
	return mem;
}

/**
 * __unflatten_device_tree - create tree of device_nodes from flat blob
 *
 * unflattens a device-tree, creating the
 * tree of struct device_node. It also fills the "name" and "type"
 * pointers of the nodes so the normal device-tree walking functions
 * can be used.
 * @blob: The blob to expand
 * @mynodes: The device_node tree created by the call
 * @dt_alloc: An allocator that provides a virtual address to memory
 * for the resulting tree
 */
// 2015-02-07, 시작
// __unflatten_device_tree(initial_boot_params, &of_allnodes,
//                               early_init_dt_alloc_memory_arch);
//
static void __unflatten_device_tree(struct boot_param_header *blob,
			     struct device_node **mynodes,
			     void * (*dt_alloc)(u64 size, u64 align))
{
	unsigned long size;
	void *start, *mem;
	struct device_node **allnextp = mynodes;

	pr_debug(" -> unflatten_device_tree()\n");

	if (!blob) {
		pr_debug("No device tree pointer\n");
		return;
	}

	pr_debug("Unflattening device tree:\n");
	pr_debug("magic: %08x\n", be32_to_cpu(blob->magic));
	pr_debug("size: %08x\n", be32_to_cpu(blob->totalsize));
	pr_debug("version: %08x\n", be32_to_cpu(blob->version));

	// little endian으로 정렬후 트리의 헤더(루트)인지 조사
	if (be32_to_cpu(blob->magic) != OF_DT_HEADER) {
		pr_err("Invalid device tree blob header\n");
		return;
	}

	/* First pass, scan for size */
	start = ((void *)blob) + be32_to_cpu(blob->off_dt_struct);
	// unflatten_dt_node() 함수는 분석하지 않고 흝어봄
	// 다음주에 분석할 예정
	size = (unsigned long)unflatten_dt_node(blob, 0, &start, NULL, NULL, 0);
	size = ALIGN(size, 4);

	pr_debug("  size is %lx, allocating...\n", size);

	/* Allocate memory for the expanded device tree */
	// unflatten_dt_node() 함수에서 구한 size만큼 할당하되
	// struct device_node 크기로 정렬된 가상주소를 구함 
	// __ALIGNOF__(type)은 type 형식에 대한 정렬 요구 사항을 반환하거나,
	// 정렬 요구 사항이 없으면 1을 반환합니다.
	// 참고: http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0348bk/BABBDFAF.html
	mem = dt_alloc(size + 4, __alignof__(struct device_node));
	memset(mem, 0, size);

	*(__be32 *)(mem + size) = cpu_to_be32(0xdeadbeef);
	// 2015-02-07, 여기까지

	pr_debug("  unflattening %p...\n", mem);

	/* Second pass, do actual unflattening */
	start = ((void *)blob) + be32_to_cpu(blob->off_dt_struct);
	unflatten_dt_node(blob, mem, &start, NULL, &allnextp, 0);
	if (be32_to_cpup(start) != OF_DT_END)
		pr_warning("Weird tag at end of tree: %08x\n", be32_to_cpup(start));
	if (be32_to_cpup(mem + size) != 0xdeadbeef)
		pr_warning("End of tree marker overwritten: %08x\n",
			   be32_to_cpup(mem + size));
	*allnextp = NULL;

	pr_debug(" <- unflatten_device_tree()\n");
}

static void *kernel_tree_alloc(u64 size, u64 align)
{
	return kzalloc(size, GFP_KERNEL);
}

/**
 * of_fdt_unflatten_tree - create tree of device_nodes from flat blob
 *
 * unflattens the device-tree passed by the firmware, creating the
 * tree of struct device_node. It also fills the "name" and "type"
 * pointers of the nodes so the normal device-tree walking functions
 * can be used.
 */
void of_fdt_unflatten_tree(unsigned long *blob,
			struct device_node **mynodes)
{
	struct boot_param_header *device_tree =
		(struct boot_param_header *)blob;
	__unflatten_device_tree(device_tree, mynodes, &kernel_tree_alloc);
}
EXPORT_SYMBOL_GPL(of_fdt_unflatten_tree);

/* Everything below here references initial_boot_params directly. */
int __initdata dt_root_addr_cells;
int __initdata dt_root_size_cells;
 
struct boot_param_header *initial_boot_params;

#ifdef CONFIG_OF_EARLY_FLATTREE

/**
 * of_scan_flat_dt - scan flattened tree blob and call callback on each.
 * @it: callback function
 * @data: context data pointer
 *
 * This function is used to scan the flattened device-tree, it is
 * used to extract the memory information at boot before we can
 * unflatten the tree
 */
/*
 of_scan_flat_dt(early_init_dt_scan_chosen, boot_command_line);
 of_scan_flat_dt(early_init_dt_scan_root, NULL);
 of_scan_flat_dt(early_init_dt_scan_memory, NULL);
 
 dtb전체의 node를 스캔해서 인자로 넘어온 함수로 넘긴다.
 root_node부터 de_end까지 tag를 처리.
 3번있으니 3바퀴를 도는것.
*/
int __init of_scan_flat_dt(int (*it)(unsigned long node,
				     const char *uname, int depth,
				     void *data),
			   void *data)
{
	unsigned long p = ((unsigned long)initial_boot_params) +
		be32_to_cpu(initial_boot_params->off_dt_struct);
	int rc = 0;
	int depth = -1;

	//root node부터 시작
	do {
		u32 tag = be32_to_cpup((__be32 *)p);
		const char *pathp;

		p += 4;
		/*
		 * OF_DT_BEGIN_NODE가 아닌 tag들은 전부 처리해 넘김
		 * */
		//0x2
		if (tag == OF_DT_END_NODE) {
			depth--;
			continue;
		}
		//0x4
		if (tag == OF_DT_NOP)
			continue;
		//0x9
		if (tag == OF_DT_END)
			break;
		if (tag == OF_DT_PROP) {
			u32 sz = be32_to_cpup((__be32 *)p);
			//sz만 가져오고 offset은 안씀. 넘기만 하면되기때문
			p += 8;
			if (be32_to_cpu(initial_boot_params->version) < 0x10)
				p = ALIGN(p, sz >= 8 ? 8 : 4);
			p += sz;
			p = ALIGN(p, 4);
			continue;
		}
		//0x1
		if (tag != OF_DT_BEGIN_NODE) {
			pr_err("Invalid tag %x in flat device tree!\n", tag);
			return -EINVAL;
		}
		depth++;
		/*
		 * OF_DT_BEGIN_NODE 바로 앞의 값을 사용함을 확인.
		 * chosen, aliases, memory, chipid@10000000, inter...
		 * 같은 문자열들이 존재함. <--이게 node name이다.
		 */
		pathp = (char *)p;
		p = ALIGN(p + strlen(pathp) + 1, 4);
		/*
		 * '/'가 맨처음에 위치했을때, 뒤에 '/'가 더 있는지를  검사.
		 * 맨처음에만 위치한다면  '/'의 다음 byte주소 값을, 아니면 그냥 넘김
		 * ex) pathp = "/abc" -> pathp+1 반환
		 *     pathp = "/ab/cd../../" -> pathp 반환
		 * 아마 dtb의 문법상 /이 먼저 오는경우에 대해 처리하는법인듯
		 * */
		if (*pathp == '/')
			pathp = kbasename(pathp);
		//early_init_dt_scan_chosen,만 data를 boot_command_line를 가져가고 나머진 NULL 
		rc = it(p, pathp, depth, data);
		if (rc != 0)
			break;
	} while (1);

	return rc;
}

/**
 * of_get_flat_dt_root - find the root node in the flat blob
 */
unsigned long __init of_get_flat_dt_root(void)
{
	/*
	 * xxd .dtb로 보면서 진행. off_dt_struct = 0x38이다
	 * */
	unsigned long p = ((unsigned long)initial_boot_params) +
		be32_to_cpu(initial_boot_params->off_dt_struct);
	/*
	 * be32_to_cpup : dtb는 기본으로 big endian이기때문에 cpu에 따라 변환하기 위함
	 * */
	while (be32_to_cpup((__be32 *)p) == OF_DT_NOP)
		p += 4;
	/*
	 * 바로 0x01이 없으면 BUG_ON을 한다. *0x38 = 0x01을 확인하고
	 * 0x38 + 4 = 0x3C가된다.
	 * */
	BUG_ON(be32_to_cpup((__be32 *)p) != OF_DT_BEGIN_NODE);
	p += 4;
	/* 0x3C의 값을 보면 0x0000이다. 이는 dtb에서 root node는 null string
	 * 이나 '/'이 오게 되있다고 한다. 또한 ALGIN을 한 이유는 코드의 일관성,
	 * 및 잘못된 dtb에 대한 오류를 방지하기 위함이라고 한다.
	 * return 0x40이 된다.
	 * */
	return ALIGN(p + strlen((char *)p) + 1, 4);
}

/**
 * of_get_flat_dt_prop - Given a node in the flat blob, return the property ptr
 *
 * This function can be used within scan_flattened_dt callback to get
 * access to properties
 */
void *__init of_get_flat_dt_prop(unsigned long node, const char *name,
				 unsigned long *size)
{
	return of_fdt_get_property(initial_boot_params, node, name, size);
}

/**
 * of_flat_dt_is_compatible - Return true if given node has compat in compatible list
 * @node: node to test
 * @compat: compatible string to compare with compatible list.
 */
int __init of_flat_dt_is_compatible(unsigned long node, const char *compat)
{
	return of_fdt_is_compatible(initial_boot_params, node, compat);
}

/**
 * of_flat_dt_match - Return true if node matches a list of compatible values
 */
int __init of_flat_dt_match(unsigned long node, const char *const *compat)
{
	return of_fdt_match(initial_boot_params, node, compat);
}

struct fdt_scan_status {
	const char *name;
	int namelen;
	int depth;
	int found;
	int (*iterator)(unsigned long node, const char *uname, int depth, void *data);
	void *data;
};

/**
 * fdt_scan_node_by_path - iterator for of_scan_flat_dt_by_path function
 */
static int __init fdt_scan_node_by_path(unsigned long node, const char *uname,
					int depth, void *data)
{
	struct fdt_scan_status *st = data;

	/*
	 * if scan at the requested fdt node has been completed,
	 * return -ENXIO to abort further scanning
	 */
	if (depth <= st->depth)
		return -ENXIO;

	/* requested fdt node has been found, so call iterator function */
	if (st->found)
		return st->iterator(node, uname, depth, st->data);

	/* check if scanning automata is entering next level of fdt nodes */
	if (depth == st->depth + 1 &&
	    strncmp(st->name, uname, st->namelen) == 0 &&
	    uname[st->namelen] == 0) {
		st->depth += 1;
		if (st->name[st->namelen] == 0) {
			st->found = 1;
		} else {
			const char *next = st->name + st->namelen + 1;
			st->name = next;
			st->namelen = strcspn(next, "/");
		}
		return 0;
	}

	/* scan next fdt node */
	return 0;
}

/**
 * of_scan_flat_dt_by_path - scan flattened tree blob and call callback on each
 *			     child of the given path.
 * @path: path to start searching for children
 * @it: callback function
 * @data: context data pointer
 *
 * This function is used to scan the flattened device-tree starting from the
 * node given by path. It is used to extract information (like reserved
 * memory), which is required on ealy boot before we can unflatten the tree.
 */
int __init of_scan_flat_dt_by_path(const char *path,
	int (*it)(unsigned long node, const char *name, int depth, void *data),
	void *data)
{
	struct fdt_scan_status st = {path, 0, -1, 0, it, data};
	int ret = 0;

	if (initial_boot_params)
                ret = of_scan_flat_dt(fdt_scan_node_by_path, &st);

	if (!st.found)
		return -ENOENT;
	else if (ret == -ENXIO)	/* scan has been completed */
		return 0;
	else
		return ret;
}

#ifdef CONFIG_BLK_DEV_INITRD //set
/**
 * early_init_dt_check_for_initrd - Decode initrd location from flat tree
 * @node: reference to node containing initrd location ('chosen')
 */
void __init early_init_dt_check_for_initrd(unsigned long node)
{
	u64 start, end;
	unsigned long len;
	__be32 *prop;

	pr_debug("Looking for initrd properties... ");

	//없다
	prop = of_get_flat_dt_prop(node, "linux,initrd-start", &len);
	if (!prop)
		return;
	start = of_read_number(prop, len/4);

	//없다
	prop = of_get_flat_dt_prop(node, "linux,initrd-end", &len);
	if (!prop)
		return;
	end = of_read_number(prop, len/4);

	early_init_dt_setup_initrd_arch(start, end);
	pr_debug("initrd_start=0x%llx  initrd_end=0x%llx\n",
		 (unsigned long long)start, (unsigned long long)end);
}
#else
inline void early_init_dt_check_for_initrd(unsigned long node)
{
	//위에꺼 입니다.
}
#endif /* CONFIG_BLK_DEV_INITRD */

/**
 * early_init_dt_scan_root - fetch the top level address and size cells
 */
int __init early_init_dt_scan_root(unsigned long node, const char *uname,
				   int depth, void *data)
{
	__be32 *prop;

	/* depth가 0인경우만을 넘기는것을 확인.
	 * #size-cells과 #address-cells은 depth 여기저기 있지만
	 * depth가 0인경우는 맨처음 root node일때 한번임
	 */
	if (depth != 0)
		return 0;

	//둘다 0x1
	dt_root_size_cells = OF_ROOT_NODE_SIZE_CELLS_DEFAULT;
	dt_root_addr_cells = OF_ROOT_NODE_ADDR_CELLS_DEFAULT;

	//prop = 0x4c
	prop = of_get_flat_dt_prop(node, "#size-cells", NULL);
	// 1저장
	if (prop)
		dt_root_size_cells = be32_to_cpup(prop);
	pr_debug("dt_root_size_cells = %x\n", dt_root_size_cells);

	//prop = 0x5c
	prop = of_get_flat_dt_prop(node, "#address-cells", NULL);
	// 1저장
	if (prop)
		dt_root_addr_cells = be32_to_cpup(prop);
	pr_debug("dt_root_addr_cells = %x\n", dt_root_addr_cells);

	// default 랑 똑같네..
	/* break now */
	return 1;
}

//base로 올때 : 1 , &0x1f8
//size로 올떄 : 1 , &0x1fc
u64 __init dt_mem_next_cell(int s, __be32 **cellp)
{
	__be32 *p = *cellp;

	//base로 올때 : reg = 0x1fc
	//size로 올때 : reg = 0x200
	*cellp = p + s;
	return of_read_number(p, s);
}

/**
 * early_init_dt_scan_memory - Look for an parse memory nodes
 */
int __init early_init_dt_scan_memory(unsigned long node, const char *uname,
				     int depth, void *data)
{
	/*
	 * node : memory, depth : 1, res : 0x1e4, property : memory
	 * node : cpu@1, depth : 2, res : 0x307c, property : cpu
	 * node : cpu@2, depth : 2, res : 0x30d8, property : cpu
	 * node : cpu@3, depth : 2, res : 0x3134, property : cpu
	 * node : cpu@3, depth : 2, res : 0x3190, property : cpu
	 * */
	char *type = of_get_flat_dt_prop(node, "device_type", NULL);
	__be32 *reg, *endp;
	unsigned long l;

	/* We are scanning "memory" nodes only */
	if (type == NULL) {
		/*
		 * The longtrail doesn't have a device_type on the
		 * /memory node, so look for the node called /memory@0.
		 */
		if (depth != 1 || strcmp(uname, "memory@0") != 0)
			return 0;
		//memory node만 골라냄
	} else if (strcmp(type, "memory") != 0)
		return 0;
	//없음
	reg = of_get_flat_dt_prop(node, "linux,usable-memory", &l);
	/*
	 * 여기저기서 꽤 보이나, 위에서 잡힌 memory노드만을 보면
	 * reg = 0x1f8, l = 8
	 * */
	if (reg == NULL)
		reg = of_get_flat_dt_prop(node, "reg", &l);
	if (reg == NULL)
		return 0;

	endp = reg + (l / sizeof(__be32));

	pr_debug("memory scan node %s, reg size %ld, data: %x %x %x %x,\n",
	    uname, l, reg[0], reg[1], reg[2], reg[3]);

	//2 >= 2
	while ((endp - reg) >= (dt_root_addr_cells + dt_root_size_cells)) {
		u64 base, size;

		//base = 0x2000_0000, size = 0x8000_0000
		base = dt_mem_next_cell(dt_root_addr_cells, &reg);
		size = dt_mem_next_cell(dt_root_size_cells, &reg);

		if (size == 0)
			continue;
		pr_debug(" - %llx ,  %llx\n", (unsigned long long)base,
		    (unsigned long long)size);

		early_init_dt_add_memory_arch(base, size);
	}

	return 0;
}

/*
 * data =  boot_command_line = CONFIG_CMDLINE 
 * "root=/dev/ram0 rw ramdisk=8192 initrd=0x41000000,8M 
 * console=ttySAC1,115200 init=/linuxrc mem=256M"
 * */
int __init early_init_dt_scan_chosen(unsigned long node, const char *uname,
				     int depth, void *data)
{
	unsigned long l;
	char *p;

	pr_debug("search \"chosen\", depth: %d, uname: %s\n", depth, uname);

	/*
	 * depth가 1이 아닌경우 chosen을 검사한다.
	 * chosen의 depth는 1이다. 
	 * depth == 1인데 chosen이 아닌경우도 많다. 하지만 아래서 쓰는건 업엇다.
	 * */
	if (depth != 1 || !data ||
	    (strcmp(uname, "chosen") != 0 && strcmp(uname, "chosen@0") != 0))
		return 0;

	//함수실행에 필요한 name이 dtb에 없어 안함.
	early_init_dt_check_for_initrd(node);

	/*
	 * 이건 실행함
	 * p = f0
	 * property : console=ttySAC2,115200 init=/linuxrc
	 */
	/* Retrieve command line */
	p = of_get_flat_dt_prop(node, "bootargs", &l);
	/*
	 * data에 복붙을 하지만..
	 * */
	if (p != NULL && l > 0)
		strlcpy(data, p, min((int)l, COMMAND_LINE_SIZE));

	/*
	 * CONFIG_CMDLINE is meant to be a default in case nothing else
	 * managed to set the command line, unless CONFIG_CMDLINE_FORCE
	 * is set in which case we override whatever was found earlier.
	 */
	//설정에 따라 CONFIG_CMDLINE이 우선순위가 댈수도 있는것이 확인
#ifdef CONFIG_CMDLINE //set
#ifndef CONFIG_CMDLINE_FORCE //not set 
	if (!((char *)data)[0])
#endif
		strlcpy(data, CONFIG_CMDLINE, COMMAND_LINE_SIZE);
#endif /* CONFIG_CMDLINE */

	pr_debug("Command line is: %s\n", (char*)data);

	/* break now */
	return 1;
}

#ifdef CONFIG_HAVE_MEMBLOCK // defined
/*
 * called from unflatten_device_tree() to bootstrap devicetree itself
 * Architectures can override this definition if memblock isn't used
 */
void * __init __weak early_init_dt_alloc_memory_arch(u64 size, u64 align)
{
	return __va(memblock_alloc(size, align));
}
#endif

/**
 * unflatten_device_tree - create tree of device_nodes from flat blob
 *
 * unflattens the device-tree passed by the firmware, creating the
 * tree of struct device_node. It also fills the "name" and "type"
 * pointers of the nodes so the normal device-tree walking functions
 * can be used.
 */
void __init unflatten_device_tree(void)
{
	__unflatten_device_tree(initial_boot_params, &of_allnodes,
				early_init_dt_alloc_memory_arch);

	/* Get pointer to "/chosen" and "/aliases" nodes for use everywhere */
	of_alias_scan(early_init_dt_alloc_memory_arch);
}

#endif /* CONFIG_OF_EARLY_FLATTREE */
