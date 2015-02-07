/*
 *  linux/arch/arm/mm/mmu.c
 *
 *  Copyright (C) 1995-2005 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/mman.h>
#include <linux/nodemask.h>
#include <linux/memblock.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <linux/sizes.h>

#include <asm/cp15.h>
#include <asm/cputype.h>
#include <asm/sections.h>
#include <asm/cachetype.h>
#include <asm/setup.h>
#include <asm/smp_plat.h>
#include <asm/tlb.h>
#include <asm/highmem.h>
#include <asm/system_info.h>
#include <asm/traps.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/pci.h>

#include "mm.h"
#include "tcm.h"

/*
 * empty_zero_page is a special page that is used for
 * zero-initialized data and COW.
 */
// 2015-01-24
// 새로운 page를 요청하면 할당하지 않고 
// 이 페이지의 주소를 제공하여
// 값을 쓸때 실제 page를 할당함
struct page *empty_zero_page;
EXPORT_SYMBOL(empty_zero_page);

/*
 * The pmd table for the upper-most set of pages.
 */
pmd_t *top_pmd;

#define CPOLICY_UNCACHED	0
#define CPOLICY_BUFFERED	1
#define CPOLICY_WRITETHROUGH	2
#define CPOLICY_WRITEBACK	3
#define CPOLICY_WRITEALLOC	4

static unsigned int cachepolicy __initdata = CPOLICY_WRITEBACK;
static unsigned int ecc_mask __initdata = 0;
pgprot_t pgprot_user;
pgprot_t pgprot_kernel;
pgprot_t pgprot_hyp_device;
pgprot_t pgprot_s2;
pgprot_t pgprot_s2_device;

EXPORT_SYMBOL(pgprot_user);
EXPORT_SYMBOL(pgprot_kernel);

struct cachepolicy {
	const char	policy[16];
	unsigned int	cr_mask;
	pmdval_t	pmd;
	pteval_t	pte;
	pteval_t	pte_s2;
};

#ifdef CONFIG_ARM_LPAE
#define s2_policy(policy)	policy
#else
#define s2_policy(policy)	0
#endif

static struct cachepolicy cache_policies[] __initdata = {
	{
		.policy		= "uncached",
		.cr_mask	= CR_W|CR_C,
		.pmd		= PMD_SECT_UNCACHED,
		.pte		= L_PTE_MT_UNCACHED,
		.pte_s2		= s2_policy(L_PTE_S2_MT_UNCACHED),
	}, {
		.policy		= "buffered",
		.cr_mask	= CR_C,
		.pmd		= PMD_SECT_BUFFERED,
		.pte		= L_PTE_MT_BUFFERABLE,
		.pte_s2		= s2_policy(L_PTE_S2_MT_UNCACHED),
	}, {
		.policy		= "writethrough",
		.cr_mask	= 0,
		.pmd		= PMD_SECT_WT,
		.pte		= L_PTE_MT_WRITETHROUGH,
		.pte_s2		= s2_policy(L_PTE_S2_MT_WRITETHROUGH),
	}, {
		.policy		= "writeback",
		.cr_mask	= 0,
		.pmd		= PMD_SECT_WB,
		.pte		= L_PTE_MT_WRITEBACK,
		.pte_s2		= s2_policy(L_PTE_S2_MT_WRITEBACK),
	}, {
		.policy		= "writealloc",
		.cr_mask	= 0,
		.pmd		= PMD_SECT_WBWA,
		.pte		= L_PTE_MT_WRITEALLOC,
		.pte_s2		= s2_policy(L_PTE_S2_MT_WRITEBACK),
	}
};

#ifdef CONFIG_CPU_CP15
/*
 * These are useful for identifying cache coherency
 * problems by allowing the cache or the cache and
 * writebuffer to be turned off.  (Note: the write
 * buffer should not be on and the cache off).
 */
static int __init early_cachepolicy(char *p)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cache_policies); i++) {
		int len = strlen(cache_policies[i].policy);

		if (memcmp(p, cache_policies[i].policy, len) == 0) {
			cachepolicy = i;
			cr_alignment &= ~cache_policies[i].cr_mask;
			cr_no_alignment &= ~cache_policies[i].cr_mask;
			break;
		}
	}
	if (i == ARRAY_SIZE(cache_policies))
		printk(KERN_ERR "ERROR: unknown or unsupported cache policy\n");
	/*
	 * This restriction is partly to do with the way we boot; it is
	 * unpredictable to have memory mapped using two different sets of
	 * memory attributes (shared, type, and cache attribs).  We can not
	 * change these attributes once the initial assembly has setup the
	 * page tables.
	 */
	if (cpu_architecture() >= CPU_ARCH_ARMv6) {
		printk(KERN_WARNING "Only cachepolicy=writeback supported on ARMv6 and later\n");
		cachepolicy = CPOLICY_WRITEBACK;
	}
	flush_cache_all();
	set_cr(cr_alignment);
	return 0;
}
early_param("cachepolicy", early_cachepolicy);

static int __init early_nocache(char *__unused)
{
	char *p = "buffered";
	printk(KERN_WARNING "nocache is deprecated; use cachepolicy=%s\n", p);
	early_cachepolicy(p);
	return 0;
}
early_param("nocache", early_nocache);

static int __init early_nowrite(char *__unused)
{
	char *p = "uncached";
	printk(KERN_WARNING "nowb is deprecated; use cachepolicy=%s\n", p);
	early_cachepolicy(p);
	return 0;
}
early_param("nowb", early_nowrite);

#ifndef CONFIG_ARM_LPAE
static int __init early_ecc(char *p)
{
	if (memcmp(p, "on", 2) == 0)
		ecc_mask = PMD_PROTECTION;
	else if (memcmp(p, "off", 3) == 0)
		ecc_mask = 0;
	return 0;
}
early_param("ecc", early_ecc);
#endif

static int __init noalign_setup(char *__unused)
{
	cr_alignment &= ~CR_A;
	cr_no_alignment &= ~CR_A;
	set_cr(cr_alignment);
	return 1;
}
__setup("noalign", noalign_setup);

#ifndef CONFIG_SMP
void adjust_cr(unsigned long mask, unsigned long set)
{
	unsigned long flags;

	mask &= ~CR_A;

	set &= mask;

	local_irq_save(flags);

	cr_no_alignment = (cr_no_alignment & ~mask) | set;
	cr_alignment = (cr_alignment & ~mask) | set;

	set_cr((get_cr() & ~mask) | set);

	local_irq_restore(flags);
}
#endif

#else /* ifdef CONFIG_CPU_CP15 */

static int __init early_cachepolicy(char *p)
{
	pr_warning("cachepolicy kernel parameter not supported without cp15\n");
}
early_param("cachepolicy", early_cachepolicy);

static int __init noalign_setup(char *__unused)
{
	pr_warning("noalign kernel parameter not supported without cp15\n");
}
__setup("noalign", noalign_setup);

#endif /* ifdef CONFIG_CPU_CP15 / else */

#define PROT_PTE_DEVICE		L_PTE_PRESENT|L_PTE_YOUNG|L_PTE_DIRTY|L_PTE_XN
#define PROT_PTE_S2_DEVICE	PROT_PTE_DEVICE
#define PROT_SECT_DEVICE	PMD_TYPE_SECT|PMD_SECT_AP_WRITE

