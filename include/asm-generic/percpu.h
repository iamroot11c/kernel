#ifndef _ASM_GENERIC_PERCPU_H_
#define _ASM_GENERIC_PERCPU_H_

#include <linux/compiler.h>
#include <linux/threads.h>
#include <linux/percpu-defs.h>

#ifdef CONFIG_SMP // defind

/*
 * per_cpu_offset() is the offset that has to be added to a
 * percpu variable to get to the instance for a certain processor.
 *
 * Most arches use the __per_cpu_offset array for those offsets but
 * some arches have their own ways of determining the offset (x86_64, s390).
 */
#ifndef __per_cpu_offset
extern unsigned long __per_cpu_offset[NR_CPUS]; // NR_CPUS = 2;

// 2015-12-12
#define per_cpu_offset(x) (__per_cpu_offset[x])
#endif

/*
 * Determine the offset for the currently active processor.
 * An arch may define __my_cpu_offset to provide a more effective
 * means of obtaining the offset to the per cpu variables of the
 * current processor.
 */
// 2015-12-12
#ifndef __my_cpu_offset
#define __my_cpu_offset per_cpu_offset(raw_smp_processor_id())
#endif
#ifdef CONFIG_DEBUG_PREEMPT
#define my_cpu_offset per_cpu_offset(smp_processor_id())
#else
#define my_cpu_offset __my_cpu_offset
#endif

/*
 * Add a offset to a pointer but keep the pointer as is.
 *
 * Only S390 provides its own means of moving the pointer.
 */
#ifndef SHIFT_PERCPU_PTR
/* Weird cast keeps both GCC and sparse happy. */
// 참고: http://egloos.zum.com/studyfoss/v/5375570
#define SHIFT_PERCPU_PTR(__p, __offset)	({				\
	__verify_pcpu_ptr((__p));					\
	RELOC_HIDE((typeof(*(__p)) __kernel __force *)(__p), (__offset)); \
})
#endif

/*
 * A percpu variable may point to a discarded regions. The following are
 * established ways to produce a usable pointer from the percpu variable
 * offset.
 */
// 2015-03-28
// 2015-08-15
#define per_cpu(var, cpu) \
	(*SHIFT_PERCPU_PTR(&(var), per_cpu_offset(cpu)))

#ifndef __this_cpu_ptr
// 2015-12-12
#define __this_cpu_ptr(ptr) SHIFT_PERCPU_PTR(ptr, __my_cpu_offset)
#endif
#ifdef CONFIG_DEBUG_PREEMPT
#define this_cpu_ptr(ptr) SHIFT_PERCPU_PTR(ptr, my_cpu_offset)
#else
// 2015-12-12
#define this_cpu_ptr(ptr) __this_cpu_ptr(ptr)
#endif

// 2015-07-11
// 2015-08-29
// 2015-10-24
// 2015-12-17
// __get_cpu_var(lru_rotate_pvecs)
#define __get_cpu_var(var) (*this_cpu_ptr(&(var)))
// 2015-06-20
#define __raw_get_cpu_var(var) (*__this_cpu_ptr(&(var)))

#ifdef CONFIG_HAVE_SETUP_PER_CPU_AREA
extern void setup_per_cpu_areas(void);
#endif

#else /* ! SMP */

#define VERIFY_PERCPU_PTR(__p) ({			\
	__verify_pcpu_ptr((__p));			\
	(typeof(*(__p)) __kernel __force *)(__p);	\
})

#define per_cpu(var, cpu)	(*((void)(cpu), VERIFY_PERCPU_PTR(&(var))))
#define __get_cpu_var(var)	(*VERIFY_PERCPU_PTR(&(var)))
#define __raw_get_cpu_var(var)	(*VERIFY_PERCPU_PTR(&(var)))
#define this_cpu_ptr(ptr)	per_cpu_ptr(ptr, 0)
#define __this_cpu_ptr(ptr)	this_cpu_ptr(ptr)

#endif	/* SMP */

#ifndef PER_CPU_BASE_SECTION
#ifdef CONFIG_SMP
#define PER_CPU_BASE_SECTION ".data..percpu"
#else
#define PER_CPU_BASE_SECTION ".data"
#endif
#endif

#ifdef CONFIG_SMP

#ifdef MODULE
#define PER_CPU_SHARED_ALIGNED_SECTION ""
#define PER_CPU_ALIGNED_SECTION ""
#else
// 2015-08-29, compile 통해 확인
#define PER_CPU_SHARED_ALIGNED_SECTION "..shared_aligned"
#define PER_CPU_ALIGNED_SECTION "..shared_aligned"
#endif
#define PER_CPU_FIRST_SECTION "..first"

#else

#define PER_CPU_SHARED_ALIGNED_SECTION ""
#define PER_CPU_ALIGNED_SECTION "..shared_aligned"
#define PER_CPU_FIRST_SECTION ""

#endif

#ifndef PER_CPU_ATTRIBUTES
#define PER_CPU_ATTRIBUTES
#endif

#ifndef PER_CPU_DEF_ATTRIBUTES
#define PER_CPU_DEF_ATTRIBUTES
#endif

#endif /* _ASM_GENERIC_PERCPU_H_ */
