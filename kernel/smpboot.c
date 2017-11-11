/*
 * Common SMP CPU bringup/teardown functions
 */
#include <linux/cpu.h>
#include <linux/err.h>
#include <linux/smp.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/export.h>
#include <linux/percpu.h>
#include <linux/kthread.h>
#include <linux/smpboot.h>

#include "smpboot.h"

#ifdef CONFIG_SMP

#ifdef CONFIG_GENERIC_SMP_IDLE_THREAD	// =y
/*
 * For the hotplug case we keep the task structs around and reuse
 * them.
 */
// 2017-11-04
static DEFINE_PER_CPU(struct task_struct *, idle_threads);

struct task_struct *idle_thread_get(unsigned int cpu)
{
	struct task_struct *tsk = per_cpu(idle_threads, cpu);

	if (!tsk)
		return ERR_PTR(-ENOMEM);
	init_idle(tsk, cpu);
	return tsk;
}

// 2016-07-01
void __init idle_thread_set_boot_cpu(void)
{
	// 현재 cpu에 대한 idle_threads를 current로 설정
	per_cpu(idle_threads, smp_processor_id()) = current;
}

/**
 * idle_init - Initialize the idle thread for a cpu
 * @cpu:	The cpu for which the idle thread should be initialized
 *
 * Creates the thread if it does not exist.
 */
// 2017-11-04
static inline void idle_init(unsigned int cpu)
{
	struct task_struct *tsk = per_cpu(idle_threads, cpu);

	if (!tsk) {
		// 2017-11-04
		tsk = fork_idle(cpu);
		if (IS_ERR(tsk))
			pr_err("SMP: fork_idle() failed for CPU %u\n", cpu);
		else
			per_cpu(idle_threads, cpu) = tsk;
	}
}

/**
 * idle_threads_init - Initialize idle threads for all cpus
 */
// 2017-11-04
void __init idle_threads_init(void)
{
	unsigned int cpu, boot_cpu;

	boot_cpu = smp_processor_id();

	// 모든 cpu에 대해서 idle thread 생성 및 정의 
	for_each_possible_cpu(cpu) {
		if (cpu != boot_cpu)
			idle_init(cpu);
	}
}
#endif

#endif /* #ifdef CONFIG_SMP */

// 2017-11-04
static LIST_HEAD(hotplug_threads);
// 2017-11-04
static DEFINE_MUTEX(smpboot_threads_lock);

// 2017-11-04
struct smpboot_thread_data {
	unsigned int			cpu;
	unsigned int			status;
	struct smp_hotplug_thread	*ht;
};

enum {
	HP_THREAD_NONE = 0,
	HP_THREAD_ACTIVE,
	HP_THREAD_PARKED,
};

/**
 * smpboot_thread_fn - percpu hotplug thread loop function
 * @data:	thread data pointer
 *
 * Checks for thread stop and park conditions. Calls the necessary
 * setup, cleanup, park and unpark functions for the registered
 * thread.
 *
 * Returns 1 when the thread should exit, 0 otherwise.
 */
// 2017-11-11
static int smpboot_thread_fn(void *data)
{
	struct smpboot_thread_data *td = data;
	struct smp_hotplug_thread *ht = td->ht;

	while (1) {
		set_current_state(TASK_INTERRUPTIBLE);
		preempt_disable();
		if (kthread_should_stop()) {
			// 스레드를 종료해야 하는 경우
			set_current_state(TASK_RUNNING);
			preempt_enable();
			if (ht->cleanup)
				ht->cleanup(td->cpu, cpu_online(td->cpu));
			kfree(td);
			return 0;
		}

		if (kthread_should_park()) {
			// 스레드를 일시 정지하는 경우?
			__set_current_state(TASK_RUNNING);
			preempt_enable();
			if (ht->park && td->status == HP_THREAD_ACTIVE) {
				BUG_ON(td->cpu != smp_processor_id());
				ht->park(td->cpu);
				td->status = HP_THREAD_PARKED;
			}
			kthread_parkme();
			/* We might have been woken for stop */
			continue;
		}

		BUG_ON(td->cpu != smp_processor_id());

		/* Check for state change setup */
		switch (td->status) {
		case HP_THREAD_NONE:
			preempt_enable();
			if (ht->setup)
				ht->setup(td->cpu);
			td->status = HP_THREAD_ACTIVE;
			preempt_disable();
			break;
		case HP_THREAD_PARKED:
			preempt_enable();
			if (ht->unpark)
				ht->unpark(td->cpu);
			td->status = HP_THREAD_ACTIVE;
			preempt_disable();
			break;
		}

		if (!ht->thread_should_run(td->cpu)) {
			preempt_enable();
			schedule();
		} else {
			set_current_state(TASK_RUNNING);
			preempt_enable();
			ht->thread_fn(td->cpu);
		}
	}
}

