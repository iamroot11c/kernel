/*
 * Generic helpers for smp ipi calls
 *
 * (C) Jens Axboe <jens.axboe@oracle.com> 2008
 */
#include <linux/rcupdate.h>
#include <linux/rculist.h>
#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/percpu.h>
#include <linux/init.h>
#include <linux/gfp.h>
#include <linux/smp.h>
#include <linux/cpu.h>

#include "smpboot.h"

#ifdef CONFIG_USE_GENERIC_SMP_HELPERS	// y
enum {
	CSD_FLAG_LOCK		= 0x01,
};

// 2015-08-29
// 2016-01-23
// 2017-06-17
struct call_function_data {
	struct call_single_data	__percpu *csd;
	cpumask_var_t		cpumask;
	cpumask_var_t		cpumask_ipi;
};

// 2015-08-29
// #define DEFINE_PER_CPU_SHARED_ALIGNED(type, name)           \
//     DEFINE_PER_CPU_SECTION(type, name, PER_CPU_SHARED_ALIGNED_SECTION/*"..shared_aligned"*/) \
//     ____cacheline_aligned_in_smp
// 2016-01-26
static DEFINE_PER_CPU_SHARED_ALIGNED(struct call_function_data, cfd_data);

// 2017-06-17
// 2015-08-29
struct call_single_queue {
	struct list_head	list;
	raw_spinlock_t		lock;
};

// 2015-08-29
// 2016-01-23
// 2017-06-17
// static struct call_single_queue call_single_queue;
static DEFINE_PER_CPU_SHARED_ALIGNED(struct call_single_queue, call_single_queue);

// 2017-06-17
// hotplug_cfd(&hotplug_cfd_notifier, CPU_UP_PREPARE, cpu);

// 동작 : 
// per_cpu call_function_data 타입인 cfd_data[cpu]를 할당
static int
hotplug_cfd(struct notifier_block *nfb, unsigned long action, void *hcpu)
{
	long cpu = (long)hcpu;
	struct call_function_data *cfd = &per_cpu(cfd_data, cpu);

	switch (action) {
	case CPU_UP_PREPARE:
	case CPU_UP_PREPARE_FROZEN:
		// zalloc_cpumask_var_node : 현 분석환경에서는 항상 true
		// cfd->cpumask clear
		if (!zalloc_cpumask_var_node(&cfd->cpumask, GFP_KERNEL,
				cpu_to_node(cpu)))
			return notifier_from_errno(-ENOMEM);
		// cfd->cpumask ipi clear
		if (!zalloc_cpumask_var_node(&cfd->cpumask_ipi, GFP_KERNEL,
				cpu_to_node(cpu))) {
			free_cpumask_var(cfd->cpumask);
			return notifier_from_errno(-ENOMEM);
		}
		// call_single_data percpu 할당. 실패 시 해제
		cfd->csd = alloc_percpu(struct call_single_data);
		if (!cfd->csd) {
			free_cpumask_var(cfd->cpumask_ipi);
			free_cpumask_var(cfd->cpumask);
			return notifier_from_errno(-ENOMEM);
		}
		break;

#ifdef CONFIG_HOTPLUG_CPU // not set
	case CPU_UP_CANCELED:
	case CPU_UP_CANCELED_FROZEN:

	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		free_cpumask_var(cfd->cpumask);
		free_cpumask_var(cfd->cpumask_ipi);
		free_percpu(cfd->csd);
		break;
#endif
	};

	return NOTIFY_OK;
}

// 2017-06-17
static struct notifier_block hotplug_cfd_notifier = {
	.notifier_call		= hotplug_cfd,
};

// 2017-06-17 시작
// 동작 : per_cpu call_single_queue 초기화. 
// hotplug_cfd 콜백 cpu_chain에 등록
void __init call_function_init(void)
{
	void *cpu = (void *)(long)smp_processor_id();
	int i;

	for_each_possible_cpu(i) {
		struct call_single_queue *q = &per_cpu(call_single_queue, i);

		raw_spin_lock_init(&q->lock);
		INIT_LIST_HEAD(&q->list);
	}

	hotplug_cfd(&hotplug_cfd_notifier, CPU_UP_PREPARE, cpu);
	register_cpu_notifier(&hotplug_cfd_notifier);
}

