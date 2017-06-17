#ifndef LINUX_PREEMPT_MASK_H
#define LINUX_PREEMPT_MASK_H

#include <linux/preempt.h>
#include <asm/hardirq.h>

/*
 * We put the hardirq and softirq counter into the preemption
 * counter. The bitmask has the following meaning:
 *
 * - bits 0-7 are the preemption count (max preemption depth: 256)
 * - bits 8-15 are the softirq count (max # of softirqs: 256)
 *
 * The hardirq count can in theory reach the same as NR_IRQS.
 * In reality, the number of nested IRQS is limited to the stack
 * size as well. For archs with over 1000 IRQS it is not practical
 * to expect that they will all nest. We give a max of 10 bits for
 * hardirq nesting. An arch may choose to give less than 10 bits.
 * m68k expects it to be 8.
 *
 * - bits 16-25 are the hardirq count (max # of nested hardirqs: 1024)
 * - bit 26 is the NMI_MASK
 * - bit 27 is the PREEMPT_ACTIVE flag
 *
 * PREEMPT_MASK: 0x000000ff
 * SOFTIRQ_MASK: 0x0000ff00
 * HARDIRQ_MASK: 0x03ff0000
 *     NMI_MASK: 0x04000000
 */
#define PREEMPT_BITS	8
#define SOFTIRQ_BITS	8
#define NMI_BITS	1

#define MAX_HARDIRQ_BITS 10

#ifndef HARDIRQ_BITS
# define HARDIRQ_BITS	MAX_HARDIRQ_BITS    // 10
#endif

#if HARDIRQ_BITS > MAX_HARDIRQ_BITS
#error HARDIRQ_BITS too high!
#endif

#define PREEMPT_SHIFT	0
#define SOFTIRQ_SHIFT	(PREEMPT_SHIFT/*0*/ + PREEMPT_BITS/*8*/)    // 8
#define HARDIRQ_SHIFT	(SOFTIRQ_SHIFT/*8*/ + SOFTIRQ_BITS/*8*/)    // 16
#define NMI_SHIFT	(HARDIRQ_SHIFT/*16*/ + HARDIRQ_BITS/*10*/)      // 26

#define __IRQ_MASK(x)	((1UL << (x))-1)

#define PREEMPT_MASK	(__IRQ_MASK(PREEMPT_BITS) << PREEMPT_SHIFT)
#define SOFTIRQ_MASK	(__IRQ_MASK(SOFTIRQ_BITS) << SOFTIRQ_SHIFT)
#define HARDIRQ_MASK	(__IRQ_MASK(HARDIRQ_BITS) << HARDIRQ_SHIFT)
#define NMI_MASK	(__IRQ_MASK(NMI_BITS)     << NMI_SHIFT)

#define PREEMPT_OFFSET	(1UL << PREEMPT_SHIFT)
#define SOFTIRQ_OFFSET	(1UL << SOFTIRQ_SHIFT)
#define HARDIRQ_OFFSET	(1UL << HARDIRQ_SHIFT)
#define NMI_OFFSET	(1UL << NMI_SHIFT)

#define SOFTIRQ_DISABLE_OFFSET	(2 * SOFTIRQ_OFFSET)

// thread_info.h에서 정의된다.
#ifndef PREEMPT_ACTIVE  // 여기 안 들어온다. 이미 다른 곳에 정의
#define PREEMPT_ACTIVE_BITS	1
#define PREEMPT_ACTIVE_SHIFT	(NMI_SHIFT/*26*/ + NMI_BITS/*1*/)       // 27
#define PREEMPT_ACTIVE	(__IRQ_MASK(PREEMPT_ACTIVE_BITS/* 1 */)/* 1 */ << PREEMPT_ACTIVE_SHIFT/*27*/)   // 0x0800_0000
#endif

#if PREEMPT_ACTIVE < (1 << (NMI_SHIFT + NMI_BITS))
#error PREEMPT_ACTIVE is too low!
#endif

// 2015-06-13;
#define hardirq_count()	(preempt_count() & HARDIRQ_MASK)
#define softirq_count()	(preempt_count() & SOFTIRQ_MASK)
// 2015-06-13;
//current_thread_info()->preempt_count & (HARDIRQ_MASK | SOFTIRQ_MASK
// 2016-09-10 
#define irq_count()	(preempt_count() & (HARDIRQ_MASK | SOFTIRQ_MASK \
				 | NMI_MASK))

/*
 * Are we doing bottom half or hardware interrupt processing?
 * Are we in a softirq context? Interrupt context?
 * in_softirq - Are we currently processing softirq or have bh disabled?
 * in_serving_softirq - Are we currently processing softirq?
 */
#define in_irq()		(hardirq_count())
#define in_softirq()		(softirq_count())
// 2016-09-10
// 인터럽트(hw interrupt, sw interrupt, nmi)가 설정이 되었는지 확인
#define in_interrupt()		(irq_count())
// 2015-06-13;
// urrent_thread_info()->preempt_count & HARDIRQ_MASK & SOFTIRQ_OFFSET
#define in_serving_softirq()	(softirq_count() & SOFTIRQ_OFFSET)

/*
 * Are we in NMI context?
 */
#define in_nmi()	(preempt_count() & NMI_MASK)

#if defined(CONFIG_PREEMPT_COUNT)
# define PREEMPT_CHECK_OFFSET 1
#else
# define PREEMPT_CHECK_OFFSET 0
#endif

/*
 * Are we running in atomic context?  WARNING: this macro cannot
 * always detect atomic context; in particular, it cannot know about
 * held spinlocks in non-preemptible kernels.  Thus it should not be
 * used in the general case to determine whether sleeping is possible.
 * Do not use in_atomic() in driver code.
 */
#define in_atomic()	((preempt_count() & ~PREEMPT_ACTIVE) != 0)

/*
 * Check whether we were atomic before we did preempt_disable():
 * (used by the scheduler, *after* releasing the kernel lock)
 */
#define in_atomic_preempt_off() \
		((preempt_count() & ~PREEMPT_ACTIVE) != PREEMPT_CHECK_OFFSET)

#ifdef CONFIG_PREEMPT_COUNT // set
// 아래 애기는, preempt_count()가 0이라면, 현재 자신이 irq라면 true를 리턴
# define preemptible()	(preempt_count() == 0 && !irqs_disabled())
#else
# define preemptible()	0
#endif

#endif /* LINUX_PREEMPT_MASK_H */
