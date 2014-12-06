/*
 * lib/smp_processor_id.c
 *
 * DEBUG_PREEMPT variant of smp_processor_id().
 */
#include <linux/export.h>
#include <linux/kallsyms.h>
#include <linux/sched.h>

// 2014-11-29 시작;
notrace unsigned int debug_smp_processor_id(void)
{
	unsigned long preempt_count = preempt_count();
	//                            current_thread_info()->preempt_count
	
	int this_cpu = raw_smp_processor_id();
	//             current_thread_info()->cpu

	if (likely(preempt_count))
		goto out;

	if (irqs_disabled())
		goto out;

	/*
	 * Kernel threads bound to a single CPU can safely use
	 * smp_processor_id():
	 */
	// if (cpumask_equal(&(tsk)->cpus_allowed, get_cpu_mask(this_cpu)) )
	// 프로세서가 같은지 확인
	if (cpumask_equal(tsk_cpus_allowed(current), cpumask_of(this_cpu)))
		goto out;

	/*
	 * It is valid to assume CPU-locality during early bootup:
	 */
	if (system_state != SYSTEM_RUNNING)
		goto out;

	/*
	 * Avoid recursion:
	 */
	// preempt 증가후 컴파일러 barrier 추가
	preempt_disable_notrace();
	// 2014-11-29; 여기까지

	if (!printk_ratelimit())
		goto out_enable;

	printk(KERN_ERR "BUG: using smp_processor_id() in preemptible [%08x] "
			"code: %s/%d\n",
			preempt_count() - 1, current->comm, current->pid);
	print_symbol("caller is %s\n", (long)__builtin_return_address(0));
	dump_stack();

out_enable:
	preempt_enable_no_resched_notrace();
out:
	return this_cpu;
}

EXPORT_SYMBOL(debug_smp_processor_id);

