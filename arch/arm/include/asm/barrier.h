#ifndef __ASM_BARRIER_H
#define __ASM_BARRIER_H

#ifndef __ASSEMBLY__
#include <asm/outercache.h>

#define nop() __asm__ __volatile__("mov\tr0,r0\t@ nop\n\t");

#if __LINUX_ARM_ARCH__ >= 7 ||		\
	(__LINUX_ARM_ARCH__ == 6 && defined(CONFIG_CPU_32v6K))
#define sev()	__asm__ __volatile__ ("sev" : : : "memory")
// 2015-11-07
// irq, fiq, 이벤트 레지스터 세팅 등의 이벤트가 일어날 때까지 대기
// http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0204ik/Cjafcggi.html
#define wfe()	__asm__ __volatile__ ("wfe" : : : "memory")
#define wfi()	__asm__ __volatile__ ("wfi" : : : "memory")
#endif

// 2015-01-31
#if __LINUX_ARM_ARCH__ >= 7
// isb, 명령어 동기화, 프로세스의 파이프라인을 비운다.
#define isb(option) __asm__ __volatile__ ("isb " #option : : : "memory")
// dsb, 이 명령이 수행되기전에, 명령어의 수행이 모두 완료 된다.
#define dsb(option) __asm__ __volatile__ ("dsb " #option : : : "memory")
// dmb, Data Access에 대한 모든 명령어가 끝나고 나서, dmb 명령 이후, Data Access가
// 발생하는 것을 보장한다.
// 2016-04-16
// dmb(ishst)
#define dmb(option) __asm__ __volatile__ ("dmb " #option : : : "memory")
#elif defined(CONFIG_CPU_XSC3) || __LINUX_ARM_ARCH__ == 6
#define isb(x) __asm__ __volatile__ ("mcr p15, 0, %0, c7, c5, 4" \
				    : : "r" (0) : "memory")
#define dsb(x) __asm__ __volatile__ ("mcr p15, 0, %0, c7, c10, 4" \
				    : : "r" (0) : "memory")
#define dmb(x) __asm__ __volatile__ ("mcr p15, 0, %0, c7, c10, 5" \
				    : : "r" (0) : "memory")
#elif defined(CONFIG_CPU_FA526)
#define isb(x) __asm__ __volatile__ ("mcr p15, 0, %0, c7, c5, 4" \
				    : : "r" (0) : "memory")
#define dsb(x) __asm__ __volatile__ ("mcr p15, 0, %0, c7, c10, 4" \
				    : : "r" (0) : "memory")
#define dmb(x) __asm__ __volatile__ ("" : : : "memory")
#else
#define isb(x) __asm__ __volatile__ ("" : : : "memory")
#define dsb(x) __asm__ __volatile__ ("mcr p15, 0, %0, c7, c10, 4" \
				    : : "r" (0) : "memory")
#define dmb(x) __asm__ __volatile__ ("" : : : "memory")
#endif

#ifdef CONFIG_ARCH_HAS_BARRIERS
#include <mach/barriers.h>
#elif defined(CONFIG_ARM_DMA_MEM_BUFFERABLE) || defined(CONFIG_SMP)
#define mb()		do { dsb(); outer_sync(); } while (0)
#define rmb()		dsb()
#define wmb()		do { dsb(st); outer_sync(); } while (0)
#else
#define mb()		barrier()
#define rmb()		barrier()
#define wmb()		barrier()
#endif

#ifndef CONFIG_SMP // CONFIG_SMP defined
#define smp_mb()	barrier()
#define smp_rmb()	barrier()
#define smp_wmb()	barrier()
#else
// 2015-09-05;
#define smp_mb()	dmb(ish)
// 2015-04-18;
#define smp_rmb()	smp_mb()
// 2016-04-16;
#define smp_wmb()	dmb(ishst)
#endif

#define read_barrier_depends()		do { } while(0)
#define smp_read_barrier_depends()	do { } while(0)

// 2015-06-20
#define set_mb(var, value)	do { var = value; smp_mb(); } while (0)

#endif /* !__ASSEMBLY__ */
#endif /* __ASM_BARRIER_H */
