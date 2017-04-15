/*
 *  arch/arm/include/asm/tlbflush.h
 *
 *  Copyright (C) 1999-2003 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _ASMARM_TLBFLUSH_H
#define _ASMARM_TLBFLUSH_H

#ifdef CONFIG_MMU // defined

#include <asm/glue.h>

#define TLB_V4_U_PAGE	(1 << 1)
#define TLB_V4_D_PAGE	(1 << 2)
#define TLB_V4_I_PAGE	(1 << 3)
#define TLB_V6_U_PAGE	(1 << 4)
#define TLB_V6_D_PAGE	(1 << 5)
#define TLB_V6_I_PAGE	(1 << 6)

#define TLB_V4_U_FULL	(1 << 9)
#define TLB_V4_D_FULL	(1 << 10)
#define TLB_V4_I_FULL	(1 << 11)
#define TLB_V6_U_FULL	(1 << 12)
#define TLB_V6_D_FULL	(1 << 13)
#define TLB_V6_I_FULL	(1 << 14)

#define TLB_V6_U_ASID	(1 << 16)
#define TLB_V6_D_ASID	(1 << 17)
#define TLB_V6_I_ASID	(1 << 18)

// 2016-12-17
#define TLB_V6_BP	(1 << 19)

/* Unified Inner Shareable TLB operations (ARMv7 MP extensions) */
#define TLB_V7_UIS_PAGE	(1 << 20)
#define TLB_V7_UIS_FULL (1 << 21)
#define TLB_V7_UIS_ASID (1 << 22)
// 2016-12-17
#define TLB_V7_UIS_BP	(1 << 23)

#define TLB_BARRIER	(1 << 28)
#define TLB_L2CLEAN_FR	(1 << 29)		/* Feroceon */
#define TLB_DCLEAN	(1 << 30)
#define TLB_WB		(1 << 31)

/*
 *	MMU TLB Model
 *	=============
 *
 *	We have the following to choose from:
 *	  v4    - ARMv4 without write buffer
 *	  v4wb  - ARMv4 with write buffer without I TLB flush entry instruction
 *	  v4wbi - ARMv4 with write buffer with I TLB flush entry instruction
 *	  fr    - Feroceon (v4wbi with non-outer-cacheable page table walks)
 *	  fa    - Faraday (v4 with write buffer with UTLB)
 *	  v6wbi - ARMv6 with write buffer with I TLB flush entry instruction
 *	  v7wbi - identical to v6wbi
 */
#undef _TLB
#undef MULTI_TLB

#ifdef CONFIG_SMP_ON_UP // set
#define MULTI_TLB 1
#endif

#define v4_tlb_flags	(TLB_V4_U_FULL | TLB_V4_U_PAGE)

#ifdef CONFIG_CPU_TLB_V4WT  // not set
# define v4_possible_flags	v4_tlb_flags
# define v4_always_flags	v4_tlb_flags
# ifdef _TLB
#  define MULTI_TLB 1
# else
#  define _TLB v4
# endif
#else
// 2015-08-29
# define v4_possible_flags	0
// 2015-08-29
# define v4_always_flags	(-1UL)
#endif

#define fa_tlb_flags	(TLB_WB | TLB_DCLEAN | TLB_BARRIER | \
			 TLB_V4_U_FULL | TLB_V4_U_PAGE)

#ifdef CONFIG_CPU_TLB_FA
# define fa_possible_flags	fa_tlb_flags
# define fa_always_flags	fa_tlb_flags
# ifdef _TLB
#  define MULTI_TLB 1
# else
#  define _TLB fa
# endif
#else
// 2015-08-29
# define fa_possible_flags	0
// 2015-08-29
# define fa_always_flags	(-1UL)
#endif

#define v4wbi_tlb_flags	(TLB_WB | TLB_DCLEAN | \
			 TLB_V4_I_FULL | TLB_V4_D_FULL | \
			 TLB_V4_I_PAGE | TLB_V4_D_PAGE)

#ifdef CONFIG_CPU_TLB_V4WBI // not set
# define v4wbi_possible_flags	v4wbi_tlb_flags
# define v4wbi_always_flags	v4wbi_tlb_flags
# ifdef _TLB
#  define MULTI_TLB 1
# else
#  define _TLB v4wbi
# endif
#else
// 2015-08-29
# define v4wbi_possible_flags	0
// 2015-08-29
# define v4wbi_always_flags	(-1UL)
#endif