static struct mem_type mem_types[] = {
	[MT_DEVICE] = {		  /* Strongly ordered / ARMv6 shared device */
		.prot_pte	= PROT_PTE_DEVICE | L_PTE_MT_DEV_SHARED |
				  L_PTE_SHARED,
		.prot_pte_s2	= s2_policy(PROT_PTE_S2_DEVICE) |
				  s2_policy(L_PTE_S2_MT_DEV_SHARED) |
				  L_PTE_SHARED,
		.prot_l1	= PMD_TYPE_TABLE,
		.prot_sect	= PROT_SECT_DEVICE | PMD_SECT_S,
		.domain		= DOMAIN_IO,
	},
	[MT_DEVICE_NONSHARED] = { /* ARMv6 non-shared device */
		.prot_pte	= PROT_PTE_DEVICE | L_PTE_MT_DEV_NONSHARED,
		.prot_l1	= PMD_TYPE_TABLE,
		.prot_sect	= PROT_SECT_DEVICE,
		.domain		= DOMAIN_IO,
	},
	[MT_DEVICE_CACHED] = {	  /* ioremap_cached */
		.prot_pte	= PROT_PTE_DEVICE | L_PTE_MT_DEV_CACHED,
		.prot_l1	= PMD_TYPE_TABLE,
		.prot_sect	= PROT_SECT_DEVICE | PMD_SECT_WB,
		.domain		= DOMAIN_IO,
	},
	[MT_DEVICE_WC] = {	/* ioremap_wc */
		.prot_pte	= PROT_PTE_DEVICE | L_PTE_MT_DEV_WC,
		.prot_l1	= PMD_TYPE_TABLE,
		.prot_sect	= PROT_SECT_DEVICE,
		.domain		= DOMAIN_IO,
	},
	[MT_UNCACHED] = {
		.prot_pte	= PROT_PTE_DEVICE,
		.prot_l1	= PMD_TYPE_TABLE,
		.prot_sect	= PMD_TYPE_SECT | PMD_SECT_XN,
		.domain		= DOMAIN_IO,
	},
	[MT_CACHECLEAN] = {
		.prot_sect = PMD_TYPE_SECT | PMD_SECT_XN,
		.domain    = DOMAIN_KERNEL,
	},
#ifndef CONFIG_ARM_LPAE
	[MT_MINICLEAN] = {
		.prot_sect = PMD_TYPE_SECT | PMD_SECT_XN | PMD_SECT_MINICACHE,
		.domain    = DOMAIN_KERNEL,
	},
#endif
	[MT_LOW_VECTORS] = {
		.prot_pte  = L_PTE_PRESENT | L_PTE_YOUNG | L_PTE_DIRTY |
				L_PTE_RDONLY,
		.prot_l1   = PMD_TYPE_TABLE,
		.domain    = DOMAIN_USER,
	},
	[MT_HIGH_VECTORS] = {
		.prot_pte  = L_PTE_PRESENT | L_PTE_YOUNG | L_PTE_DIRTY |
				L_PTE_USER | L_PTE_RDONLY,
		.prot_l1   = PMD_TYPE_TABLE,
		.domain    = DOMAIN_USER,
	},
	[MT_MEMORY] = {
		.prot_pte  = L_PTE_PRESENT | L_PTE_YOUNG | L_PTE_DIRTY,
		.prot_l1   = PMD_TYPE_TABLE,
		.prot_sect = PMD_TYPE_SECT | PMD_SECT_AP_WRITE,
		.domain    = DOMAIN_KERNEL,
	},
	[MT_ROM] = {
		.prot_sect = PMD_TYPE_SECT,
		.domain    = DOMAIN_KERNEL,
	},
	[MT_MEMORY_NONCACHED] = {
		.prot_pte  = L_PTE_PRESENT | L_PTE_YOUNG | L_PTE_DIRTY |
				L_PTE_MT_BUFFERABLE,
		.prot_l1   = PMD_TYPE_TABLE,
		.prot_sect = PMD_TYPE_SECT | PMD_SECT_AP_WRITE,
		.domain    = DOMAIN_KERNEL,
	},
	[MT_MEMORY_DTCM] = {
		.prot_pte  = L_PTE_PRESENT | L_PTE_YOUNG | L_PTE_DIRTY |
				L_PTE_XN,
		.prot_l1   = PMD_TYPE_TABLE,
		.prot_sect = PMD_TYPE_SECT | PMD_SECT_XN,
		.domain    = DOMAIN_KERNEL,
	},
	[MT_MEMORY_ITCM] = {
		.prot_pte  = L_PTE_PRESENT | L_PTE_YOUNG | L_PTE_DIRTY,
		.prot_l1   = PMD_TYPE_TABLE,
		.domain    = DOMAIN_KERNEL,
	},
	[MT_MEMORY_SO] = {
		.prot_pte  = L_PTE_PRESENT | L_PTE_YOUNG | L_PTE_DIRTY |
				L_PTE_MT_UNCACHED | L_PTE_XN,
		.prot_l1   = PMD_TYPE_TABLE,
		.prot_sect = PMD_TYPE_SECT | PMD_SECT_AP_WRITE | PMD_SECT_S |
				PMD_SECT_UNCACHED | PMD_SECT_XN,
		.domain    = DOMAIN_KERNEL,
	},
	[MT_MEMORY_DMA_READY] = {
		.prot_pte  = L_PTE_PRESENT | L_PTE_YOUNG | L_PTE_DIRTY,
		.prot_l1   = PMD_TYPE_TABLE,
		.domain    = DOMAIN_KERNEL,
	},
};

const struct mem_type *get_mem_type(unsigned int type)
{
	return type < ARRAY_SIZE(mem_types) ? &mem_types[type] : NULL;
}
EXPORT_SYMBOL(get_mem_type);

/*
 * Adjust the PMD section entries according to the CPU in use.
 */
