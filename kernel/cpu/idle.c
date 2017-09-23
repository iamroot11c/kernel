/*
 * Generic entry point for the idle threads
 */
#include <linux/sched.h>
#include <linux/cpu.h>
#include <linux/tick.h>
#include <linux/mm.h>
#include <linux/stackprotector.h>

#include <asm/tlb.h>

#include <trace/events/power.h>

// 2017-09-23
static int __read_mostly cpu_idle_force_poll;

void cpu_idle_poll_ctrl(bool enable)
{
	if (enable) {
		cpu_idle_force_poll++;
	} else {
		cpu_idle_force_poll--;
		WARN_ON_ONCE(cpu_idle_force_poll < 0);
	}
}

#ifdef CONFIG_GENERIC_IDLE_POLL_SETUP
static int __init cpu_idle_poll_setup(char *__unused)
{
	cpu_idle_force_poll = 1;
	return 1;
}
__setup("nohlt", cpu_idle_poll_setup);

static int __init cpu_idle_nopoll_setup(char *__unused)
{
	cpu_idle_force_poll = 0;
	return 1;
}
__setup("hlt", cpu_idle_nopoll_setup);
#endif

// 2017-09-23
static inline int cpu_idle_poll(void)
{
	rcu_idle_enter();
	trace_cpu_idle_rcuidle(0, smp_processor_id());
	local_irq_enable();
	// pollling 여기서 발생
	while (!tif_need_resched())
		cpu_relax();
	trace_cpu_idle_rcuidle(PWR_EVENT_EXIT, smp_processor_id());
	rcu_idle_exit();
	return 1;
}

/* Weak implementations for optional arch specific functions */
// 2017-09-23
void __weak arch_cpu_idle_prepare(void) { }
void __weak arch_cpu_idle_enter(void) { }
void __weak arch_cpu_idle_exit(void) { }
// 2017-09-23
void __weak arch_cpu_idle_dead(void) { }
void __weak arch_cpu_idle(void)
{
	cpu_idle_force_poll = 1;
	local_irq_enable();
}

/*
 * Generic idle loop implementation
 */
// 2017-09-23
static void cpu_idle_loop(void)
{
	while (1) {
		tick_nohz_idle_enter();

		// 2017-09-23
		while (!need_resched()) {
			check_pgt_cache();	// NOP
			rmb();

			if (cpu_is_offline(smp_processor_id()))
				arch_cpu_idle_dead();

			local_irq_disable();
			arch_cpu_idle_enter();

			/*
			 * In poll mode we reenable interrupts and spin.
			 *
			 * Also if we detected in the wakeup from idle
			 * path that the tick broadcast device expired
			 * for us, we don't want to go deep idle as we
			 * know that the IPI is going to arrive right
			 * away
			 */
			if (cpu_idle_force_poll || tick_check_broadcast_expired()) {
				// 대기
				cpu_idle_poll();
			} else {
				// 대부분 아래 if문 실행될 것임
				if (!current_clr_polling_and_test()) {
					stop_critical_timings();	// NOP
					rcu_idle_enter();
					arch_cpu_idle();		// 여기서 대기
					WARN_ON_ONCE(irqs_disabled());
					rcu_idle_exit();
					start_critical_timings();	// NOP
				} else {
					local_irq_enable();
				}
				__current_set_polling();	// NOP
			}
			arch_cpu_idle_exit();	// NOP
		}

		// 2017-09-23, exit시 동작 먼저 봄
		tick_nohz_idle_exit();
		schedule_preempt_disabled();
	}
}

// 2017-09-23
void cpu_startup_entry(enum cpuhp_state state)
{
	/*
	 * This #ifdef needs to die, but it's too late in the cycle to
	 * make this generic (arm and sh have never invoked the canary
	 * init for the non boot cpus!). Will be fixed in 3.11
	 */
#ifdef CONFIG_X86	// =n
	/*
	 * If we're the non-boot CPU, nothing set the stack canary up
	 * for us. The boot CPU already has it initialized but no harm
	 * in doing it again. This is a good place for updating it, as
	 * we wont ever return from this function (so the invalid
	 * canaries already on the stack wont ever trigger).
	 */
	boot_init_stack_canary();
#endif
	__current_set_polling();
	arch_cpu_idle_prepare();	// fiq enable
	// infinite loop, using cpu_relax() or wfi()
	cpu_idle_loop();
}