#define fr_tlb_flags	(TLB_WB | TLB_DCLEAN | TLB_L2CLEAN_FR | \
			 TLB_V4_I_FULL | TLB_V4_D_FULL | \
			 TLB_V4_I_PAGE | TLB_V4_D_PAGE)

#ifdef CONFIG_CPU_TLB_FEROCEON
# define fr_possible_flags	fr_tlb_flags
# define fr_always_flags	fr_tlb_flags
# ifdef _TLB
#  define MULTI_TLB 1
# else
#  define _TLB v4wbi
# endif
#else
// 2015-08-29
# define fr_possible_flags	0
// 2015-08-29
# define fr_always_flags	(-1UL)
#endif

#define v4wb_tlb_flags	(TLB_WB | TLB_DCLEAN | \
			 TLB_V4_I_FULL | TLB_V4_D_FULL | \
			 TLB_V4_D_PAGE)

#ifdef CONFIG_CPU_TLB_V4WB
# define v4wb_possible_flags	v4wb_tlb_flags
# define v4wb_always_flags	v4wb_tlb_flags
# ifdef _TLB
#  define MULTI_TLB 1
# else
#  define _TLB v4wb
# endif
#else
// 2015-08-29
# define v4wb_possible_flags	0
// 2015-08-29
# define v4wb_always_flags	(-1UL)
#endif

#define v6wbi_tlb_flags (TLB_WB | TLB_DCLEAN | TLB_BARRIER | \
			 TLB_V6_I_FULL | TLB_V6_D_FULL | \
			 TLB_V6_I_PAGE | TLB_V6_D_PAGE | \
			 TLB_V6_I_ASID | TLB_V6_D_ASID | \
			 TLB_V6_BP)

#ifdef CONFIG_CPU_TLB_V6
# define v6wbi_possible_flags	v6wbi_tlb_flags
# define v6wbi_always_flags	v6wbi_tlb_flags
# ifdef _TLB
#  define MULTI_TLB 1
# else
#  define _TLB v6wbi
# endif
#else
// 2015-08-29
# define v6wbi_possible_flags	0
// 2015-08-29
# define v6wbi_always_flags	(-1UL)
#endif

// 2015-08-29
#define v7wbi_tlb_flags_smp	(TLB_WB | TLB_BARRIER | \
				 TLB_V7_UIS_FULL | TLB_V7_UIS_PAGE | \
				 TLB_V7_UIS_ASID | TLB_V7_UIS_BP)
// 2015-08-29
#define v7wbi_tlb_flags_up	(TLB_WB | TLB_DCLEAN | TLB_BARRIER | \
				 TLB_V6_U_FULL | TLB_V6_U_PAGE | \
				 TLB_V6_U_ASID | TLB_V6_BP)

#ifdef CONFIG_CPU_TLB_V7    // set

# ifdef CONFIG_SMP_ON_UP    // set
// 현재 프로세서 설정 값
// CONFIG_SMP_ON_UP, CONFIG_SMP 두 값이 설정이 되어있지만
// 첫 번째 항목이 먼저 있기 때문에 해당 위치의 플래그가 세팅된다.
// DCLEAN config
// 2015-08-29
#  define v7wbi_possible_flags	(v7wbi_tlb_flags_smp | v7wbi_tlb_flags_up)
// DCLEAN nonConfig
// 2015-08-29
// => TLB_WB | TLB_BARRIER
#  define v7wbi_always_flags	(v7wbi_tlb_flags_smp & v7wbi_tlb_flags_up)

# elif defined(CONFIG_SMP)
#  define v7wbi_possible_flags	v7wbi_tlb_flags_smp
#  define v7wbi_always_flags	v7wbi_tlb_flags_smp
# else
#  define v7wbi_possible_flags	v7wbi_tlb_flags_up
#  define v7wbi_always_flags	v7wbi_tlb_flags_up
# endif

# ifdef _TLB
#  define MULTI_TLB 1
# else
// 2015-01-31, here, _TLB is not setted, but MULTI_TLB is setted by 64 line(CONFIG_SMP_ON_UP) 
#  define _TLB v7wbi
# endif
#else
# define v7wbi_possible_flags	0
# define v7wbi_always_flags	(-1UL)
#endif

#ifndef _TLB
#error Unknown TLB model
#endif

#ifndef __ASSEMBLY__

#include <linux/sched.h>

// 2016-12-17
struct cpu_tlb_fns {
	void (*flush_user_range)(unsigned long, unsigned long, struct vm_area_struct *);
	void (*flush_kern_range)(unsigned long, unsigned long);
	unsigned long tlb_flags;
};