/*
 * csd_lock/csd_unlock used to serialize access to per-cpu csd resources
 *
 * For non-synchronous ipi calls the csd can still be in use by the
 * previous function call. For multi-cpu calls its even more interesting
 * as we'll have to ensure no other cpu is observing our csd.
 */
// 2015-08-29
// 2016-01-23;
// ini 인터럽트 핸들러 handle_IPI 함수에서 
// generic_smp_call_function_single_interrupt() 함수를 호출할 때까지 대기
// 이 함수는 내부에서 csd_unlock() 함수를 호출
static void csd_lock_wait(struct call_single_data *csd)
{
	while (csd->flags & CSD_FLAG_LOCK)
		cpu_relax();
}

// 2015-08-29
// 2016-01-23;
static void csd_lock(struct call_single_data *csd)
{
	csd_lock_wait(csd);
	csd->flags |= CSD_FLAG_LOCK;

	/*
	 * prevent CPU from reordering the above assignment
	 * to ->flags with any subsequent assignments to other
	 * fields of the specified call_single_data structure:
	 */
	smp_mb();
}

static void csd_unlock(struct call_single_data *csd)
{
	WARN_ON(!(csd->flags & CSD_FLAG_LOCK));

	/*
	 * ensure we're all done before releasing data:
	 */
	smp_mb();

	csd->flags &= ~CSD_FLAG_LOCK;
}

/*
 * Insert a previously allocated call_single_data element
 * for execution on the given CPU. data must already have
 * ->func, ->info, and ->flags set.
 */
// 2015-08-29, glance
// 2016-01-23
// generic_exec_single(cpu, csd, 1)
static
void generic_exec_single(int cpu, struct call_single_data *csd, int wait)
{
	struct call_single_queue *dst = &per_cpu(call_single_queue, cpu);
	unsigned long flags;
	int ipi;

	raw_spin_lock_irqsave(&dst->lock, flags);
	ipi = list_empty(&dst->list);
	// dst->list에 enqueue
	list_add_tail(&csd->list, &dst->list);
	raw_spin_unlock_irqrestore(&dst->lock, flags);

	/*
	 * The list addition should be visible before sending the IPI
	 * handler locks the list to pull the entry off it because of
	 * normal cache coherency rules implied by spinlocks.
	 *
	 * If IPIs can go out of order to the cache coherency protocol
	 * in an architecture, sufficient synchronisation should be added
	 * to arch code to make it appear to obey cache coherency WRT
	 * locking and barrier primitives. Generic code isn't really
	 * equipped to do the right thing...
	 */
	if (ipi)
		arch_send_call_function_single_ipi(cpu);

	if (wait)
		csd_lock_wait(csd);
}

/*
 * Invoked by arch to handle an IPI for call function single. Must be
 * called from the arch with interrupts disabled.
 */
// 2016-01-23
void generic_smp_call_function_single_interrupt(void)
{
	struct call_single_queue *q = &__get_cpu_var(call_single_queue);
	LIST_HEAD(list);

	/*
	 * Shouldn't receive this interrupt on a cpu that is not yet online.
	 */
	WARN_ON_ONCE(!cpu_online(smp_processor_id()));

	raw_spin_lock(&q->lock);
	list_replace_init(&q->list, &list);
	raw_spin_unlock(&q->lock);

	while (!list_empty(&list)) {
		struct call_single_data *csd;

		csd = list_entry(list.next, struct call_single_data, list);
		list_del(&csd->list);

		csd->func(csd->info);

		csd_unlock(csd);
	}
}

static DEFINE_PER_CPU_SHARED_ALIGNED(struct call_single_data, csd_data);

/*
 * smp_call_function_single - Run a function on a specific CPU
 * @func: The function to run. This must be fast and non-blocking.
 * @info: An arbitrary pointer to pass to the function.
 * @wait: If true, wait until function has completed on other CPUs.
 *
 * Returns 0 on success, else a negative status code.
 */
