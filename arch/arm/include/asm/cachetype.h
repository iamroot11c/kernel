#ifndef __ASM_ARM_CACHETYPE_H
#define __ASM_ARM_CACHETYPE_H

#define CACHEID_VIVT			(1 << 0)
#define CACHEID_VIPT_NONALIASING	(1 << 1)
#define CACHEID_VIPT_ALIASING		(1 << 2)
#define CACHEID_VIPT			(CACHEID_VIPT_ALIASING|CACHEID_VIPT_NONALIASING)
#define CACHEID_ASID_TAGGED		(1 << 3)
#define CACHEID_VIPT_I_ALIASING		(1 << 4)
#define CACHEID_PIPT			(1 << 5)

extern unsigned int cacheid;

// 2015-08-22
#define cache_is_vivt()			cacheid_is(CACHEID_VIVT/*1*/)
#define cache_is_vipt()			cacheid_is(CACHEID_VIPT)
// 2015-10-03
// 2016-04-16
#define cache_is_vipt_nonaliasing()	cacheid_is(CACHEID_VIPT_NONALIASING/*2*/)
// 2015-08-22
// 현 분석 기준 항상 참
#define cache_is_vipt_aliasing()	cacheid_is(CACHEID_VIPT_ALIASING/*4*/)
// 2015-08-22
// 2015-12-26
#define icache_is_vivt_asid_tagged()	cacheid_is(CACHEID_ASID_TAGGED/*8*/)
#define icache_is_vipt_aliasing()	cacheid_is(CACHEID_VIPT_I_ALIASING)
#define icache_is_pipt()		cacheid_is(CACHEID_PIPT)

/*
 * __LINUX_ARM_ARCH__ is the minimum supported CPU architecture
 * Mask out support which will never be present on newer CPUs.
 * - v6+ is never VIVT
 * - v7+ VIPT never aliases on D-side
 */
#if __LINUX_ARM_ARCH__ >= 7
// 2015-08-22, (2 | 8 | 16 | 32), 1, 3, 4, 5 번째 비트 셋
// 0b111010 == 0x3A
#define __CACHEID_ARCH_MIN	(CACHEID_VIPT_NONALIASING/*2*/ |\
				 CACHEID_ASID_TAGGED/*8*/ |\
				 CACHEID_VIPT_I_ALIASING/*16*/|\
				 CACHEID_PIPT/*32*/)
#elif __LINUX_ARM_ARCH__ >= 6
#define	__CACHEID_ARCH_MIN	(~CACHEID_VIVT)
#else
#define __CACHEID_ARCH_MIN	(~0)
#endif

/*
 * Mask out support which isn't configured
 */
#if defined(CONFIG_CPU_CACHE_VIVT/*N*/) && !defined(CONFIG_CPU_CACHE_VIPT/*Y*/)
#define __CACHEID_ALWAYS	(CACHEID_VIVT)
#define __CACHEID_NEVER		(~CACHEID_VIVT)
#elif !defined(CONFIG_CPU_CACHE_VIVT/*N*/) && defined(CONFIG_CPU_CACHE_VIPT/*Y*/)
// 2015-08-22
#define __CACHEID_ALWAYS	(0)
// 2015-08-22
#define __CACHEID_NEVER		(CACHEID_VIVT/*1*/)
#else
#define __CACHEID_ALWAYS	(0)
#define __CACHEID_NEVER		(0)
#endif

// 2015-08-22
// cacheid_is(CACHEID_VIVT/*0*/), 0리턴
// cacheid_is(CACHEID_VIPT_ALIASING/*4*/, 0리턴)
// cacheid_is(CACHEID_ASID_TAGGED/*8*/), 0리턴
// 2015-10-03
// cacheid_is(CACHEID_VIPT_NONALIASING/*2*/, 0리턴)
// 2016-04-16 
static inline unsigned int __attribute__((pure)) cacheid_is(unsigned int mask)
{
    // cacheid : 0x8444C004
	return (__CACHEID_ALWAYS/*0*/ & mask) |
	       (~__CACHEID_NEVER/*0xFFFF_FFFE*/ & __CACHEID_ARCH_MIN/* 0b111010 */ & mask & cacheid);
}

#endif