/*
 * Select the calling method
 */
#ifdef MULTI_TLB

#define __cpu_flush_user_tlb_range	cpu_tlb.flush_user_range
#define __cpu_flush_kern_tlb_range	cpu_tlb.flush_kern_range

#else

#define __cpu_flush_user_tlb_range	__glue(_TLB,_flush_user_tlb_range)
// 2016-04-23
// v7wbi_flush_kern_tlb_range
#define __cpu_flush_kern_tlb_range	__glue(_TLB,_flush_kern_tlb_range)

extern void __cpu_flush_user_tlb_range(unsigned long, unsigned long, struct vm_area_struct *);
extern void __cpu_flush_kern_tlb_range(unsigned long, unsigned long);

#endif

extern struct cpu_tlb_fns cpu_tlb;

// 2015-08-22
#define __cpu_tlb_flags			cpu_tlb.tlb_flags

/*
 *	TLB Management
 *	==============
 *
 *	The arch/arm/mm/tlb-*.S files implement these methods.
 *
 *	The TLB specific code is expected to perform whatever tests it
 *	needs to determine if it should invalidate the TLB for each
 *	call.  Start addresses are inclusive and end addresses are
 *	exclusive; it is safe to round these addresses down.
 *
 *	flush_tlb_all()
 *
 *		Invalidate the entire TLB.
 *
 *	flush_tlb_mm(mm)
 *
 *		Invalidate all TLB entries in a particular address
 *		space.
 *		- mm	- mm_struct describing address space
 *
 *	flush_tlb_range(mm,start,end)
 *
 *		Invalidate a range of TLB entries in the specified
 *		address space.
 *		- mm	- mm_struct describing address space
 *		- start - start address (may not be aligned)
 *		- end	- end address (exclusive, may not be aligned)
 *
 *	flush_tlb_page(vaddr,vma)
 *
 *		Invalidate the specified page in the specified address range.
 *		- vaddr - virtual address (may not be aligned)
 *		- vma	- vma_struct describing address range
 *
 *	flush_kern_tlb_page(kaddr)
 *
 *		Invalidate the TLB entry for the specified page.  The address
 *		will be in the kernels virtual memory space.  Current uses
 *		only require the D-TLB to be invalidated.
 *		- kaddr - Kernel virtual memory address
 */

/*
 * We optimise the code below by:
 *  - building a set of TLB flags that might be set in __cpu_tlb_flags
 *  - building a set of TLB flags that will always be set in __cpu_tlb_flags
 *  - if we're going to need __cpu_tlb_flags, access it once and only once
 *
 * This allows us to build optimal assembly for the single-CPU type case,
 * and as close to optimal given the compiler constrants for multi-CPU
 * case.  We could do better for the multi-CPU case if the compiler
 * implemented the "%?" method, but this has been discontinued due to too
 * many people getting it wrong.
 */
 // config 옵션이 걸리지 않은 경우는 0으로 설정
 // 0 | .... my_config_flag == my_config_flag만 남을 것이다. 
 // 2015-08-29
 // TLB_WB | TLB_BARRIER | TLB_V7_UIS_FULL | TLB_V7_UIS_PAGE | TLB_V7_UIS_ASID | TLB_V7_UIS_BP
 // TLB_WB | TLB_DCLEAN | TLB_BARRIER | TLB_V6_U_FULL | TLB_V6_U_PAGE | TLB_V6_U_ASID | TLB_V6_BP
 // 2016-12-17
#define possible_tlb_flags	(v4_possible_flags/*0*/ | \
				 v4wbi_possible_flags /*0*/ | \
				 fr_possible_flags/*0*/  | \
				 v4wb_possible_flags/*0*/  | \
				 fa_possible_flags/*0*/  | \
				 v6wbi_possible_flags/*0*/  | \
				 v7wbi_possible_flags/*가능한 flag 모두*/)

// config옵션이 걸리지 않은 경우는 (-1)UL로 설정
// (-1)UL & .... my_config_flag == my_config_flag만 남을 것이다.
// 2015-08-29
// 결과적으로, TLB_WB | TLB_BARRIER
#define always_tlb_flags	(v4_always_flags/*0xFFFF_FFFF, -1UL*/ & \
				 v4wbi_always_flags/*0xFFFF_FFFF, -1UL*/ & \
				 fr_always_flags /*0xFFFF_FFFF, -1UL*/& \
				 v4wb_always_flags /*0xFFFF_FFFF, -1UL*/& \
				 fa_always_flags /*0xFFFF_FFFF, -1UL*/& \
				 v6wbi_always_flags /*0xFFFF_FFFF, -1UL*/& \
				 v7wbi_always_flags/*TLB_WB | TLB_BARRIER*/)

