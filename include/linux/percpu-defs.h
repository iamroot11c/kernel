#ifndef _LINUX_PERCPU_DEFS_H
#define _LINUX_PERCPU_DEFS_H

/*
 * Base implementations of per-CPU variable declarations and definitions, where
 * the section in which the variable is to be placed is provided by the
 * 'sec' argument.  This may be used to affect the parameters governing the
 * variable's storage.
 *
 * NOTE!  The sections for the DECLARE and for the DEFINE must match, lest
 * linkage errors occur due the compiler generating the wrong code to access
 * that section.
 */
// 2016-01-23
#define __PCPU_ATTRS(sec)						\
	__percpu __attribute__((section(PER_CPU_BASE_SECTION/*.data..percpu*/ sec)))	\
	PER_CPU_ATTRIBUTES

#define __PCPU_DUMMY_ATTRS						\
	__attribute__((section(".discard"), unused))

/*
 * Macro which verifies @ptr is a percpu pointer without evaluating
 * @ptr.  This is to be used in percpu accessors to verify that the
 * input parameter is a percpu pointer.
 *
 * + 0 is required in order to convert the pointer type from a
 * potential array type to a pointer to a single item of the array.
 */
#define __verify_pcpu_ptr(ptr)	do {					\
	const void __percpu *__vpp_verify = (typeof((ptr) + 0))NULL;	\
	(void)__vpp_verify;						\
} while (0)

/*
 * s390 and alpha modules require percpu variables to be defined as
 * weak to force the compiler to generate GOT based external
 * references for them.  This is necessary because percpu sections
 * will be located outside of the usually addressable area.
 *
 * This definition puts the following two extra restrictions when
 * defining percpu variables.
 *
 * 1. The symbol must be globally unique, even the static ones.
 * 2. Static percpu variables cannot be defined inside a function.
 *
 * Archs which need weak percpu definitions should define
 * ARCH_NEEDS_WEAK_PER_CPU in asm/percpu.h when necessary.
 *
 * To ensure that the generic code observes the above two
 * restrictions, if CONFIG_DEBUG_FORCE_WEAK_PER_CPU is set weak
 * definition is used for all cases.
 */
// ARCH_NEEDS_WEAK_PER_CPU and CONFIG_DEBUG_FORCE_WEAK_PER_CPU not define
#if defined(ARCH_NEEDS_WEAK_PER_CPU) || defined(CONFIG_DEBUG_FORCE_WEAK_PER_CPU)
/*
 * __pcpu_scope_* dummy variable is used to enforce scope.  It
 * receives the static modifier when it's used in front of
 * DEFINE_PER_CPU() and will trigger build failure if
 * DECLARE_PER_CPU() is used for the same variable.
 *
 * __pcpu_unique_* dummy variable is used to enforce symbol uniqueness
 * such that hidden weak symbol collision, which will cause unrelated
 * variables to share the same address, can be detected during build.
 */
// 2015-06-20
// extern 선언을 위한 용도로 사용
// 그렇기 때문에, DEFINE_PER_CPU_SECTION()이 먼저 선언된 것을 가정하고
// 사용할 것이다.
#define DECLARE_PER_CPU_SECTION(type, name, sec)			\
	extern __PCPU_DUMMY_ATTRS char __pcpu_scope_##name;		\
	extern __PCPU_ATTRS(sec) __typeof__(type) name

// 2015-06-20
// 2015-08-20
#define DEFINE_PER_CPU_SECTION(type, name, sec)				\
	__PCPU_DUMMY_ATTRS char __pcpu_scope_##name;			\
	extern __PCPU_DUMMY_ATTRS char __pcpu_unique_##name;		\
	__PCPU_DUMMY_ATTRS char __pcpu_unique_##name;			\
	__PCPU_ATTRS(sec) PER_CPU_DEF_ATTRIBUTES __weak			\
	__typeof__(type) name
#else
/*
 * Normal declaration and definition macros.
 */
// ARCH_NEEDS_WEAK_PER_CPU와 
//  CONFIG_DEBUG_FORCE_WEAK_PER_CPU 미 정의
#define DECLARE_PER_CPU_SECTION(type, name, sec)			\
	extern __PCPU_ATTRS(sec) __typeof__(type) name

// 2015-03-28; 디버그가 활성화 되어있으나, 간단히 보기위해 여기에 주석을 첨가
// DEFINE_PER_CPU_SECTION(struct call_function_data, cfd_data, "..shared_aligned")
// 2016-01-23
// __percpu __attribute__((section(".data..percpu""..shared_aligned"))) PER_CPU_ATTRIBUTES
// __typeof__(struct call_function_data) cfd_data
// https://gcc.gnu.org/onlinedocs/gcc/Alternate-Keywords.html#Alternate-Keywords
// typeof는 gcc에서만 지원되며, 다른 컴파일러는 표준 함수 이름인 __typeof__를 사용해야됨
#define DEFINE_PER_CPU_SECTION(type, name, sec)				\
	__PCPU_ATTRS(sec) PER_CPU_DEF_ATTRIBUTES			\
	__typeof__(type) name