// 2017-11-04
// smpboot와 관련된 스레드 생성 및 create 콜백을 호출한다.
// 만약 기존에 생성된 스레드가 있다면 무시한다.
static int
__smpboot_create_thread(struct smp_hotplug_thread *ht, unsigned int cpu)
{
	struct task_struct *tsk = *per_cpu_ptr(ht->store, cpu);
	struct smpboot_thread_data *td;

	if (tsk)
		return 0;

	td = kzalloc_node(sizeof(*td), GFP_KERNEL, cpu_to_node(cpu));
	if (!td)
		return -ENOMEM;
	td->cpu = cpu;
	td->ht = ht;

	// 2017-11-04
	tsk = kthread_create_on_cpu(smpboot_thread_fn, td, cpu,
				    ht->thread_comm);
	// 2017-11-04, 여기까지
	// 차주는 smpboot_thread_fn부터 
	// 2017-11-11 smpboot_thread_fn 분석 완료
	// 2017-11-11 시작
	if (IS_ERR(tsk)) {
		kfree(td);
		return PTR_ERR(tsk);
	}
	get_task_struct(tsk);
	*per_cpu_ptr(ht->store, cpu) = tsk;
	if (ht->create) {
		/*
		 * Make sure that the task has actually scheduled out
		 * into park position, before calling the create
		 * callback. At least the migration thread callback
		 * requires that the task is off the runqueue.
		 */
		// tsk가 수행되고 있지 않고, 수행가능하지도 않은 경우에만 create콜백 호출 가능
		if (!wait_task_inactive(tsk, TASK_PARKED))
			WARN_ON(1);
		else
			ht->create(cpu);
	}
	return 0;
}

// 2017-11-04
int smpboot_create_threads(unsigned int cpu)
{
	struct smp_hotplug_thread *cur;
	int ret = 0;

	mutex_lock(&smpboot_threads_lock);
	list_for_each_entry(cur, &hotplug_threads, list) {
		// 2017-11-04
		ret = __smpboot_create_thread(cur, cpu);
		if (ret) {
			// ret != 0인 경우, 메모리 부족, 스레드 생성 실패 시 발생한다.
			// 이 경우, 재차 시도할 때도 동일 이슈가 발생할 가능성이 높기 때문에
			// 빠져 나오는 것으로 추측
			break;
		}
	}
	mutex_unlock(&smpboot_threads_lock);
	return ret;
}

// 2017-11-11
static void smpboot_unpark_thread(struct smp_hotplug_thread *ht, unsigned int cpu)
{
	struct task_struct *tsk = *per_cpu_ptr(ht->store, cpu);

	if (ht->pre_unpark)
		ht->pre_unpark(cpu);
	kthread_unpark(tsk);
}

// 2017-11-11
void smpboot_unpark_threads(unsigned int cpu)
{
	struct smp_hotplug_thread *cur;

	mutex_lock(&smpboot_threads_lock);
	list_for_each_entry(cur, &hotplug_threads, list)
		smpboot_unpark_thread(cur, cpu);
	mutex_unlock(&smpboot_threads_lock);
}

static void smpboot_park_thread(struct smp_hotplug_thread *ht, unsigned int cpu)
{
	struct task_struct *tsk = *per_cpu_ptr(ht->store, cpu);

	if (tsk && !ht->selfparking)
		kthread_park(tsk);
}

void smpboot_park_threads(unsigned int cpu)
{
	struct smp_hotplug_thread *cur;

	mutex_lock(&smpboot_threads_lock);
	list_for_each_entry_reverse(cur, &hotplug_threads, list)
		smpboot_park_thread(cur, cpu);
	mutex_unlock(&smpboot_threads_lock);
}

static void smpboot_destroy_threads(struct smp_hotplug_thread *ht)
{
	unsigned int cpu;

	/* We need to destroy also the parked threads of offline cpus */
	for_each_possible_cpu(cpu) {
		struct task_struct *tsk = *per_cpu_ptr(ht->store, cpu);

		if (tsk) {
			kthread_stop(tsk);
			put_task_struct(tsk);
			*per_cpu_ptr(ht->store, cpu) = NULL;
		}
	}
}

/**
 * smpboot_register_percpu_thread - Register a per_cpu thread related to hotplug
 * @plug_thread:	Hotplug thread descriptor
 *
 * Creates and starts the threads on all online cpus.
 */
int smpboot_register_percpu_thread(struct smp_hotplug_thread *plug_thread)
{
	unsigned int cpu;
	int ret = 0;

	mutex_lock(&smpboot_threads_lock);
	for_each_online_cpu(cpu) {
		ret = __smpboot_create_thread(plug_thread, cpu);
		if (ret) {
			smpboot_destroy_threads(plug_thread);
			goto out;
		}
		smpboot_unpark_thread(plug_thread, cpu);
	}
	list_add(&plug_thread->list, &hotplug_threads);
out:
	mutex_unlock(&smpboot_threads_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(smpboot_register_percpu_thread);

/**
 * smpboot_unregister_percpu_thread - Unregister a per_cpu thread related to hotplug
 * @plug_thread:	Hotplug thread descriptor
 *
 * Stops all threads on all possible cpus.
 */
void smpboot_unregister_percpu_thread(struct smp_hotplug_thread *plug_thread)
{
	get_online_cpus();
	mutex_lock(&smpboot_threads_lock);
	list_del(&plug_thread->list);
	smpboot_destroy_threads(plug_thread);
	mutex_unlock(&smpboot_threads_lock);
	put_online_cpus();
}
EXPORT_SYMBOL_GPL(smpboot_unregister_percpu_thread);