// 2015-08-29
// 2016-12-17
// tlb_flag(TLB_V6_BP) // __tlb_flag = cpu_tlb.tlb_flags;
// tlb_flag(TLB_V7_UIS_BP) // __tlb_flag = __cpu_tlb_flags;
//
// '__tlb_flag'는 매크로를 호출하는 함수 안의 지역변수이다.
#define tlb_flag(f)	((always_tlb_flags/*TLB_WB | TLB_BARRIER*/ & (f)) || (__tlb_flag & possible_tlb_flags & (f)))

	/* f == tlb_DCLEAN, tlb_L2CLEAN
	 arg => [0] = 0, [1] = 0이 세팅된 주소가 넘어온다.
     */                                         
	/* if(alway .. ) tlb_DCLEAN, tlb_L2CLEAN 미설정 */  
    /* else if(..) 
    %? -> inline assembly의 인자 항목에서 ?번째의 인자를 말함
     __tlb_flag : 매크로 함수를 부르기 전에 미리 정의한 로컬변수
     tst __tlb_flag, f
     mcrne "전달받은 명령어"
     tlb_DCLEAN 설정 인자 전달 시 해당 명령문 실행
     cotex-a15 mpcore 4-8 DCCMVAC
     Clean data cache line by MVA to PoC
     mcrne    p15, 0, %0, c7, c10, 1 @ flush_pmd
     mcr      coproc, op1, Rt, Crn, Crm, op2
     %0 = pmd
     %1 = __tlb_flag
     %2 =  TLB_DCLEAN
     poc, pou, mva 관련내용은 옛날에도 했던것이며 wiki 10차
     poc로 검색하면 잘나옴
     */
// 2016-12-17
#define __tlb_op(f, insnarg, arg)					\
	do {								\
		if (always_tlb_flags & (f))				\
			asm("mcr " insnarg				\
			    : : "r" (arg) : "cc");			\
		else if (possible_tlb_flags & (f))      \
            /* '__tlb_flag'와 'f'가 같은지 먼저 확인 */ \
			asm("tst %1, %2\n\t"				\
			    "mcrne " insnarg				\
			    : : "r" (arg), "r" (__tlb_flag), "Ir" (f)	\
			    : "cc");					\
	} while (0)

#define tlb_op(f, regs, arg)	__tlb_op(f, "p15, 0, %0, " regs, arg)
#define tlb_l2_op(f, regs, arg)	__tlb_op(f, "p15, 1, %0, " regs, arg)

// 2016-12-17
static inline void __local_flush_tlb_all(void)
{
	const int zero = 0;
	const unsigned int __tlb_flag = __cpu_tlb_flags;

    // #define TLB_V4_U_FULL   (1 << 9)
    // #define TLB_V4_D_FULL   (1 << 10)
    // #define TLB_V4_I_FULL   (1 << 11)
    // #define TLB_V6_U_FULL   (1 << 12)
    // #define TLB_V6_D_FULL   (1 << 13)
    // #define TLB_V6_I_FULL   (1 << 14)
    // Invalidate unified TLB
	tlb_op(TLB_V4_U_FULL | TLB_V6_U_FULL, "c8, c7, 0", zero);
    // Invalidate data TLB 
	tlb_op(TLB_V4_D_FULL | TLB_V6_D_FULL, "c8, c6, 0", zero);
    // Invalidate instruction TLB
	tlb_op(TLB_V4_I_FULL | TLB_V6_I_FULL, "c8, c5, 0", zero);
}

// 2014년 11월 01일
// 2016-12-17
static inline void local_flush_tlb_all(void)
{
	const int zero = 0;
    // #define __cpu_tlb_flags         cpu_tlb.tlb_flags
	const unsigned int __tlb_flag = __cpu_tlb_flags;

    // write back을 지원하면 캐쉬에 남아 있는 데이터(명령어)를 
    // 모두 쓸때까지 대기
    // http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0204ik/CIHJFGFE.html
	if (tlb_flag(TLB_WB))
		dsb(nshst); // dsb 명령어의 nshst 옵션

	__local_flush_tlb_all();
    // #define TLB_V7_UIS_FULL (1 << 21)
    // // Invalidate unified TLB
	tlb_op(TLB_V7_UIS_FULL, "c8, c7, 0", zero);

    // #define TLB_BARRIER (1 << 28)
	if (tlb_flag(TLB_BARRIER)) {
		dsb(nsh);
        // 파이프를 비움
		isb();
	}
}