static void __init build_mem_type_table(void)
{
	struct cachepolicy *cp;
	unsigned int cr = get_cr();
	pteval_t user_pgprot, kern_pgprot, vecs_pgprot;
	pteval_t hyp_device_pgprot, s2_pgprot, s2_device_pgprot;
	int cpu_arch = cpu_architecture();	//cpu_arch = CPU_ARCH_ARMv7
	int i;

	if (cpu_arch < CPU_ARCH_ARMv6) {
#if defined(CONFIG_CPU_DCACHE_DISABLE)
		if (cachepolicy > CPOLICY_BUFFERED)
			cachepolicy = CPOLICY_BUFFERED;
#elif defined(CONFIG_CPU_DCACHE_WRITETHROUGH)
		if (cachepolicy > CPOLICY_WRITETHROUGH)
			cachepolicy = CPOLICY_WRITETHROUGH;
#endif
	}
	if (cpu_arch < CPU_ARCH_ARMv5) {
		if (cachepolicy >= CPOLICY_WRITEALLOC)
			cachepolicy = CPOLICY_WRITEBACK;
		ecc_mask = 0;
	}
	if (is_smp())	//is_smp() => True
		cachepolicy = CPOLICY_WRITEALLOC;

	/*
	 * Strip out features not present on earlier architectures.
	 * Pre-ARMv5 CPUs don't have TEX bits.  Pre-ARMv6 CPUs or those
	 * without extended page tables don't have the 'Shared' bit.
	 */
	if (cpu_arch < CPU_ARCH_ARMv5)
		for (i = 0; i < ARRAY_SIZE(mem_types); i++)
			mem_types[i].prot_sect &= ~PMD_SECT_TEX(7);
	//CR_XP : Reserved,RAO/SBOP. Read-As-One, Should-Be-One-or-Preserved on writes
	if ((cpu_arch < CPU_ARCH_ARMv6 || !(cr & CR_XP)) && !cpu_is_xsc3())
		for (i = 0; i < ARRAY_SIZE(mem_types); i++)
			mem_types[i].prot_sect &= ~PMD_SECT_S;

	/*
	 * ARMv5 and lower, bit 4 must be set for page tables (was: cache
	 * "update-able on write" bit on ARM610).  However, Xscale and
	 * Xscale3 require this bit to be cleared.
	 */
	if (cpu_is_xscale() || cpu_is_xsc3()) {
		for (i = 0; i < ARRAY_SIZE(mem_types); i++) {
			mem_types[i].prot_sect &= ~PMD_BIT4;
			mem_types[i].prot_l1 &= ~PMD_BIT4;
		}
	} else if (cpu_arch < CPU_ARCH_ARMv6) {
		for (i = 0; i < ARRAY_SIZE(mem_types); i++) {
			if (mem_types[i].prot_l1)
				mem_types[i].prot_l1 |= PMD_BIT4;
			if (mem_types[i].prot_sect)
				mem_types[i].prot_sect |= PMD_BIT4;
		}
	}

	/*
	 * Mark the device areas according to the CPU/architecture.
	 */
	if (cpu_is_xsc3() || (cpu_arch >= CPU_ARCH_ARMv6 && (cr & CR_XP))) {
		if (!cpu_is_xsc3()) {
			/*
			 * Mark device regions on ARMv6+ as execute-never
			 * to prevent speculative instruction fetches.
			 */
			mem_types[MT_DEVICE].prot_sect |= PMD_SECT_XN;
			mem_types[MT_DEVICE_NONSHARED].prot_sect |= PMD_SECT_XN;
			mem_types[MT_DEVICE_CACHED].prot_sect |= PMD_SECT_XN;
			mem_types[MT_DEVICE_WC].prot_sect |= PMD_SECT_XN;
		}
		if (cpu_arch >= CPU_ARCH_ARMv7 && (cr & CR_TRE)) {
			/*
			 * For ARMv7 with TEX remapping,
			 * - shared device is SXCB=1100
			 * - nonshared device is SXCB=0100
			 * - write combine device mem is SXCB=0001
			 * (Uncached Normal memory)
			 */
			mem_types[MT_DEVICE].prot_sect |= PMD_SECT_TEX(1);
			mem_types[MT_DEVICE_NONSHARED].prot_sect |= PMD_SECT_TEX(1);
			mem_types[MT_DEVICE_WC].prot_sect |= PMD_SECT_BUFFERABLE;
		} else if (cpu_is_xsc3()) {
			/*
			 * For Xscale3,
			 * - shared device is TEXCB=00101
			 * - nonshared device is TEXCB=01000
			 * - write combine device mem is TEXCB=00100
			 * (Inner/Outer Uncacheable in xsc3 parlance)
			 */
			mem_types[MT_DEVICE].prot_sect |= PMD_SECT_TEX(1) | PMD_SECT_BUFFERED;
			mem_types[MT_DEVICE_NONSHARED].prot_sect |= PMD_SECT_TEX(2);
			mem_types[MT_DEVICE_WC].prot_sect |= PMD_SECT_TEX(1);
		} else {
			/*
			 * For ARMv6 and ARMv7 without TEX remapping,
			 * - shared device is TEXCB=00001
			 * - nonshared device is TEXCB=01000
			 * - write combine device mem is TEXCB=00100
			 * (Uncached Normal in ARMv6 parlance).
			 */
			mem_types[MT_DEVICE].prot_sect |= PMD_SECT_BUFFERED;
			mem_types[MT_DEVICE_NONSHARED].prot_sect |= PMD_SECT_TEX(2);
			mem_types[MT_DEVICE_WC].prot_sect |= PMD_SECT_TEX(1);
		}
	} else {
		/*
		 * On others, write combining is "Uncached/Buffered"
		 */
		mem_types[MT_DEVICE_WC].prot_sect |= PMD_SECT_BUFFERABLE;
	}

	/*
	 * Now deal with the memory-type mappings
	 */
	cp = &cache_policies[cachepolicy];	
//		.policy		= "writealloc",
//		.cr_mask	= 0,
//		.pmd		= PMD_SECT_WBWA,
//		.pte		= L_PTE_MT_WRITEALLOC,
//		.pte_s2		= s2_policy(L_PTE_S2_MT_WRITEBACK),
	vecs_pgprot = kern_pgprot = user_pgprot = cp->pte; 
	s2_pgprot = cp->pte_s2;	//cp->pte_s2 = 0;
	hyp_device_pgprot = mem_types[MT_DEVICE].prot_pte;
	s2_device_pgprot = mem_types[MT_DEVICE].prot_pte_s2;

	/*
	 * We don't use domains on ARMv6 (since this causes problems with
	 * v6/v7 kernels), so we must use a separate memory type for user
	 * r/o, kernel r/w to map the vectors page.
	 */
#ifndef CONFIG_ARM_LPAE
	if (cpu_arch == CPU_ARCH_ARMv6)
		vecs_pgprot |= L_PTE_MT_VECTORS;
#endif

	/*
	 * ARMv6 and above have extended page tables.
	 */
	if (cpu_arch >= CPU_ARCH_ARMv6 && (cr & CR_XP)) {
#ifndef CONFIG_ARM_LPAE
		/*
		 * Mark cache clean areas and XIP ROM read only
		 * from SVC mode and no access from userspace.
		 */
		mem_types[MT_ROM].prot_sect |= PMD_SECT_APX|PMD_SECT_AP_WRITE;
		mem_types[MT_MINICLEAN].prot_sect |= PMD_SECT_APX|PMD_SECT_AP_WRITE;
		mem_types[MT_CACHECLEAN].prot_sect |= PMD_SECT_APX|PMD_SECT_AP_WRITE;
#endif

		if (is_smp()) {
			/*
			 * Mark memory with the "shared" attribute
			 * for SMP systems
			 */
			user_pgprot |= L_PTE_SHARED; // L_PTE_MT_WRITEALLOC | L_PTE_SHARED
			kern_pgprot |= L_PTE_SHARED;
			vecs_pgprot |= L_PTE_SHARED;
			s2_pgprot |= L_PTE_SHARED;
			mem_types[MT_DEVICE_WC].prot_sect |= PMD_SECT_S;
			mem_types[MT_DEVICE_WC].prot_pte |= L_PTE_SHARED;
			mem_types[MT_DEVICE_CACHED].prot_sect |= PMD_SECT_S;
			mem_types[MT_DEVICE_CACHED].prot_pte |= L_PTE_SHARED;
			mem_types[MT_MEMORY].prot_sect |= PMD_SECT_S;
			mem_types[MT_MEMORY].prot_pte |= L_PTE_SHARED;
			mem_types[MT_MEMORY_DMA_READY].prot_pte |= L_PTE_SHARED;
			mem_types[MT_MEMORY_NONCACHED].prot_sect |= PMD_SECT_S;
			mem_types[MT_MEMORY_NONCACHED].prot_pte |= L_PTE_SHARED;
		}
	}

	/*
	 * Non-cacheable Normal - intended for memory areas that must
	 * not cause dirty cache line writebacks when used
	 */
	if (cpu_arch >= CPU_ARCH_ARMv6) {
		if (cpu_arch >= CPU_ARCH_ARMv7 && (cr & CR_TRE)) {
			/* Non-cacheable Normal is XCB = 001 */
			mem_types[MT_MEMORY_NONCACHED].prot_sect |=
				PMD_SECT_BUFFERED;
		} else {
			/* For both ARMv6 and non-TEX-remapping ARMv7 */
			mem_types[MT_MEMORY_NONCACHED].prot_sect |=
				PMD_SECT_TEX(1);
		}
	} else {
		mem_types[MT_MEMORY_NONCACHED].prot_sect |= PMD_SECT_BUFFERABLE;
	}

#ifdef CONFIG_ARM_LPAE
	/*
	 * Do not generate access flag faults for the kernel mappings.
	 */
	for (i = 0; i < ARRAY_SIZE(mem_types); i++) {
		mem_types[i].prot_pte |= PTE_EXT_AF;
		if (mem_types[i].prot_sect)
			mem_types[i].prot_sect |= PMD_SECT_AF;
	}
	kern_pgprot |= PTE_EXT_AF;
	vecs_pgprot |= PTE_EXT_AF;
#endif

	for (i = 0; i < 16; i++) {
		pteval_t v = pgprot_val(protection_map[i]);
		protection_map[i] = __pgprot(v | user_pgprot);
// protection_map[index] = __pgprot( protection_map[index] | (L_PTE_MT_WRITEALLOC | L_PTE_SHARED))
 
	}

	mem_types[MT_LOW_VECTORS].prot_pte |= vecs_pgprot;
	mem_types[MT_HIGH_VECTORS].prot_pte |= vecs_pgprot;

	pgprot_user   = __pgprot(L_PTE_PRESENT | L_PTE_YOUNG | user_pgprot);
	pgprot_kernel = __pgprot(L_PTE_PRESENT | L_PTE_YOUNG |
				 L_PTE_DIRTY | kern_pgprot);
	// s2 : Stage 2 translation (I.P.A -> P.A), owned by VMM(Virtual Machine Monitor, Hypervisor) 
	pgprot_s2  = __pgprot(L_PTE_PRESENT | L_PTE_YOUNG | s2_pgprot);
	pgprot_s2_device  = __pgprot(s2_device_pgprot);
	pgprot_hyp_device  = __pgprot(hyp_device_pgprot);

	mem_types[MT_LOW_VECTORS].prot_l1 |= ecc_mask;
	mem_types[MT_HIGH_VECTORS].prot_l1 |= ecc_mask;
	mem_types[MT_MEMORY].prot_sect |= ecc_mask | cp->pmd;
	mem_types[MT_MEMORY].prot_pte |= kern_pgprot;
	mem_types[MT_MEMORY_DMA_READY].prot_pte |= kern_pgprot;
	mem_types[MT_MEMORY_NONCACHED].prot_sect |= ecc_mask;
	mem_types[MT_ROM].prot_sect |= cp->pmd;

	switch (cp->pmd) {
	case PMD_SECT_WT:
		mem_types[MT_CACHECLEAN].prot_sect |= PMD_SECT_WT;
		break;
	case PMD_SECT_WB:
	case PMD_SECT_WBWA:
		mem_types[MT_CACHECLEAN].prot_sect |= PMD_SECT_WB;
		break;
	}
	printk("Memory policy: ECC %sabled, Data cache %s\n",
		ecc_mask ? "en" : "dis", cp->policy);

	for (i = 0; i < ARRAY_SIZE(mem_types); i++) {
		struct mem_type *t = &mem_types[i];
		if (t->prot_l1)
			t->prot_l1 |= PMD_DOMAIN(t->domain);
		if (t->prot_sect)
			t->prot_sect |= PMD_DOMAIN(t->domain);
	}
	//14-09-13 end
}

#ifdef CONFIG_ARM_DMA_MEM_BUFFERABLE
pgprot_t phys_mem_access_prot(struct file *file, unsigned long pfn,
			      unsigned long size, pgprot_t vma_prot)
{
	if (!pfn_valid(pfn))
		return pgprot_noncached(vma_prot);
	else if (file->f_flags & O_SYNC)
		return pgprot_writecombine(vma_prot);
	return vma_prot;
}
EXPORT_SYMBOL(phys_mem_access_prot);
#endif

#define vectors_base()	(vectors_high() ? 0xffff0000 : 0)

static void __init *early_alloc_aligned(unsigned long sz, unsigned long align)
{
	// sz 만큼 메모리 할당, 초기화 후 포인터 리턴
	void *ptr = __va(memblock_alloc(sz, align));
	memset(ptr, 0, sz);
	return ptr;
}

static void __init *early_alloc(unsigned long sz)
{
	return early_alloc_aligned(sz, sz);
}