// 2015-08,29, glance
// 2016-01-23
// smp_call_function_single(cpu, drain_local_pages, null, 1);
int smp_call_function_single(int cpu, smp_call_func_t func, void *info,
			     int wait)
{
	struct call_single_data d = {
		.flags = 0,
	};
	unsigned long flags;
	int this_cpu;
	int err = 0;

	/*
	 * prevent preemption and reschedule on another processor,
	 * as well as CPU removal
	 */
	this_cpu = get_cpu();

	/*
	 * Can deadlock when called with interrupts disabled.
	 * We allow cpu's that are not yet online though, as no one else can
	 * send smp call function interrupt to this cpu and as such deadlocks
	 * can't happen.
	 */
	WARN_ON_ONCE(cpu_online(this_cpu) && irqs_disabled()
		     && !oops_in_progress);

	if (cpu == this_cpu) {
		local_irq_save(flags);
		// 2016-01-23; smp_call_function_single() 함수에서
		// drain_local_pages() 함수 호출하는 것을 제외한 분석 완료 
		// 2016-01-30; drain_local_pages() 함수 분석 예정
		func(info);
		local_irq_restore(flags);
	} else {
		if ((unsigned)cpu < nr_cpu_ids && cpu_online(cpu)) {
			struct call_single_data *csd = &d;

			if (!wait)
				csd = &__get_cpu_var(csd_data);

			csd_lock(csd);

			csd->func = func;
			csd->info = info;
			generic_exec_single(cpu, csd, wait);
		} else {
			err = -ENXIO;	/* CPU not online */
		}
	}

	// enable preempt
	put_cpu();

	return err;
}
EXPORT_SYMBOL(smp_call_function_single);

/*
 * smp_call_function_any - Run a function on any of the given cpus
 * @mask: The mask of cpus it can run on.
 * @func: The function to run. This must be fast and non-blocking.
 * @info: An arbitrary pointer to pass to the function.
 * @wait: If true, wait until function has completed.
 *
 * Returns 0 on success, else a negative status code (if no cpus were online).
 *
 * Selection preference:
 *	1) current cpu if in @mask
 *	2) any cpu of current node if in @mask
 *	3) any other online cpu in @mask
 */
int smp_call_function_any(const struct cpumask *mask,
			  smp_call_func_t func, void *info, int wait)
{
	unsigned int cpu;
	const struct cpumask *nodemask;
	int ret;

	/* Try for same CPU (cheapest) */
	cpu = get_cpu();
	if (cpumask_test_cpu(cpu, mask))
		goto call;

	/* Try for same node. */
	nodemask = cpumask_of_node(cpu_to_node(cpu));
	for (cpu = cpumask_first_and(nodemask, mask); cpu < nr_cpu_ids;
	     cpu = cpumask_next_and(cpu, nodemask, mask)) {
		if (cpu_online(cpu))
			goto call;
	}

	/* Any online will do: smp_call_function_single handles nr_cpu_ids. */
	cpu = cpumask_any_and(mask, cpu_online_mask);
call:
	ret = smp_call_function_single(cpu, func, info, wait);
	put_cpu();
	return ret;
}
EXPORT_SYMBOL_GPL(smp_call_function_any);

/**
 * __smp_call_function_single(): Run a function on a specific CPU
 * @cpu: The CPU to run on.
 * @data: Pre-allocated and setup data structure
 * @wait: If true, wait until function has completed on specified CPU.
 *
 * Like smp_call_function_single(), but allow caller to pass in a
 * pre-allocated data structure. Useful for embedding @data inside
 * other structures, for instance.
 */
void __smp_call_function_single(int cpu, struct call_single_data *csd,
				int wait)
{
	unsigned int this_cpu;
	unsigned long flags;

	this_cpu = get_cpu();
	/*
	 * Can deadlock when called with interrupts disabled.
	 * We allow cpu's that are not yet online though, as no one else can
	 * send smp call function interrupt to this cpu and as such deadlocks
	 * can't happen.
	 */
	WARN_ON_ONCE(cpu_online(smp_processor_id()) && wait && irqs_disabled()
		     && !oops_in_progress);

	if (cpu == this_cpu) {
		local_irq_save(flags);
		csd->func(csd->info);
		local_irq_restore(flags);
	} else {
		csd_lock(csd);
		generic_exec_single(cpu, csd, wait);
	}
	put_cpu();
}

/**
 * smp_call_function_many(): Run a function on a set of other CPUs.
 * @mask: The set of cpus to run on (only runs on online subset).
 * @func: The function to run. This must be fast and non-blocking.
 * @info: An arbitrary pointer to pass to the function.
 * @wait: If true, wait (atomically) until function has completed
 *        on other CPUs.
 *
 * If @wait is true, then returns once @func has returned.
 *
 * You must not call this function with disabled interrupts or from a
 * hardware interrupt handler or from a bottom half handler. Preemption
 * must be disabled when calling this function.
 */