static inline void __flush_tlb_all(void)
{
	const int zero = 0;
	const unsigned int __tlb_flag = __cpu_tlb_flags;

	if (tlb_flag(TLB_WB))
		dsb(ishst);

	__local_flush_tlb_all();
	tlb_op(TLB_V7_UIS_FULL, "c8, c3, 0", zero);

	if (tlb_flag(TLB_BARRIER)) {
		dsb(ish);
		isb();
	}
}

// 2017-04-15
static inline void __local_flush_tlb_mm(struct mm_struct *mm)
{
	const int zero = 0;
	const int asid = ASID(mm);
	const unsigned int __tlb_flag = __cpu_tlb_flags;

	if (possible_tlb_flags & (TLB_V4_U_FULL|TLB_V4_D_FULL|TLB_V4_I_FULL)) {
		if (cpumask_test_cpu(smp_processor_id(), mm_cpumask(mm))) {
			tlb_op(TLB_V4_U_FULL, "c8, c7, 0", zero);
			tlb_op(TLB_V4_D_FULL, "c8, c6, 0", zero);
			tlb_op(TLB_V4_I_FULL, "c8, c5, 0", zero);
		}
	}

	tlb_op(TLB_V6_U_ASID, "c8, c7, 2", asid);
	tlb_op(TLB_V6_D_ASID, "c8, c6, 2", asid);
	tlb_op(TLB_V6_I_ASID, "c8, c5, 2", asid);
}

static inline void local_flush_tlb_mm(struct mm_struct *mm)
{
	const int asid = ASID(mm);
	const unsigned int __tlb_flag = __cpu_tlb_flags;

	if (tlb_flag(TLB_WB))
		dsb(nshst);

	__local_flush_tlb_mm(mm);
	tlb_op(TLB_V7_UIS_ASID, "c8, c7, 2", asid);

	if (tlb_flag(TLB_BARRIER))
		dsb(nsh);
}

// 2017-04-15
static inline void __flush_tlb_mm(struct mm_struct *mm)
{
	const unsigned int __tlb_flag = __cpu_tlb_flags;

	if (tlb_flag(TLB_WB))
		dsb(ishst);

	__local_flush_tlb_mm(mm);
#ifdef CONFIG_ARM_ERRATA_720789
	tlb_op(TLB_V7_UIS_ASID, "c8, c3, 0", 0);
#else
	tlb_op(TLB_V7_UIS_ASID, "c8, c3, 2", ASID(mm));
#endif

	if (tlb_flag(TLB_BARRIER))
		dsb(ish);
}

// 2015-08-29
static inline void
__local_flush_tlb_page(struct vm_area_struct *vma, unsigned long uaddr)
{
	const int zero = 0;
	const unsigned int __tlb_flag = __cpu_tlb_flags;

	uaddr = (uaddr & PAGE_MASK) | ASID(vma->vm_mm);

    // TLB_WB | TLB_BARRIER | TLB_V7_UIS_FULL | TLB_V7_UIS_PAGE | TLB_V7_UIS_ASID | TLB_V7_UIS_BP
    // TLB_WB | TLB_DCLEAN | TLB_BARRIER | TLB_V6_U_FULL | TLB_V6_U_PAGE | TLB_V6_U_ASID | TLB_V6_BP
    // false
	if (possible_tlb_flags & (TLB_V4_U_PAGE|TLB_V4_D_PAGE|TLB_V4_I_PAGE|TLB_V4_I_FULL) &&
	    cpumask_test_cpu(smp_processor_id(), mm_cpumask(vma->vm_mm))) {
		tlb_op(TLB_V4_U_PAGE, "c8, c7, 1", uaddr);
		tlb_op(TLB_V4_D_PAGE, "c8, c6, 1", uaddr);
		tlb_op(TLB_V4_I_PAGE, "c8, c5, 1", uaddr);
		if (!tlb_flag(TLB_V4_I_PAGE) && tlb_flag(TLB_V4_I_FULL))
			asm("mcr p15, 0, %0, c8, c5, 0" : : "r" (zero) : "cc");
	}

    // #define tlb_op(f, regs, arg)    __tlb_op(f, "p15, 0, %0, " regs, arg)
    //  __tlb_op(TLB_V6_U_PAGE, "p15, 0, %0, " "c8, c7, 1", uaddr)
    //
    // #define __tlb_op(f, insnarg, arg)                   \
    //     do {                                \
    //         if (always_tlb_flags & (f))             \
    //             asm("mcr " insnarg              \
    //                 : : "r" (arg) : "cc");          \
    //         else if (possible_tlb_flags & (f))      \
    //             /* '__tlb_flag'와 'f'가 같은지 먼저 확인 */ \
    //             asm("tst %1, %2\n\t"                \
    //                 "mcrne " insnarg                \
    //                 : : "r" (arg), "r" (__tlb_flag), "Ir" (f)   \
    //                 : "cc");                    \
    //     } while (0)
    //  
    // uaddr을 코프로세서에 write
	tlb_op(TLB_V6_U_PAGE, "c8, c7, 1", uaddr); // Invalidate unified TLB entry by MVA and ASID

	tlb_op(TLB_V6_D_PAGE, "c8, c6, 1", uaddr);  // NO OP
	tlb_op(TLB_V6_I_PAGE, "c8, c5, 1", uaddr);  // NO OP
}