static pte_t * __init early_pte_alloc(pmd_t *pmd, unsigned long addr, unsigned long prot)
{
	if (pmd_none(*pmd)) {
		// *pmd == 0인 경우, 즉 pmd 내에 매핑된 주소가 설정되지 않은 경우
		// PTE_HWTABLE_OFF + PTE_WHTABLE_SIZE == 4KB
		// 4kb size 4kb align으로 메모리 할당
		pte_t *pte = early_alloc(PTE_HWTABLE_OFF + PTE_HWTABLE_SIZE);
		// 메모리 할당 후 물리 주소 변환, PMD_TABLE 세팅 값을 넘김
		// 할당된 주소를 pmd[0], pmd[1]에 세팅.
		// l1 table -> page table type 
		__pmd_populate(pmd, __pa(pte), prot);
	}
	BUG_ON(pmd_bad(*pmd));
	return pte_offset_kernel(pmd, addr);
}

static void __init alloc_init_pte(pmd_t *pmd, unsigned long addr,
				  unsigned long end, unsigned long pfn,
				  const struct mem_type *type)
{
	// pmd 로 pte를 구하는 동작 
	pte_t *pte = early_pte_alloc(pmd, addr, type->prot_l1);
	do {
		set_pte_ext(pte, pfn_pte(pfn, __pgprot(type->prot_pte)), 0);
		pfn++;
	} while (pte++, addr += PAGE_SIZE, addr != end);
}

static void __init __map_init_section(pmd_t *pmd, unsigned long addr,
			unsigned long end, phys_addr_t phys,
			const struct mem_type *type)
{
	pmd_t *p = pmd;

#ifndef CONFIG_ARM_LPAE
	/*
	 * In classic MMU format, puds and pmds are folded in to
	 * the pgds. pmd_offset gives the PGD entry. PGDs refer to a
	 * group of L1 entries making up one logical pointer to
	 * an L2 table (2MB), where as PMDs refer to the individual
	 * L1 entries (1MB). Hence increment to get the correct
	 * offset for odd 1MB sections.
	 * (See arch/arm/include/asm/pgtable-2level.h)
	 */
	 // in createmapping, addr == md->virtual의 4KB align
	 // addr이 SECTION_SIZE(1mb)단위이면  pmd증가
	 /* 이 함수 진입시 |~SECTION_MASK == 0임을 체크했을때 
	  * addr이 SECTION_SIZE보다 작을 것임을 이미 체크.
	  * 다른곳에서 쓰는진 모름 (틀린 가정일지도 모름)
	  * 일단 이건 넘긴다고 예상
	  */
	if (addr & SECTION_SIZE)
		pmd++;
#endif
	do {
		// type->prot_sect : MT_MEMORY로 넘길 때 (1 << 2 | 1 << 10)의 값을 지님
		// __pmd 단순한 형변환
		// pmd : 4kb aligned address 
		// -> 10bit(AP(0x01)), 2bit(B bit Setting) 페이지 테이블 세팅
		// read/write privileged only 설정
		/*
		 * pgd는 0xc000_4000~0xc000_8000이다.
		 * pgd한개당 2MB를 관리하는데 결국 pgd는 pmd두개를 묶는
		 * 역할만을 할 뿐이며, pgd가 2MB때문에 이 루프를 최소 두번
		 * 실행한다. (pgd_t = pgd[0] , pgd[1] -> pmd 2개) 
		 * */
		*pmd = __pmd(phys | type->prot_sect);
		/*
		 * pmd_size란게 아닌 SECTION_SIZE인 이유는 리눅스에서 1MB
		 * 단위를 SECTION으로 보기때문.(? 1MB는 phys적인 측면에서
		 * section이고 soft적인 측면에서는 pmd인가?)
		 * *pmd의 값은 크게 다음과 같이 입력, 출력되는거 같음
		 * 1. 4001_1c0e -> 4001_140e, 4011_1c0e -> 4011_140e
		 *	입력:11c0e ,출력 1140e고정
		 * 2. 0 -> 40a1_140e, 0 -> 40b1_140e
		 *	입력:0 , 출력 1140e고정
		 * 3. 0 -> 6f7f_e821, 0 -> 6f7fec21(high mem 할때 단한번, pte할때임)
		 * 4. 0 -> 6f7f_c841
		 *	출력 841 고정 pgd[0]의 부분 pte할때
		 * 5. 0 -> 6f7fac41, ? -> 6f7f9c41
		 *	출력 c41 고정 pgd[1]의 부분 pte할때
		 *
		 * 이 루프는 1, 2번에 해당
		 * 1번 : 0001_0001_1100_0000_1110 
		 *    -> 0001_0001_0100_0000_1110
		 * 2번 : 0 
		 *    -> 0001_0001_0100_0000_1110
		 *
		 * FLPD(First Level Page Descriptor Fomart)에서 1MB PAGE는
		 * 31:20 -> 1MB page base address
		 * 이고 뒤를 보면 
		 * 1번의 경우 접근권한이 수정됬고
		 * 2번의 경우는 아에 새로 할당된것 
		 * 
		 * 3번 : 1000_0010_0001
		 *    -> 1100_0010_0001
		 * 4번 : 1000_0100_0001
		 * 5번 : 1100_0100_0001
		 * SLPD(Second ...)에서 4KB page base는
		 * 31:16 -> 4KB page base address 
		 * /// Format에 AP가 2개있는데 한비트를 사용하는 AP(접근권한)와
		 * /// 2비트를 사용하는 AP의 각각의 용도는 뭐지?
		 * /// pte에서는 S비트가 다S는 뭐지?
		 * /// 1번비트가 1아닌 0인데 이경우 64KB page인거 같은데?
		 * */
		phys += SECTION_SIZE;
	} while (pmd++, addr += SECTION_SIZE, addr != end);

	flush_pmd_entry(p);
}
/*
 * pud = pgd
 * addr = region virtual addr
 * end = next = end
 * phys = region 의 phys addr
 * type = MT_MEMORY
 * */
static void __init alloc_init_pmd(pud_t *pud, unsigned long addr,
				      unsigned long end, phys_addr_t phys,
				      const struct mem_type *type)
{
	pmd_t *pmd = pmd_offset(pud, addr);
	unsigned long next;

	do {
		/*
		 * With LPAE, we must loop over to map
		 * all the pmds for the given range.
		 */
		next = pmd_addr_end(addr, end);

		/*
		 * Try a section mapping - addr, next and phys must all be
		 * aligned to a section boundary.
		 */
		/*
		 *2단계 페이징이므로 여기까지 next = end, pmd=pud=pgd 가된다
		 *type->prot_sect 는 값이 있으며,
		 *뒤의 addr...들은 안들어올것으로 예상(들어올수도있다)
		 *prot_sect = PMD_TYPE_SECT | PMD_SECT_AP_WRITE
		 * */
		 //2014-10-11 
		 /*
		  * addr(가상주소), next(다음의 가상주소), phys이 전부 
		  * section_mask, 즉 1M인 경우 이는 map init의 맨 처음임을
		  * 예상할 수 있다.
		  * 만약 처음임에도 이 if문에 걸리지 않는 경우는 map init을
		  * 할 필요가 없는 경우라고 판단된다.
		  * (addr == 0인경우와 addr = 0xffff_0000인 경우등등이
		  * 있었음을 상기. 그렇다면 addr = 0인 경우는 그렇다쳐도 
		  * 다른경우는 왜 map 초기화를 할필요가 없을까?)
		  * addr == 0인경우 0xc000_4000 이고 
		  * addr == 0xffff_0000인 경우 0xc000_7ff8이다.
		  * 즉 addr == 0xffff_0000은 pgd_end를 의미.
		  * addr == 0xffff_0000인 경우는 MT_HIGH_VECTORS를 세팅할때다.
		  * high_vectors를 세팅할때는 alloc_init_pte로 넘어가서
		  * pte 1개만 초기화를 하는거 같다.(?)
		  * pgd는 2MB 단위, pmd는 1MB 단위라는 것을 유념.
		  * */
		if (type->prot_sect &&
				((addr | next | phys) & ~SECTION_MASK) == 0) {
			// prot_sect가 설정되어있고, addr, next, phys 3개 중 1개 이상이 1mb단위가 아닐 때
			// pmd에 대해 type->prot_sect의 비트(여기서는 ap(0x01), b(0x01))를 설정한다.
			__map_init_section(pmd, addr, next, phys, type);
		} else {
			alloc_init_pte(pmd, addr, next,
						__phys_to_pfn(phys), type);
		}

		phys += next - addr;

	} while (pmd++, addr = next, addr != end);
}
/*
 * pgd : 현재 설정중인 pgd
 * addr : 현재 설정주인 region의 virtual addr
 * end : end
 * phys : region의 Phys addr
 * type : MT_MEMORY의 정보가 있는곳의 주소
 * */
static void __init alloc_init_pud(pgd_t *pgd, unsigned long addr,
				  unsigned long end, phys_addr_t phys,
				  const struct mem_type *type)
{
	pud_t *pud = pud_offset(pgd, addr);
	unsigned long next;

	do {
		next = pud_addr_end(addr, end);
		//pud는 사용을 안하므로 next=end가 됨
		alloc_init_pmd(pud, addr, next, phys, type);
		phys += next - addr;
	} while (pud++, addr = next, addr != end);
}