// 2015-08-29
// smp_call_function_many(&mask, ipi_flush_tlb_a15_erratum, NULL, 1);
// 2016-01-23; drain_local_pages() 함수 제외한 분석 완료
// smp_call_function_many(mask, drain_local_pages, NULL, 1);
void smp_call_function_many(const struct cpumask *mask,
			    smp_call_func_t func, void *info, bool wait)
{
	struct call_function_data *cfd;
	int cpu, next_cpu, this_cpu = smp_processor_id();

	/*
	 * Can deadlock when called with interrupts disabled.
	 * We allow cpu's that are not yet online though, as no one else can
	 * send smp call function interrupt to this cpu and as such deadlocks
	 * can't happen.
	 */
	// early_boot_irqs_disabled은 start_kernel 시작 시 true, 거의 끝날 때즘 false로 세팅
	WARN_ON_ONCE(cpu_online(this_cpu) && irqs_disabled()
		     && !oops_in_progress && !early_boot_irqs_disabled);

	/* Try to fastpath.  So, what's a CPU they want? Ignoring this one. */
	cpu = cpumask_first_and(mask, cpu_online_mask);
	if (cpu == this_cpu)
		cpu = cpumask_next_and(cpu, mask, cpu_online_mask);

	/* No online cpus?  We're done. */
	if (cpu >= nr_cpu_ids)
		return;

	/* Do we have another CPU which isn't us? */
	next_cpu = cpumask_next_and(cpu, mask, cpu_online_mask);
	if (next_cpu == this_cpu)
		next_cpu = cpumask_next_and(next_cpu, mask, cpu_online_mask);

	/* Fastpath: do that cpu by itself. */
	if (next_cpu >= nr_cpu_ids) {
		// 2015-08-29, 나중에
		// 2016-01-23 시작; drain_local_pages() 함수 제외한 분석 완료 
		smp_call_function_single(cpu, func, info, wait);
		return;
	}

	// 식사전
	cfd = &__get_cpu_var(cfd_data);

	cpumask_and(cfd->cpumask, mask, cpu_online_mask);
	cpumask_clear_cpu(this_cpu, cfd->cpumask);

	/* Some callers race with other cpus changing the passed mask */
	// cfd->cpumask 비트열에 셋된 비트가 없으면 오류로 간주하고 리턴
	if (unlikely(!cpumask_weight(cfd->cpumask)))
		return;

	/*
	 * After we put an entry into the list, cfd->cpumask may be cleared
	 * again when another CPU sends another IPI for a SMP function call, so
	 * cfd->cpumask will be zero.
	 */
	// 2015-08-29
	// cfd->cpumask 를 cfd->cpumask_ipi에 복사
	// NOTE: cfd->cpumask에서 this_cpu 번째 비트가 클리어 된 상태 
	cpumask_copy(cfd->cpumask_ipi, cfd->cpumask);

	// #define for_each_cpu(cpu, mask)             \
	//      for ((cpu) = -1;                \
	//          (cpu) = cpumask_next((cpu), (mask)),    \
	//          (cpu) < nr_cpu_ids;
	// 
	// 각 cpu 마다 동일한 call_single_data를 할당
	for_each_cpu(cpu, cfd->cpumask) {
		struct call_single_data *csd = per_cpu_ptr(cfd->csd, cpu);
		struct call_single_queue *dst =
					&per_cpu(call_single_queue, cpu);
		unsigned long flags;

		// 2016-01-23; 단위 csd 설정
		csd_lock(csd);
		csd->func = func;
		csd->info = info;

		raw_spin_lock_irqsave(&dst->lock, flags);
		// 2016-01-23; call_single_queue 리스트에 enqueue
		list_add_tail(&csd->list, &dst->list);
		raw_spin_unlock_irqrestore(&dst->lock, flags);
	}

	/* Send a message to all CPUs in the map */
	// 2015-08-29
	// 좀 이상하다. 우리 arch에서는 설정된 적이 없는데 함수포인터 형태로 호출되고 있다.
	arch_send_call_function_ipi_mask(cfd->cpumask_ipi);

	if (wait) {
		for_each_cpu(cpu, cfd->cpumask) {
			struct call_single_data *csd;

			csd = per_cpu_ptr(cfd->csd, cpu);
			csd_lock_wait(csd);
		}
	}
}
EXPORT_SYMBOL(smp_call_function_many);