static inline void
local_flush_tlb_page(struct vm_area_struct *vma, unsigned long uaddr)
{
	const unsigned int __tlb_flag = __cpu_tlb_flags;

	uaddr = (uaddr & PAGE_MASK) | ASID(vma->vm_mm);

	if (tlb_flag(TLB_WB))
		dsb(nshst);

	__local_flush_tlb_page(vma, uaddr);
	tlb_op(TLB_V7_UIS_PAGE, "c8, c7, 1", uaddr);

	if (tlb_flag(TLB_BARRIER))
		dsb(nsh);
}

// 2015-08-22
static inline void
__flush_tlb_page(struct vm_area_struct *vma, unsigned long uaddr)
{
	const unsigned int __tlb_flag = __cpu_tlb_flags;

    // 상위 12비트 이상의 값 + 하위 1byte 값이 더해진 값이다.
	uaddr = (uaddr & PAGE_MASK) | ASID(vma->vm_mm);
    // 2015-08-22, 여기까지

    // TLB_WB | TLB_BARRIER 임으로 true
    // 실질적인 flush 동작 이전에 수행해야할 명령들 수행
	if (tlb_flag(TLB_WB))
		dsb(ishst); // ishst, write

    // 2015-08-29
	__local_flush_tlb_page(vma, uaddr);
#ifdef CONFIG_ARM_ERRATA_720789     // not set
	tlb_op(TLB_V7_UIS_PAGE, "c8, c3, 3", uaddr & PAGE_MASK);
#else
    // 2015-08-29
	tlb_op(TLB_V7_UIS_PAGE, "c8, c3, 1", uaddr);
#endif

    // TLB_WB | TLB_BARRIER 임으로 true
	if (tlb_flag(TLB_BARRIER))
		dsb(ish);   // ish, read and write
}

// 2015-01-31, kaddr은 PAGE_MASK를 씌운 값이다.
static inline void __local_flush_tlb_kernel_page(unsigned long kaddr)
{
	const int zero = 0;
	const unsigned int __tlb_flag = __cpu_tlb_flags;

	tlb_op(TLB_V4_U_PAGE, "c8, c7, 1", kaddr);
	tlb_op(TLB_V4_D_PAGE, "c8, c6, 1", kaddr);
	tlb_op(TLB_V4_I_PAGE, "c8, c5, 1", kaddr);
	if (!tlb_flag(TLB_V4_I_PAGE) && tlb_flag(TLB_V4_I_FULL))
		asm("mcr p15, 0, %0, c8, c5, 0" : : "r" (zero) : "cc");

	tlb_op(TLB_V6_U_PAGE, "c8, c7, 1", kaddr);
	tlb_op(TLB_V6_D_PAGE, "c8, c6, 1", kaddr);
	tlb_op(TLB_V6_I_PAGE, "c8, c5, 1", kaddr);
}

// 2015-01-31
// 2015-09-19;
static inline void local_flush_tlb_kernel_page(unsigned long kaddr)
{
	const unsigned int __tlb_flag = __cpu_tlb_flags;

	kaddr &= PAGE_MASK;     // 0xFFFF_F000

    // dsb(nshst) : 저장이 완료 될때까지만, 기다리고,
    // 통합 지점만 수행되는 dsb작업
    //
    // TLB가 Write Back이라면, dsb작업을 한다.
	if (tlb_flag(TLB_WB))
		dsb(nshst);

	__local_flush_tlb_kernel_page(kaddr);
	tlb_op(TLB_V7_UIS_PAGE, "c8, c7, 1", kaddr);

	if (tlb_flag(TLB_BARRIER)) {
		dsb(nsh);   // 통합 지점까지만, 수행되는 dsb 작업
		isb();      // 이전 명령어 파이프라인 플러시
	}
}