// 2014-10-25
// *pmd값을 아래와 같이 설정하는 것이 주기능.
// *pmd++ = __pmd(phys | type->prot_sect | PMD_SECT_SUPER);
#ifndef CONFIG_ARM_LPAE
static void __init create_36bit_mapping(struct map_desc *md,
					const struct mem_type *type)
{
	unsigned long addr, length, end;
	phys_addr_t phys;
	pgd_t *pgd;

	addr = md->virtual;
	// pfn(page frame number)
	phys = __pfn_to_phys(md->pfn);
	length = PAGE_ALIGN(md->length);

	// cpu_is_xsc3 - Intel's XScale3 core
	if (!(cpu_architecture() >= CPU_ARCH_ARMv6 || cpu_is_xsc3())) {
		printk(KERN_ERR "MM: CPU does not support supersection "
		       "mapping for 0x%08llx at 0x%08lx\n",
		       (long long)__pfn_to_phys((u64)md->pfn), addr);
		return;
	}

	/* N.B.	ARMv6 supersections are only defined to work with domain 0.
	 *	Since domain assignments can in fact be arbitrary, the
	 *	'domain == 0' check below is required to insure that ARMv6
	 *	supersections are only allocated for domain 0 regardless
	 *	of the actual domain assignments in use.
	 */
	 // ARMv6 supsections는 domain 0에서 동작하는것으로 미리 정의됨.
	if (type->domain) {
		printk(KERN_ERR "MM: invalid domain in supersection "
		       "mapping for 0x%08llx at 0x%08lx\n",
		       (long long)__pfn_to_phys((u64)md->pfn), addr);
		return;
	}

	// ~SUPERSECTION_MASK : 하위 24비트
	// 하위 24비트의 값은 0으로 미리 채워져 있을것이다라고 예상함.
	if ((addr | length | __pfn_to_phys(md->pfn)) & ~SUPERSECTION_MASK) {
		printk(KERN_ERR "MM: cannot create mapping for 0x%08llx"
		       " at 0x%08lx invalid alignment\n",
		       (long long)__pfn_to_phys((u64)md->pfn), addr);
		return;
	}

	/*
	 * Shift bits [35:32] of address into bits [23:20] of PMD
	 * (See ARMv6 spec).
	 */
	// 위 주석의 내용대로라면, [23:20]의 값을, [35:32]에 채워넣는다.
	// if (md->pfn >= 0x100000) 20bit
	phys |= (((md->pfn >> (32 - PAGE_SHIFT)) & 0xF) << 20);
	//

	pgd = pgd_offset_k(addr);
	end = addr + length;
	do {
		pud_t *pud = pud_offset(pgd, addr);
		pmd_t *pmd = pmd_offset(pud, addr);
		int i;

		// Loop, total 16회.
		// [MEMORY].prot_sect = PMD_TYPE_SECT[:2] | PMD_SECT_AP_WRITE[:10],
		// PMD_SECT_SUPER = (1 << 18);
		for (i = 0; i < 16; i++)
			*pmd++ = __pmd(phys | type->prot_sect | PMD_SECT_SUPER);

		// SUPERSECTION_SIZE : 1 << 24;
		addr += SUPERSECTION_SIZE;
		phys += SUPERSECTION_SIZE;
		// PGDIR_SHIFT 21
		pgd += SUPERSECTION_SIZE >> PGDIR_SHIFT;
	} while (addr != end);
}
#endif	/* !CONFIG_ARM_LPAE */

/*
 * Create the page directory entries and any necessary
 * page tables for the mapping specified by `md'.  We
 * are able to cope here with varying sizes and address
 * offsets, and we take full advantage of sections and
 * supersections.
 */
/* arm 에서
 * 1단계 paging 
 * super section : 16MB
 * section : 1MB
 *
 * 2단계 paging
 * super section : 64kb
 * section : 4kb
 * */
static void __init create_mapping(struct map_desc *md)
{
	unsigned long addr, length, end;
	phys_addr_t phys;
	const struct mem_type *type;
	pgd_t *pgd;

	//region start지점이 task_size보다 작은지, vectors_base와 같은지를 검사
	//vectors_base의 V flag( 0xffff_0000로 relocation을 하는지에 대한)가 설정되있는지
	//확인한다.
	/*relocation 을 안했으면 vectors_base = 0이고
	 *relocation 을 했으면 vectors_base = 0xffff_0000 인데
	 *start지점이 base와 같으면 정상적으로 설정이 됬다고 판단
	*/
	if (md->virtual != vectors_base() && md->virtual < TASK_SIZE) {
		printk(KERN_WARNING "BUG: not creating mapping for 0x%08llx"
		       " at 0x%08lx in user region\n",
		       (long long)__pfn_to_phys((u64)md->pfn), md->virtual);
		return;
	}

	if ((md->type == MT_DEVICE || md->type == MT_ROM) &&
	    md->virtual >= PAGE_OFFSET &&
	    (md->virtual < VMALLOC_START || md->virtual >= VMALLOC_END)) {
		printk(KERN_WARNING "BUG: mapping for 0x%08llx"
		       " at 0x%08lx out of vmalloc space\n",
		       (long long)__pfn_to_phys((u64)md->pfn), md->virtual);
	}

	//mem_type struct의 MT_MEMORY 배열인덱스 주소를 type에 저장
	type = &mem_types[md->type];

#ifndef CONFIG_ARM_LPAE
	/*
	 * Catch 36-bit addresses
	 */
	//32bit 이상인 경우 pfn >= 0x100000 이다. 
	// 1pfn = 0x1000(4k) 이므로 0x100000(1M) 을 곱하면 4G.
	//나중에
	// 2014-10-25, 시작!
	if (md->pfn >= 0x100000) {
		create_36bit_mapping(md, type);
		return;
	}
#endif
	/*
	 * md->virtual(start)가 0 -> addr = 0
	 * md->virtual(start)가 ffff_0000 -> addr = ffff_0000
	 * md->virtual이 TASK_SIZE보다 큰경우 -> &PAGE_MASK 한값
	 * PAGE_MASK는 시작주소를 PAGE_ALIGN한 개념
	 * */
	addr = md->virtual & PAGE_MASK;
	phys = __pfn_to_phys(md->pfn);
	/*
	 *원래 length에 위에 addr(start)를 할때 PAGE_MASK에 의해 버려진값
	 *을 더한후 ALIGN한 것.
	 * */
	length = PAGE_ALIGN(md->length + (md->virtual & ~PAGE_MASK));

	/*
	 * prot_l1 == 0인 동시에 (addr | phys | length) & ~SECTION_MASK가 의미있는 값을
	 * 가질수는 없다.
	 * MT_MEMORY의 prot_l1은 table_type(0x1)이다. 0인경우는 fault
	 * */
	if (type->prot_l1 == 0 && ((addr | phys | length) & ~SECTION_MASK)) {
		printk(KERN_WARNING "BUG: map for 0x%08llx at 0x%08lx can not "
		       "be mapped using pages, ignoring.\n",
		       (long long)__pfn_to_phys(md->pfn), addr);
		return;
	}

	//pgd base addr(0xc000_8000(ram+text) - 0x4000(pgdir size) + pgd_offset(addr))
	/*
	 * pgt 는 16kb, pgd는 4096개이지만 리눅스에서는 pgd를 두개씩 묶어서
	 * 사용 한다. pgd[2] * 2048로 사용하며 pgd[2]는 section으로접근?
	 * */
	pgd = pgd_offset_k(addr);
	//이때당시 addr은 startstart이므로 length를 add하면 end가 나온다
	end = addr + length;
	do {
		//addr+end가 pgd끝을 넘는지를 검사. 넘으면 pgd끝으로, 아니면 addr+end로
		/*
		 *이때 생각해놔야 될것이, pgd, next는 실제 물리주소에 있는
		 * page table의 주소이고, addr,phy는 해당 pgd(pmd,pte등등)
		 * 가리키고 있는 실제 물리주소를 의미하는걸 유념..
		 * */
		unsigned long next = pgd_addr_end(addr, end);

		alloc_init_pud(pgd, addr, next, phys, type);

		//phys는 ptn을 다시 원상복귀한 값이였음을 유념
		//안에서 pfn을 다시 계산하기 위해 사용하는것 같음
		//더해지는 값은 각 층(pgd,...pte)의 값
		//pgd에서는 2MB, pte에서는 ..값임을 예상
		phys += next - addr;
		addr = next;
	} while (pgd++, addr != end);
}

/*
 * Create the architecture specific mappings
 */
// 2014-10-25, iotable_init(&map, 1);
void __init iotable_init(struct map_desc *io_desc, int nr)
{
	struct map_desc *md;
	struct vm_struct *vm;
	struct static_vm *svm;

	if (!nr)
		return;

	// 만약, sizeof(svm)이라면 4일것이다.
	svm = early_alloc_aligned(sizeof(*svm) * nr, __alignof__(*svm));

	for (md = io_desc; nr; md++, nr--) {
		create_mapping(md);

		vm = &svm->vm;
		vm->addr = (void *)(md->virtual & PAGE_MASK);
		vm->size = PAGE_ALIGN(md->length + (md->virtual & ~PAGE_MASK));
		vm->phys_addr = __pfn_to_phys(md->pfn);
		vm->flags = VM_IOREMAP | VM_ARM_STATIC_MAPPING;
		// #define VM_ARM_MTYPE(mt)        ((mt) << 20)
		vm->flags |= VM_ARM_MTYPE(md->type);
		vm->caller = iotable_init;
		// static_vmlist에 추가
		add_static_vm_early(svm++);
	}
}