/**
 * smp_call_function(): Run a function on all other CPUs.
 * @func: The function to run. This must be fast and non-blocking.
 * @info: An arbitrary pointer to pass to the function.
 * @wait: If true, wait (atomically) until function has completed
 *        on other CPUs.
 *
 * Returns 0.
 *
 * If @wait is true, then returns once @func has returned; otherwise
 * it returns just before the target cpu calls @func.
 *
 * You must not call this function with disabled interrupts or from a
 * hardware interrupt handler or from a bottom half handler.
 */
int smp_call_function(smp_call_func_t func, void *info, int wait)
{
	preempt_disable();
	smp_call_function_many(cpu_online_mask, func, info, wait);
	preempt_enable();

	return 0;
}
EXPORT_SYMBOL(smp_call_function);
#endif /* USE_GENERIC_SMP_HELPERS */

/* Setup configured maximum number of CPUs to activate */
// 2017-11-04
unsigned int setup_max_cpus = NR_CPUS;
EXPORT_SYMBOL(setup_max_cpus);


/*
 * Setup routine for controlling SMP activation
 *
 * Command-line option of "nosmp" or "maxcpus=0" will disable SMP
 * activation entirely (the MPS table probe still happens, though).
 *
 * Command-line option of "maxcpus=<NUM>", where <NUM> is an integer
 * greater than 0, limits the maximum number of CPUs activated in
 * SMP mode to <NUM>.
 */

void __weak arch_disable_smp_support(void) { }

static int __init nosmp(char *str)
{
	setup_max_cpus = 0;
	arch_disable_smp_support();

	return 0;
}

early_param("nosmp", nosmp);

/* this is hard limit */
static int __init nrcpus(char *str)
{
	int nr_cpus;

	get_option(&str, &nr_cpus);
	if (nr_cpus > 0 && nr_cpus < nr_cpu_ids)
		nr_cpu_ids = nr_cpus;

	return 0;
}

early_param("nr_cpus", nrcpus);

static int __init maxcpus(char *str)
{
	get_option(&str, &setup_max_cpus);
	if (setup_max_cpus == 0)
		arch_disable_smp_support();

	return 0;
}

early_param("maxcpus", maxcpus);

/* Setup number of possible processor ids */
// 2015-08-29
int nr_cpu_ids __read_mostly = NR_CPUS/*2*/;
//__read_mostly	: 이 데이터는 자주 수정되지 않으며, 대부분 읽기 연산만 이루어진다라는 것을 
//컴파일러에게 알려줌 	20140712
EXPORT_SYMBOL(nr_cpu_ids);

// 2015-03-07, 여기까지
// 2015-03-14, find_last_bit() 함수 분석 완료
/* An arch may set nr_cpu_ids earlier if needed, so this would be redundant */
void __init setup_nr_cpu_ids(void)
{
	// 검색을 실패하면 NR_CPUS 개수가 리턴됨
	nr_cpu_ids = find_last_bit(cpumask_bits(cpu_possible_mask),NR_CPUS) + 1;
}

/* Called by boot processor to activate the rest. */
// 2017-11-04
void __init smp_init(void)
{
	unsigned int cpu;

	// 2017-11-04, start
	idle_threads_init();
	// 2017-11-04, end


	/* FIXME: This should be done in userspace --RR */
	for_each_present_cpu(cpu) {
		if (num_online_cpus() >= setup_max_cpus)
			break;
		if (!cpu_online(cpu))
			// 2017-11-04
			cpu_up(cpu);
	}

	/* Any cleanup work */
	printk(KERN_INFO "Brought up %ld CPUs\n", (long)num_online_cpus());
	smp_cpus_done(setup_max_cpus);
}

/*
 * Call a function on all processors.  May be used during early boot while
 * early_boot_irqs_disabled is set.  Use local_irq_save/restore() instead
 * of local_irq_disable/enable().
 */
int on_each_cpu(void (*func) (void *info), void *info, int wait)
{
	unsigned long flags;
	int ret = 0;

	preempt_disable();
	ret = smp_call_function(func, info, wait);
	local_irq_save(flags);
	func(info);
	local_irq_restore(flags);
	preempt_enable();
	return ret;
}
EXPORT_SYMBOL(on_each_cpu);