static inline void __flush_tlb_kernel_page(unsigned long kaddr)
{
	const unsigned int __tlb_flag = __cpu_tlb_flags;

	kaddr &= PAGE_MASK;

	if (tlb_flag(TLB_WB))
		dsb(ishst);

	__local_flush_tlb_kernel_page(kaddr);
	tlb_op(TLB_V7_UIS_PAGE, "c8, c3, 1", kaddr);

	if (tlb_flag(TLB_BARRIER)) {
		dsb(ish);
		isb();
	}
}

/*
 * Branch predictor maintenance is paired with full TLB invalidation, so
 * there is no need for any barriers here.
 */
// 2016-12-17
static inline void __local_flush_bp_all(void)
{
	const int zero = 0;
	const unsigned int __tlb_flag = __cpu_tlb_flags;

	if (tlb_flag(TLB_V6_BP)) // true
        // BPIALL - Invalidate all branch predictors
        // 분기 예측을 무효화함 
		asm("mcr p15, 0, %0, c7, c5, 6" : : "r" (zero));
}

// 2016-12-17
static inline void local_flush_bp_all(void)
{
	const int zero = 0;
	const unsigned int __tlb_flag = __cpu_tlb_flags;

	__local_flush_bp_all();
	if (tlb_flag(TLB_V7_UIS_BP)) // true
        // 분기 예측을 무효화함 
		asm("mcr p15, 0, %0, c7, c5, 6" : : "r" (zero));
}

static inline void __flush_bp_all(void)
{
	const int zero = 0;
	const unsigned int __tlb_flag = __cpu_tlb_flags;

	__local_flush_bp_all();
	if (tlb_flag(TLB_V7_UIS_BP))
		asm("mcr p15, 0, %0, c7, c1, 6" : : "r" (zero));
}

#include <asm/cputype.h>
#ifdef CONFIG_ARM_ERRATA_798181 // not set
static inline int erratum_a15_798181(void)
{
	unsigned int midr = read_cpuid_id();

	/* Cortex-A15 r0p0..r3p2 affected */
	if ((midr & 0xff0ffff0) != 0x410fc0f0 || midr > 0x413fc0f2)
		return 0;
	return 1;
}

static inline void dummy_flush_tlb_a15_erratum(void)
{
	/*
	 * Dummy TLBIMVAIS. Using the unmapped address 0 and ASID 0.
	 */
	asm("mcr p15, 0, %0, c8, c3, 1" : : "r" (0));
	dsb(ish);
}
#else
// 2015-08-29
static inline int erratum_a15_798181(void)
{
	return 0;
}

// 2015-08-29
static inline void dummy_flush_tlb_a15_erratum(void)
{
}
#endif

/*
 *	flush_pmd_entry
 *
 *	Flush a PMD entry (word aligned, or double-word aligned) to
 *	RAM if the TLB for the CPU we are running on requires this.
 *	This is typically used when we are creating PMD entries.
 *
 *	clean_pmd_entry
 *
 *	Clean (but don't drain the write buffer) if the CPU requires
 *	these operations.  This is typically used when we are removing
 *	PMD entries.
 */
// [flush] 캐시라인을 비움(0으로 초기화)
// 2016-04-16
// flush_pmd_entry(pmdp)
static inline void flush_pmd_entry(void *pmd)
{
	const unsigned int __tlb_flag = __cpu_tlb_flags;
	// mcr p15, 0, pmd, c7, c10, 1  @ flush_pmd
    // clean data cache 
	tlb_op(TLB_DCLEAN, "c7, c10, 1	@ flush_pmd", pmd);

	// TLB_L2CLEAN_FR에 해당 명령어는 실행되지 않는다.
    // mcr p15, 1, pmd, c15, c9, 1  @ L2 flush_pmd
	tlb_l2_op(TLB_L2CLEAN_FR, "c15, c9, 1  @ L2 flush_pmd", pmd);

	if (tlb_flag(TLB_WB))
		// 저장이 완료될 때까지만 기다리고 공유 가능한 내부 도메인까지 수행되는 dsb 작업
		// 캐시 내 명령어가 완수될 때까지 대기
        // 보충 - 2014-11-03, 양만철
        // 참고:  http://goo.gl/pcO5xq
        // 함수 이름에 접두어로 'flush'가 붙어있으며 아래는 'clean'이 붙어있는데
        // 접두어와 상관없이 캐기라인을 비우기 전 더티비트를 검사하여 
        // 강제로 메모리에 쓰기를 완료할 때까지 기다리는것으로 보임
		dsb(ishst);
}