void __init vm_reserve_area_early(unsigned long addr, unsigned long size,
				  void *caller)
{
	struct vm_struct *vm;
	struct static_vm *svm;

	svm = early_alloc_aligned(sizeof(*svm), __alignof__(*svm));

	vm = &svm->vm;
	vm->addr = (void *)addr;
	vm->size = size;
	vm->flags = VM_IOREMAP | VM_ARM_EMPTY_MAPPING;
	vm->caller = caller;
	add_static_vm_early(svm);
}

#ifndef CONFIG_ARM_LPAE

/*
 * The Linux PMD is made of two consecutive section entries covering 2MB
 * (see definition in include/asm/pgtable-2level.h).  However a call to
 * create_mapping() may optimize static mappings by using individual
 * 1MB section mappings.  This leaves the actual PMD potentially half
 * initialized if the top or bottom section entry isn't used, leaving it
 * open to problems if a subsequent ioremap() or vmalloc() tries to use
 * the virtual space left free by that unused section entry.
 *
 * Let's avoid the issue by inserting dummy vm entries covering the unused
 * PMD halves once the static mappings are in place.
 */
// 2014년 11월 01일
static void __init pmd_empty_section_gap(unsigned long addr)
{
	// 아래의 함수는 바로 위에 있음
	vm_reserve_area_early(addr, SECTION_SIZE, pmd_empty_section_gap);
}

// 2014년 11월 01일 완료
static void __init fill_pmd_gaps(void)
{
	struct static_vm *svm;
	struct vm_struct *vm;
	unsigned long addr, next = 0;
	pmd_t *pmd;

	//#define list_for_each_entry(pos, head, member)              \
	//    for (pos = list_entry((head)->next, typeof(*pos), member);  \
	//             &pos->member != (head);    \
	//                      pos = list_entry(pos->member.next, typeof(*pos), member))
	//
	//for (svn =  
	list_for_each_entry(svm, &static_vmlist, list) {
		vm = &svm->vm;
		addr = (unsigned long)vm->addr;
		if (addr < next)
			continue;

		/*
		 * Check if this vm starts on an odd section boundary.
		 * If so and the first section entry for this PMD is free
		 * then we block the corresponding virtual address.
		 */
		// #define PMD_SIZE        (1UL << PMD_SHIFT)
		// #define PMD_MASK        (~(PMD_SIZE-1))
		// #define SECTION_SHIFT       20
		// #define SECTION_SIZE        (1UL << SECTION_SHIFT)
		// (addr & ((1UL << 21) -1)) == (1UL << 20)
		// (addr & (0x1FFFFF)) == 0x100000
		// 
		if ((addr & ~PMD_MASK) == SECTION_SIZE) {
			pmd = pmd_off_k(addr);
			if (pmd_none(*pmd))
				pmd_empty_section_gap(addr & PMD_MASK);
		}

		/*
		 * Then check if this vm ends on an odd section boundary.
		 * If so and the second section entry for this PMD is empty
		 * then we block the corresponding virtual address.
		 */
		addr += vm->size;
		if ((addr & ~PMD_MASK) == SECTION_SIZE) {
			pmd = pmd_off_k(addr) + 1; // <- 확인 할 것
			if (pmd_none(*pmd))
				pmd_empty_section_gap(addr);
		}

		/* no need to look at any vm entry until we hit the next PMD */
		// 2M 단위로 이동
		// (addr + 0x1FFFFF) & 0xFFE00000
		next = (addr + PMD_SIZE - 1) & PMD_MASK;
	}
}

#else
#define fill_pmd_gaps() do { } while (0)
#endif

// 2014년 11월 01일
// PIC를 사용하지 않음
#if defined(CONFIG_PCI) && !defined(CONFIG_NEED_MACH_IO_H)
static void __init pci_reserve_io(void)
{
	struct static_vm *svm;

	svm = find_static_vm_vaddr((void *)PCI_IO_VIRT_BASE);
	if (svm)
		return;

	vm_reserve_area_early(PCI_IO_VIRT_BASE, SZ_2M, pci_reserve_io);
}
#else
#define pci_reserve_io() do { } while (0)
#endif

#ifdef CONFIG_DEBUG_LL
void __init debug_ll_io_init(void)
{
	struct map_desc map;

	debug_ll_addr(&map.pfn, &map.virtual);
	if (!map.pfn || !map.virtual)
		return;
	map.pfn = __phys_to_pfn(map.pfn);
	map.virtual &= PAGE_MASK;
	map.length = PAGE_SIZE;
	map.type = MT_DEVICE;
	iotable_init(&map, 1);
}
#endif

static void * __initdata vmalloc_min =
	(void *)(VMALLOC_END - (240 << 20) - VMALLOC_OFFSET);
//      (void *)(0xff000000UL - (240 << 20) - (8*1024*1024));
//               4080MB       -  240MB      - 8MB
//             = 3832MB
//      (void *)0xEF80_0000
// 8MB는 high memory와 normal memory사이의 보호를 위한 완충지대

/*
 * vmalloc=size forces the vmalloc area to be exactly 'size'
 * bytes. This can be used to increase (or decrease) the vmalloc
 * area - the default is 240m.
 */
static int __init early_vmalloc(char *arg)
{
	unsigned long vmalloc_reserve = memparse(arg, NULL);

	if (vmalloc_reserve < SZ_16M) {
		vmalloc_reserve = SZ_16M;
		printk(KERN_WARNING
			"vmalloc area too small, limiting to %luMB\n",
			vmalloc_reserve >> 20);
	}

	if (vmalloc_reserve > VMALLOC_END - (PAGE_OFFSET + SZ_32M)) {
		vmalloc_reserve = VMALLOC_END - (PAGE_OFFSET + SZ_32M);
		printk(KERN_WARNING
			"vmalloc area is too big, limiting to %luMB\n",
			vmalloc_reserve >> 20);
	}

	vmalloc_min = (void *)(VMALLOC_END - vmalloc_reserve);
	return 0;
}
early_param("vmalloc", early_vmalloc);

phys_addr_t arm_lowmem_limit __initdata = 0;