#endif

/*
 * Variant on the per-CPU variable declaration/definition theme used for
 * ordinary per-CPU variables.
 */
// 2015-06-20
// 2016-09-24
#define DECLARE_PER_CPU(type, name)					\
	DECLARE_PER_CPU_SECTION(type, name, "")

// DEFINE_PER_CPU(int, __kmap_atomic_idx); 
// 2016-08-13
// DEFINE_PER_CPU(struct list_head [NR_SOFTIRQS], softirq_work_list);
// 2017-09-23
// DEFINE_PER_CPU(struct tick_sched, tick_cpu_sched);
#define DEFINE_PER_CPU(type, name)					\
	DEFINE_PER_CPU_SECTION(type, name, "")

/*
 * Declaration/definition used for per-CPU variables that must come first in
 * the set of variables.
 */
#define DECLARE_PER_CPU_FIRST(type, name)				\
	DECLARE_PER_CPU_SECTION(type, name, PER_CPU_FIRST_SECTION)

#define DEFINE_PER_CPU_FIRST(type, name)				\
	DEFINE_PER_CPU_SECTION(type, name, PER_CPU_FIRST_SECTION)

/*
 * Declaration/definition used for per-CPU variables that must be cacheline
 * aligned under SMP conditions so that, whilst a particular instance of the
 * data corresponds to a particular CPU, inefficiencies due to direct access by
 * other CPUs are reduced by preventing the data from unnecessarily spanning
 * cachelines.
 *
 * An example of this would be statistical data, where each CPU's set of data
 * is updated by that CPU alone, but the data from across all CPUs is collated
 * by a CPU processing a read from a proc file.
 */
#define DECLARE_PER_CPU_SHARED_ALIGNED(type, name)			\
	DECLARE_PER_CPU_SECTION(type, name, PER_CPU_SHARED_ALIGNED_SECTION) \
	____cacheline_aligned_in_smp

// 2015-06-20
// 2015-08-29
// static DEFINE_PER_CPU_SHARED_ALIGNED(struct call_function_data, cfd_data)
// simple: static struct call_function_data cfd_data;
// 이 변수는 ".data..percpu..shared_aligned" 영역에 저장됨
#define DEFINE_PER_CPU_SHARED_ALIGNED(type, name)			\
	DEFINE_PER_CPU_SECTION(type, name, PER_CPU_SHARED_ALIGNED_SECTION/*"..shared_aligned"*/) \
	____cacheline_aligned_in_smp

#define DECLARE_PER_CPU_ALIGNED(type, name)				\
	DECLARE_PER_CPU_SECTION(type, name, PER_CPU_ALIGNED_SECTION)	\
	____cacheline_aligned

#define DEFINE_PER_CPU_ALIGNED(type, name)				\
	DEFINE_PER_CPU_SECTION(type, name, PER_CPU_ALIGNED_SECTION)	\
	____cacheline_aligned

/*
 * Declaration/definition used for per-CPU variables that must be page aligned.
 */
#define DECLARE_PER_CPU_PAGE_ALIGNED(type, name)			\
	DECLARE_PER_CPU_SECTION(type, name, "..page_aligned")		\
	__aligned(PAGE_SIZE)

#define DEFINE_PER_CPU_PAGE_ALIGNED(type, name)				\
	DEFINE_PER_CPU_SECTION(type, name, "..page_aligned")		\
	__aligned(PAGE_SIZE)

/*
 * Declaration/definition used for per-CPU variables that must be read mostly.
 */
#define DECLARE_PER_CPU_READ_MOSTLY(type, name)			\
	DECLARE_PER_CPU_SECTION(type, name, "..readmostly")

#define DEFINE_PER_CPU_READ_MOSTLY(type, name)				\
	DEFINE_PER_CPU_SECTION(type, name, "..readmostly")

/*
 * Intermodule exports for per-CPU variables.  sparse forgets about
 * address space across EXPORT_SYMBOL(), change EXPORT_SYMBOL() to
 * noop if __CHECKER__.
 */
#ifndef __CHECKER__
#define EXPORT_PER_CPU_SYMBOL(var) EXPORT_SYMBOL(var)
#define EXPORT_PER_CPU_SYMBOL_GPL(var) EXPORT_SYMBOL_GPL(var)
#else
#define EXPORT_PER_CPU_SYMBOL(var)
#define EXPORT_PER_CPU_SYMBOL_GPL(var)
#endif

#endif /* _LINUX_PERCPU_DEFS_H */