/**
 * on_each_cpu_mask(): Run a function on processors specified by
 * cpumask, which may include the local processor.
 * @mask: The set of cpus to run on (only runs on online subset).
 * @func: The function to run. This must be fast and non-blocking.
 * @info: An arbitrary pointer to pass to the function.
 * @wait: If true, wait (atomically) until function has completed
 *        on other CPUs.
 *
 * If @wait is true, then returns once @func has returned.
 *
 * You must not call this function with disabled interrupts or from a
 * hardware interrupt handler or from a bottom half handler.  The
 * exception is that it may be used during early boot while
 * early_boot_irqs_disabled is set.
 */
// 2016-01-23
// on_each_cpu_mask(&cpus_with_pcps, drain_local_pages, NULL, 1);
void on_each_cpu_mask(const struct cpumask *mask, smp_call_func_t func,
			void *info, bool wait)
{
	// {preempt_disable(); smp_processor_id();}
	// 먼저 선점 불능하게 하고, cpu에 프로세서 ID를 저장
	int cpu = get_cpu();

	// 2016-01-23 시작;
	smp_call_function_many(mask, func, info, wait);

	// 2016-01-30, drain_local_pages() 분석 시작 
	if (cpumask_test_cpu(cpu, mask)) {
		unsigned long flags;
		local_irq_save(flags);
		func(info);
		local_irq_restore(flags);
	}
	put_cpu();
}
EXPORT_SYMBOL(on_each_cpu_mask);

/*
 * on_each_cpu_cond(): Call a function on each processor for which
 * the supplied function cond_func returns true, optionally waiting
 * for all the required CPUs to finish. This may include the local
 * processor.
 * @cond_func:	A callback function that is passed a cpu id and
 *		the the info parameter. The function is called
 *		with preemption disabled. The function should
 *		return a blooean value indicating whether to IPI
 *		the specified CPU.
 * @func:	The function to run on all applicable CPUs.
 *		This must be fast and non-blocking.
 * @info:	An arbitrary pointer to pass to both functions.
 * @wait:	If true, wait (atomically) until function has
 *		completed on other CPUs.
 * @gfp_flags:	GFP flags to use when allocating the cpumask
 *		used internally by the function.
 *
 * The function might sleep if the GFP flags indicates a non
 * atomic allocation is allowed.
 *
 * Preemption is disabled to protect against CPUs going offline but not online.
 * CPUs going online during the call will not be seen or sent an IPI.
 *
 * You must not call this function with disabled interrupts or
 * from a hardware interrupt handler or from a bottom half handler.
 */
void on_each_cpu_cond(bool (*cond_func)(int cpu, void *info),
			smp_call_func_t func, void *info, bool wait,
			gfp_t gfp_flags)
{
	cpumask_var_t cpus;
	int cpu, ret;

	might_sleep_if(gfp_flags & __GFP_WAIT);

	if (likely(zalloc_cpumask_var(&cpus, (gfp_flags|__GFP_NOWARN)))) {
		preempt_disable();
		for_each_online_cpu(cpu)
			if (cond_func(cpu, info))
				cpumask_set_cpu(cpu, cpus);
		on_each_cpu_mask(cpus, func, info, wait);
		preempt_enable();
		free_cpumask_var(cpus);
	} else {
		/*
		 * No free cpumask, bother. No matter, we'll
		 * just have to IPI them one by one.
		 */
		preempt_disable();
		for_each_online_cpu(cpu)
			if (cond_func(cpu, info)) {
				ret = smp_call_function_single(cpu, func,
								info, wait);
				WARN_ON_ONCE(!ret);
			}
		preempt_enable();
	}
}
EXPORT_SYMBOL(on_each_cpu_cond);

static void do_nothing(void *unused)
{
}

/**
 * kick_all_cpus_sync - Force all cpus out of idle
 *
 * Used to synchronize the update of pm_idle function pointer. It's
 * called after the pointer is updated and returns after the dummy
 * callback function has been executed on all cpus. The execution of
 * the function can only happen on the remote cpus after they have
 * left the idle function which had been called via pm_idle function
 * pointer. So it's guaranteed that nothing uses the previous pointer
 * anymore.
 */
void kick_all_cpus_sync(void)
{
	/* Make sure the change is visible before we kick the cpus */
	smp_mb();
	smp_call_function(do_nothing, NULL, 1);
}
EXPORT_SYMBOL_GPL(kick_all_cpus_sync);