// memblock 구조체의 current limit 멤버 변수와
// high_memory,
// meminfo 구조체의 nr_banks 멤버 변수 설정
void __init sanity_check_meminfo(void)
{
	phys_addr_t memblock_limit = 0;
	int i, j, highmem = 0;
	phys_addr_t vmalloc_limit = __pa(vmalloc_min - 1) + 1; // __pa(0xEF80_0000 - 1) + 1;
	                                                       // __virt_to_phys((unsigned long)0xEF80_0000)
	                                                       // 가상 주소를 물리 주소로 변환
							       // 10차 26추차(2013.10.19) 후기
							       //  __va, __pa 사용 전에 -1을 하는 이유는 
							       // 이 값이 상한값(limit)이기 때문이다.
							       // 즉 배열의 범위(0 ~ (배열의 크기 - 1))와
							       // 같이 끝은 길이보다 하나가 적어야 하기 때문이다 

	for (i = 0, j = 0; i < meminfo.nr_banks; i++) {        // 뱅크 개수만큼 반복(1회만 실행)
		struct membank *bank = &meminfo.bank[j];       // 뱅크 주소를 구함, j는 meminfo.nr_banks 개수
		phys_addr_t size_limit;

		*bank = meminfo.bank[i];                       // 뱅크 j에 뱅크 i를 복사(덮어쓰기)
		size_limit = bank->size;                       // 크기를 구함

		if (bank->start >= vmalloc_limit)              // 뱅크 시작 주소가 범위 보다 크거나 같으면
			highmem = 1;                           // 하이 메모리로 설정
		else
			size_limit = vmalloc_limit - bank->start; // 범위 보다 작으면 
		                                                  // 크기를 다시 계산

		bank->highmem = highmem;  // 메모리 상태 표시

#ifdef CONFIG_HIGHMEM
		/*
		 * Split those memory banks which are partially overlapping
		 * the vmalloc area greatly simplifying things later.
		 */
		if (!highmem && bank->size > size_limit) {    // 하이메모리가 아니며 뱅크 크기가 
			                                      // 최대 메모리 크기보다 크다
			if (meminfo.nr_banks >= NR_BANKS) {   
				// NR_BANKS default 16 if ARCH_EP93XX
				//          default 8
				// 뱅크를 생성할 수 없음
				printk(KERN_CRIT "NR_BANKS too low, "
						 "ignoring high memory\n");
			} 
			else {
				// 새로운 뱅크를 생성
				memmove(bank + 1, bank,
					(meminfo.nr_banks - i) * sizeof(*bank));
				meminfo.nr_banks++;
				i++;
				// [1]은, 현재의 다음 것이 된다.
				// [1]로 고정된 것이 아니다.
				bank[1].size -= size_limit;
				bank[1].start = vmalloc_limit;
				bank[1].highmem = highmem = 1;
				j++;
			}
			bank->size = size_limit; // 뱅크 크기를 줄임
		}
#else
		/*
		 * Highmem banks not allowed with !CONFIG_HIGHMEM.
		 */
		if (highmem) {
			printk(KERN_NOTICE "Ignoring RAM at %.8llx-%.8llx "
			       "(!CONFIG_HIGHMEM).\n",
			       (unsigned long long)bank->start,
			       (unsigned long long)bank->start + bank->size - 1);
			continue;
		}

		/*
		 * Check whether this memory bank would partially overlap
		 * the vmalloc area.
		 */
		if (bank->size > size_limit) {
			printk(KERN_NOTICE "Truncating RAM at %.8llx-%.8llx "
			       "to -%.8llx (vmalloc region overlap).\n",
			       (unsigned long long)bank->start,
			       (unsigned long long)bank->start + bank->size - 1,
			       (unsigned long long)bank->start + size_limit - 1);
			bank->size = size_limit;
		}
#endif
		if (!bank->highmem) { // 하이 메모리(zone_highmem)가 아님
			phys_addr_t bank_end = bank->start + bank->size;

			if (bank_end > arm_lowmem_limit)     // bank_end > 0
				arm_lowmem_limit = bank_end; // 뱅크의 끝 주소로 하위 메모리의 최대를 변경

			/*
			 * Find the first non-section-aligned page, and point
			 * memblock_limit at it. This relies on rounding the
			 * limit down to be section-aligned, which happens at
			 * the end of this function.
			 *
			 * With this algorithm, the start or end of almost any
			 * bank can be non-section-aligned. The only exception
			 * is that the start of the bank 0 must be section-
			 * aligned, since otherwise memory would need to be
			 * allocated when mapping the start of bank 0, which
			 * occurs before any free memory is mapped.
			 */
			if (!memblock_limit) {     // 하이 메모리가 아니며 aligne 이 안되어
				                   // 있는 뱅크에서 실행
				if (!IS_ALIGNED(bank->start, SECTION_SIZE)) {
//                              if (!(((bank->start) & ((typeof(bank->start))(1<<20) - 1)) == 0))
//                                                                             1MB
					memblock_limit = bank->start; 
				}
				else if (!IS_ALIGNED(bank_end, SECTION_SIZE)) {
					memblock_limit = bank_end;
				}
			}
		}
		j++;
	} // fof (!highmem && bank->size > size_limit)r
#ifdef CONFIG_HIGHMEM
	if (highmem) { 
	// 하이 메모리이지만 Cortex A15는 PIPT로 아래의 조건에 맞지 않아 실행 되지 않음
	// PIPT(Physically Indexed, Physically Tagged)
	// VIPT(Virtually Indexed, Physically Tagged)
		     
		const char *reason = NULL;

		if (cache_is_vipt_aliasing()) { 
			/*
			 * Interactions between kmap and other mappings
			 * make highmem support with aliasing VIPT caches
			 * rather difficult.
			 */
			reason = "with VIPT aliasing cache";
		}
		if (reason) {
			printk(KERN_CRIT "HIGHMEM is not supported %s, ignoring high memory\n",
				reason);
			while (j > 0 && meminfo.bank[j - 1].highmem)
				j--;
		}
	}
#endif
	meminfo.nr_banks = j;
	high_memory = __va(arm_lowmem_limit - 1) + 1; // 물리 주소를 가상 주소로 변환
	// 2014-08-15, Blomdhal, high_memory가 중요한 이유는 VMALLOC_START macro에서 사용되기 때문이다.
	// #define VMALLOC_START       (((unsigned long)high_memory + VMALLOC_OFFSET) & ~(VMALLOC_OFFSET-1))

	/*
	 * Round the memblock limit down to a section size.  This
	 * helps to ensure that we will allocate memory from the
	 * last full section, which should be mapped.
	 */
	if (memblock_limit) // 
		memblock_limit = round_down(memblock_limit, SECTION_SIZE);
	if (!memblock_limit)
		memblock_limit = arm_lowmem_limit;

	memblock_set_current_limit(memblock_limit);
}

static inline void prepare_page_table(void)
{
	unsigned long addr;
	phys_addr_t end;

	/*
	 * Clear out all the mappings below the kernel image.
	 */ 
	 // MODULES_VADDR : PAGE_OFFSET - 16M
	 // PMD_SIZE : 2MB
	for (addr = 0; addr < MODULES_VADDR; addr += PMD_SIZE)
		// pmd_clear : 
		pmd_clear(pmd_off_k(addr));
	
#ifdef CONFIG_XIP_KERNEL
	/* The XIP kernel is mapped in the module area -- skip over it */
	addr = ((unsigned long)_etext + PMD_SIZE - 1) & PMD_MASK;
#endif
	// 결국 0~PAGE_OFFSET 까지 pmd_clear를 한다.
	for ( ; addr < PAGE_OFFSET; addr += PMD_SIZE)
		pmd_clear(pmd_off_k(addr));

	/*
	 * Find the end of the first block of lowmem.
	 */
	end = memblock.memory.regions[0].base + memblock.memory.regions[0].size;
	if (end >= arm_lowmem_limit)
		end = arm_lowmem_limit;

	/*
	 * Clear out all the kernel space mappings, except for the first
	 * memory bank, up to the vmalloc region.
	 */
	for (addr = __phys_to_virt(end);
	     addr < VMALLOC_START; addr += PMD_SIZE)
		pmd_clear(pmd_off_k(addr));
	/*
	 * addr 0 ~ pageoffset 까지와
	 * lowmem end~ vmlloc start 까지 pmd_clear 한다
	 * */
}

#ifdef CONFIG_ARM_LPAE
/* the first page is reserved for pgd */
#define SWAPPER_PG_DIR_SIZE	(PAGE_SIZE + \
				 PTRS_PER_PGD * PTRS_PER_PMD * sizeof(pmd_t))
#else
#define SWAPPER_PG_DIR_SIZE	(PTRS_PER_PGD * sizeof(pgd_t))
#endif

/*
 * Reserve the special regions of memory
 */
void __init arm_mm_memblock_reserve(void)
{
	/*
	 * Reserve the page tables.  These are already in use,
	 * and can only be in node 0.
	 */
	// #define SWAPPER_PG_DIR_SIZE (PTRS_PER_PGD * sizeof(pgd_t)) 
	// PTRS_PER_PGD = 2048
	// pgd_t = unsigned long  
	// SWAPPER_PG_DIR_SIZE = 2048 * 4 = 8192
	// #define swapper_pg_dir KERNEL_RAM_VADDR - PG_DIR_SIZE (head.S 에 .globl, .equ로 정의되어있음.   gnu assembly에서 .equ는 define과 같은의미)
	// KERNEL_RAM_VADDR = 0xc0008000
	//  PG_DIR_SIZE = 0x4000
	//  swapper_pg_dir = KERNEL_RAM_VADDR - PG_DIR_SIZE = 0xc0004000
	//  __pa(0xc0004000) = 0xc0004000 - 0xc0000000(page_offset) + 0x40000000(phys_offset) = 0x40004000
	//   따라서 memblock_reserve(0x40004000, 8192);
	// page globl directory 메모리 영역을 reserved 영역에 추가함  
	memblock_reserve(__pa(swapper_pg_dir), SWAPPER_PG_DIR_SIZE);

#ifdef CONFIG_SA1111
	/*
	 * Because of the SA1111 DMA bug, we want to preserve our
	 * precious DMA-able memory...
	 */
	memblock_reserve(PHYS_OFFSET, __pa(swapper_pg_dir) - PHYS_OFFSET);
#endif
}

/*
 * Set up the device mappings.  Since we clear out the page tables for all
 * mappings above VMALLOC_START, we will remove any debug device mappings.
 * This means you have to be careful how you debug this function, or any
 * called function.  This means you can't use any function or debugging
 * method which may touch any device, otherwise the kernel _will_ crash.
 */
