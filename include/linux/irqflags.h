/*
 * include/linux/irqflags.h
 *
 * IRQ flags tracing: follow the state of the hardirq and softirq flags and
 * provide callbacks for transitions between ON and OFF states.
 *
 * This file gets included from lowlevel asm headers too, to provide
 * wrapped versions of the local_irq_*() APIs, based on the
 * raw_local_irq_*() macros from the lowlevel headers.
 */
#ifndef _LINUX_TRACE_IRQFLAGS_H
#define _LINUX_TRACE_IRQFLAGS_H

#include <linux/typecheck.h>
#include <asm/irqflags.h>

#ifdef CONFIG_TRACE_IRQFLAGS    // not set
  extern void trace_softirqs_on(unsigned long ip);
  extern void trace_softirqs_off(unsigned long ip);
  extern void trace_hardirqs_on(void);
  extern void trace_hardirqs_off(void);
# define trace_hardirq_context(p)	((p)->hardirq_context)
# define trace_softirq_context(p)	((p)->softirq_context)
# define trace_hardirqs_enabled(p)	((p)->hardirqs_enabled)
# define trace_softirqs_enabled(p)	((p)->softirqs_enabled)
# define trace_hardirq_enter()	do { current->hardirq_context++; } while (0)
# define trace_hardirq_exit()	do { current->hardirq_context--; } while (0)
# define lockdep_softirq_enter()	do { current->softirq_context++; } while (0)
# define lockdep_softirq_exit()	do { current->softirq_context--; } while (0)
# define INIT_TRACE_IRQFLAGS	.softirqs_enabled = 1,
#else
# define trace_hardirqs_on()		do { } while (0)
# define trace_hardirqs_off()		do { } while (0)
# define trace_softirqs_on(ip)		do { } while (0)
# define trace_softirqs_off(ip)		do { } while (0)
# define trace_hardirq_context(p)	0
# define trace_softirq_context(p)	0
# define trace_hardirqs_enabled(p)	0
# define trace_softirqs_enabled(p)	0
# define trace_hardirq_enter()		do { } while (0)
# define trace_hardirq_exit()		do { } while (0)
# define lockdep_softirq_enter()	do { } while (0)
# define lockdep_softirq_exit()		do { } while (0)
# define INIT_TRACE_IRQFLAGS
#endif

#if defined(CONFIG_IRQSOFF_TRACER) || \
	defined(CONFIG_PREEMPT_TRACER)
 extern void stop_critical_timings(void);
 extern void start_critical_timings(void);
#else
# define stop_critical_timings() do { } while (0)
# define start_critical_timings() do { } while (0)
#endif

/*
 * Wrap the arch provided IRQ routines to provide appropriate checks.
 */
 // 2015-11-07
#define raw_local_irq_disable()		arch_local_irq_disable()
#define raw_local_irq_enable()		arch_local_irq_enable()
// 2016-03-26
#define raw_local_irq_save(flags)			\
	do {						\
		typecheck(unsigned long, flags);	\
		flags = arch_local_irq_save();		\
	} while (0)
#define raw_local_irq_restore(flags)			\
	do {						\
		typecheck(unsigned long, flags);	\
		arch_local_irq_restore(flags);		\
	} while (0)

// 2016-07-09
// CPSR값을 읽어어서 flag 저장함
#define raw_local_save_flags(flags)			\
	do {						\
		typecheck(unsigned long, flags);	\
		flags = arch_local_save_flags();	\
	} while (0)
// 2016-07-09
#define raw_irqs_disabled_flags(flags)			\
	({						\
		typecheck(unsigned long, flags);	\
		arch_irqs_disabled_flags(flags);	\
	})
#define raw_irqs_disabled()		(arch_irqs_disabled())
#define raw_safe_halt()			arch_safe_halt()

/*
 * The local_irq_*() APIs are equal to the raw_local_irq*()
 * if !TRACE_IRQFLAGS.
 */
#ifdef CONFIG_TRACE_IRQFLAGS_SUPPORT    // set
#define local_irq_enable() \
	do { trace_hardirqs_on(); raw_local_irq_enable(); } while (0)
 // 2015-11-07
#define local_irq_disable() \
	do { raw_local_irq_disable(); trace_hardirqs_off(); } while (0)
#define local_irq_save(flags)				\
	do {						\
		raw_local_irq_save(flags);		\
		trace_hardirqs_off();			\
	} while (0)

// 2015-11-07
// 이 함수에서 flags에 해당되는 값은 cpsr 등의 상태 값일 것이다.
#define local_irq_restore(flags)			\
	do {						\
        /*IRQ I Mask bit가 flag에 세팅된 경우*/\
		if (raw_irqs_disabled_flags(flags)) {	\
            /*cpsr_c에 flags값을 저장*/\
			raw_local_irq_restore(flags);	\
			trace_hardirqs_off();		\
		} else {				\
			trace_hardirqs_on();		\
            /*cpsr_c에 flags값을 저장*/\
			raw_local_irq_restore(flags);	\
		}					\
	} while (0)
#define local_save_flags(flags)				\
	do {						\
		raw_local_save_flags(flags);		\
	} while (0)

#define irqs_disabled_flags(flags)			\
	({						\
		raw_irqs_disabled_flags(flags);		\
	})

// _flags = current of cpsr;
// 2015-08-29
// 2016-07-09
// CPSR을 읽어 와서 IRQ의 disable인지 확인하는 기능
#define irqs_disabled()					\
	({						\
		unsigned long _flags;			\
		raw_local_save_flags(_flags);		\
		raw_irqs_disabled_flags(_flags);	\
	})

#define safe_halt()				\
	do {					\
		trace_hardirqs_on();		\
		raw_safe_halt();		\
	} while (0)


#else /* !CONFIG_TRACE_IRQFLAGS_SUPPORT */
// CONFIG_TRACE_IRQFLAGS 비 활성화
#define local_irq_enable()	do { raw_local_irq_enable(); } while (0)
#define local_irq_disable()	do { raw_local_irq_disable(); } while (0)
#define local_irq_save(flags)					\
	do {							\
		raw_local_irq_save(flags);			\
	} while (0)
#define local_irq_restore(flags) do { raw_local_irq_restore(flags); } while (0)
#define local_save_flags(flags)	do { raw_local_save_flags(flags); } while (0)
#define irqs_disabled()		(raw_irqs_disabled())
#define irqs_disabled_flags(flags) (raw_irqs_disabled_flags(flags))
#define safe_halt()		do { raw_safe_halt(); } while (0)

#endif /* CONFIG_TRACE_IRQFLAGS_SUPPORT */

#endif