// [clean] 캐시라인에서 더티비트를 검사하여 주 메모리에
// 강제로 쓴 후 캐시라인을 비움(0으로 초기화)
static inline void clean_pmd_entry(void *pmd)
{
	const unsigned int __tlb_flag = __cpu_tlb_flags;

    //          2        1    3   4
    // mcr p15, 0, pmd,  c7, c10, 1  @ flush_pmd
    // clean data cache
	tlb_op(TLB_DCLEAN, "c7, c10, 1	@ flush_pmd", pmd);
	// 2014.09.27 end
   
    // mcr p15, 1, pmd, c15, c9, 1  @ L2 flush_pmd
    // ? 
	tlb_l2_op(TLB_L2CLEAN_FR, "c15, c9, 1  @ L2 flush_pmd", pmd);

}

#undef tlb_op
#undef tlb_flag
#undef always_tlb_flags
#undef possible_tlb_flags

/*
 * Convert calls to our calling convention.
 */
#define local_flush_tlb_range(vma,start,end)	__cpu_flush_user_tlb_range(start,end,vma)
// 2016-04-23
#define local_flush_tlb_kernel_range(s,e)	__cpu_flush_kern_tlb_range(s,e)

#ifndef CONFIG_SMP // CONFIG_SMP = y
#define flush_tlb_all		local_flush_tlb_all
#define flush_tlb_mm		local_flush_tlb_mm
#define flush_tlb_page		local_flush_tlb_page
#define flush_tlb_kernel_page	local_flush_tlb_kernel_page
#define flush_tlb_range		local_flush_tlb_range
#define flush_tlb_kernel_range	local_flush_tlb_kernel_range
#define flush_bp_all		local_flush_bp_all
#else
extern void flush_tlb_all(void);
extern void flush_tlb_mm(struct mm_struct *mm);
extern void flush_tlb_page(struct vm_area_struct *vma, unsigned long uaddr);
extern void flush_tlb_kernel_page(unsigned long kaddr);
extern void flush_tlb_range(struct vm_area_struct *vma, unsigned long start, unsigned long end);
extern void flush_tlb_kernel_range(unsigned long start, unsigned long end);
extern void flush_bp_all(void);
#endif

/*
 * If PG_dcache_clean is not set for the page, we need to ensure that any
 * cache entries for the kernels virtual memory range are written
 * back to the page. On ARMv6 and later, the cache coherency is handled via
 * the set_pte_at() function.
 */
#if __LINUX_ARM_ARCH__ < 6
extern void update_mmu_cache(struct vm_area_struct *vma, unsigned long addr,
	pte_t *ptep);
#else
static inline void update_mmu_cache(struct vm_area_struct *vma,
				    unsigned long addr, pte_t *ptep)
{
}
#endif

#define update_mmu_cache_pmd(vma, address, pmd) do { } while (0)

#endif

#elif defined(CONFIG_SMP)	/* !CONFIG_MMU */

#ifndef __ASSEMBLY__

#include <linux/mm_types.h>

static inline void local_flush_tlb_all(void)									{ }
static inline void local_flush_tlb_mm(struct mm_struct *mm)							{ }
static inline void local_flush_tlb_page(struct vm_area_struct *vma, unsigned long uaddr)			{ }
static inline void local_flush_tlb_kernel_page(unsigned long kaddr)						{ }
static inline void local_flush_tlb_range(struct vm_area_struct *vma, unsigned long start, unsigned long end)	{ }
static inline void local_flush_tlb_kernel_range(unsigned long start, unsigned long end)				{ }
static inline void local_flush_bp_all(void)									{ }

extern void flush_tlb_all(void);
extern void flush_tlb_mm(struct mm_struct *mm);
extern void flush_tlb_page(struct vm_area_struct *vma, unsigned long uaddr);
extern void flush_tlb_kernel_page(unsigned long kaddr);
extern void flush_tlb_range(struct vm_area_struct *vma, unsigned long start, unsigned long end);
extern void flush_tlb_kernel_range(unsigned long start, unsigned long end);
extern void flush_bp_all(void);
#endif	/* __ASSEMBLY__ */

#endif

#endif