static void __init devicemaps_init(const struct machine_desc *mdesc)
{
	struct map_desc map;
	unsigned long addr;
	void *vectors;

	/*
	 * Allocate the vector page early.
	 */
	vectors = early_alloc(PAGE_SIZE * 2);

	// 2014-10-25, 분석중
	early_trap_init(vectors);

	// 2014-11-01, 분석 시작
	// 페이지 디렉토리(PGD)의 각 엔트리를 순회하여 초기화를 함.
	// 먼저 PGD 엔트리의 인덱스를 구하기 위해 상위 11 비트(31:23)를 
	// 제외한 하위 비트를 모두 클리어하며
	// 이 때 시작 인덱스는 high_memory에서 0x800000를 더해서 구함
	// 
	// pmd_clear() 함수는 2개의 엔트리를 클리어하므로 인덱스를
	// 구하기 위해 0x20_0000(2MB)를 더함
	// for (add = (high_memory + 0x80_0000) & (!(0x80_0000 - 1)); 
	//      addr;
	//      addr += 0x20_0000[2MB])  
	for (addr = VMALLOC_START; addr; addr += PMD_SIZE)
		pmd_clear(pmd_off_k(addr));

	/*
	 * Map the kernel if it is XIP.
	 * It is always first in the modulearea.
	 */
#ifdef CONFIG_XIP_KERNEL // 정의 안됨
	map.pfn = __phys_to_pfn(CONFIG_XIP_PHYS_ADDR & SECTION_MASK);
	map.virtual = MODULES_VADDR;
	map.length = ((unsigned long)_etext - map.virtual + ~SECTION_MASK) & SECTION_MASK;
	map.type = MT_ROM;
	create_mapping(&map);
#endif

	/*
	 * Map the cache flushing regions.
	 */
	// mach-shark/include/mach/memory.h:24:#define FLUSH_BASE		0xdf000000
	// mach-footbridge/include/mach/memory.h:60:#define FLUSH_BASE		0xf9000000
	// mach-sa1100/include/mach/memory.h:38:#define FLUSH_BASE		0xf5000000
	// mach-rpc/include/mach/memory.h:30:#define FLUSH_BASE		        0xdf000000
	// mach-ebsa110/include/mach/memory.h:28:#define FLUSH_BASE		0xdf000000
	// 나머지는 사용안하는 것으로 보임
#ifdef FLUSH_BASE
	map.pfn = __phys_to_pfn(FLUSH_BASE_PHYS);
	map.virtual = FLUSH_BASE;
	map.length = SZ_1M;
	map.type = MT_CACHECLEAN;
	create_mapping(&map);
#endif
	// mach-sa1100/include/mach/memory.h:39:#define FLUSH_BASE_MINICACHE	0xf5100000
	// 나머지는 사용안하는 것으로 보임
#ifdef FLUSH_BASE_MINICACHE
	map.pfn = __phys_to_pfn(FLUSH_BASE_PHYS + SZ_1M);
	map.virtual = FLUSH_BASE_MINICACHE;
	map.length = SZ_1M;
	map.type = MT_MINICLEAN;
	create_mapping(&map);
#endif

	/*
	 * Create a mapping for the machine vectors at the high-vectors
	 * location (0xffff0000).  If we aren't using high-vectors, also
	 * create a mapping at the low-vectors virtual address.
	 */
	// PFN - Page Frame Number(or Node)
	// pfn을 구하기 위해 가상주소를 물리 주소를 먼저 변경 
	// 물리 주소의 상위 20 비트(31:12)인 4KB 범위만으로 
	// 페이지 프레임 인덱스를 구함
	// map.pfn = (virt_to_phys(vectors)) >> 12
	map.pfn = __phys_to_pfn(virt_to_phys(vectors)); 
	// vectors를 가상주소 0xffff_0000로 연결
	map.virtual = 0xffff0000;
	map.length = PAGE_SIZE; // 4KB
#ifdef CONFIG_KUSER_HELPERS
	// vectors를 HIGH_VECTORS로 지정
	map.type = MT_HIGH_VECTORS;
#else
	map.type = MT_LOW_VECTORS;
#endif
	create_mapping(&map);

	// #define vectors_high()  (cr_alignment & 13)
	// vectors를 0xFFFF_0000로 연결하지 못하면 0x00으로 연결
	// cr_alignment 변수?
	if (!vectors_high()) {
		map.virtual = 0;
		map.length = PAGE_SIZE * 2;
		map.type = MT_LOW_VECTORS;
		create_mapping(&map);
	}

	/* Now create a kernel read-only mapping */
	// vectors의 다음 페이지 프레임을 0xFFFF_0000으로 연결
	map.pfn += 1;
	map.virtual = 0xffff0000 + PAGE_SIZE;
	map.length = PAGE_SIZE;
	map.type = MT_LOW_VECTORS;
	create_mapping(&map);

	// 가상주소 0xFFFF_0000에 각 각 high vector와 low vector를 
	// 페이지 프레임에 연결

	/*
	 * Ask the machine support to map in the statically mapped devices.
	 */
	// map_io 콜백함수가 아래와 같이 지정
	// arch/arm/mach-exynos/common.c: 91:		.map_io		= exynos5_map_io,
	// exynos5_map_io() 함수는 struct map_desc exynos5_iodesc[] 구조체와
	// 구조체 크기를 인자로 iotable_init(struct map_desc*, int) 함수를 호출
	//
	if (mdesc->map_io)
		mdesc->map_io();
	else
		debug_ll_io_init();
	fill_pmd_gaps();

	/* Reserve fixed i/o space in VMALLOC region */
	// ARM arch PCI 미 지원
	pci_reserve_io();

	/*
	 * Finally flush the caches and tlb to ensure that we're in a
	 * consistent state wrt the writebuffer.  This also ensures that
	 * any write-allocated cache lines in the vector page are written
	 * back.  After this point, we can start to touch devices again.
	 */
	local_flush_tlb_all();
	
	// mm/cache-v7.S 파일의 ENTRY(v7_flush_kern_cache_all)로 연결
	flush_cache_all();
}

static void __init kmap_init(void)
{
#ifdef CONFIG_HIGHMEM	//y
	// include/uapi/linux/const.h:21:#define _AT(T,X)  ((T)(X))
	//
	// pmd_off_k(PAGE_OFFSET - PMD_SIZE)
	//
	// #define PMD_TYPE_TABLE      (_AT(pmdval_t, 1) << 0)
	// #define PMD_BIT4        (_AT(pmdval_t, 1) << 4)
	//
	// #define PMD_DOMAIN(x)       (_AT(pmdval_t, (x)) << 5) 
	// DOMAIN_KERNEL   0
	// PMD_DOMAIN(0)
	// #define _PAGE_KERNEL_TABLE  (PMD_TYPE_TABLE | PMD_BIT4 | PMD_DOMAIN(DOMAIN_KERNEL)) 
	// pkmap(persistent kernel map)
	//
	// pkmap_page_table = early_pte_alloc(
	//			pmd_off_k(0xC000_0000 + 0x0020_0000),
	//			0xC000_0000 + 0x0020_0000,
	//			0b0001_0001) // L1 코어스 페이지 테이블
	//			             // 도메인 사용 안함
	//
	// pkmap 하이 메모리의 페이지를 커널이 접근 가능하도록  정해진
	// 공간에 매핑제공
	// 메모리가 페이지 오프셋부터 2M에 위치에 있으며 여기는 유저 공간이지만
	// 유저에서는 접근 불가하며 커널에서만 접근가능하도록 되어있다 
	//
	// 코드로 알아보는 ARM 리눅스 커널 215쪽
	// PTE 마지막 엔트리를 하이 메모리를 관리하는데 할당
	pkmap_page_table = early_pte_alloc(pmd_off_k(PKMAP_BASE),
		PKMAP_BASE, _PAGE_KERNEL_TABLE);
#endif
}

static void __init map_lowmem(void)
{
	struct memblock_region *reg;

	/* Map all the lowmem memory banks. */
	/*memblock_type은 memory */
	for_each_memblock(memory, reg) {
		phys_addr_t start = reg->base;
		phys_addr_t end = start + reg->size;
		struct map_desc map;

		if (end > arm_lowmem_limit)
			end = arm_lowmem_limit;
		if (start >= end)
			break;

		map.pfn = __phys_to_pfn(start);
		map.virtual = __phys_to_virt(start);
		map.length = end - start;
		map.type = MT_MEMORY;

		create_mapping(&map);
	}
}

/*
 * paging_init() sets up the page tables, initialises the zone memory
 * maps, and sets up the zero page, bad page and bad page tables.
 */
void __init paging_init(const struct machine_desc *mdesc)
{
	void *zero_page;

	build_mem_type_table();
	//https://www.google.co.kr/search?q=linux+page+section+1mb&newwindow=1&espv=2&biw=1920&bih=979&source=lnms&tbm=isch&sa=X&ei=vDxGVI6AOuHImAWUm4HgAg&ved=0CAYQ_AUoAQ#imgdii=_
	//http://www.iamroot.org/xe/Kernel_10_ARM/176798
	prepare_page_table();
	// 2014-10-11, start 
	map_lowmem();
	// 2014-10-25, end

	// 2014-10-25, start
	dma_contiguous_remap();
	// 2014-10-25, end

	// 2014-10-25, start
	devicemaps_init(mdesc);
	// 2014-11-01, end

	// 2014-11-01, start, end
	kmap_init();

	// 2014-11-01, start, end
	// Cortex A15 TCM 지원 안함	
	tcm_init();

	// 가상주소를 0xFFFF_0000의 페이지 디렉토리(PGD)의 인덱스를 구함
	top_pmd = pmd_off_k(0xffff0000);

	/* allocate the zero page. */
	zero_page = early_alloc(PAGE_SIZE);

	// 2014-11-01, start
	bootmem_init();
	// 2015-01-24, 완료

	// 2015-01-24, 시작
	// 가상주소의 page를 구함
	empty_zero_page = virt_to_page(zero_page);
	__flush_dcache_page(NULL, empty_zero_page);
	// 2015-02-07, 끝
}
