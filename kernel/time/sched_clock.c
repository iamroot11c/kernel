/*
 * sched_clock.c: support for extending counters to full 64-bit ns counter
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/clocksource.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <linux/syscore_ops.h>
#include <linux/timer.h>
#include <linux/sched_clock.h>

// 2016-08-13
struct clock_data {
	u64 epoch_ns;
	u32 epoch_cyc;
	u32 epoch_cyc_copy;
	unsigned long rate;
	u32 mult;
	u32 shift;
	bool suspended;
};

static void sched_clock_poll(unsigned long wrap_ticks);
// 2016-08-13
// #define DEFINE_TIMER(_name, _function, _expires, _data)     \
//      struct timer_list _name =               \
//          TIMER_INITIALIZER(_function, _expires, _data)
//
// #define __TIMER_INITIALIZER(_function, _expires, _data, _flags) { \
//          .entry = { .prev = TIMER_ENTRY_STATIC },    \
//          .function = (_function),            \
//          .expires = (_expires),              \
//          .data = (_data),                \
//          .base = (void *)((unsigned long)&boot_tvec_bases + (_flags)), \
//          .slack = -1,                    \
//          __TIMER_LOCKDEP_MAP_INITIALIZER(        \
//              __FILE__ ":" __stringify(__LINE__)) \
//      }
//  
// #define TIMER_INITIALIZER(_function, _expires, _data)       \
//     __TIMER_INITIALIZER((_function), (_expires), (_data), 0)

// struct timer_list sched_clock_timer = { \
//          .entry = { .prev = TIMER_ENTRY_STATIC },    \
//          .function = (sched_clock_poll),            \
//          .expires = (0),              \
//          .data = (0),                \
//          .base = (void *)((unsigned long)&boot_tvec_bases + (0)), \
//          .slack = -1,                    \
//      }

static DEFINE_TIMER(sched_clock_timer, sched_clock_poll, 0, 0);
static int irqtime = -1;

core_param(irqtime, irqtime, int, 0400);

// 2016-08-13
// 2016-08-20
// setup_sched_clock(jiffy_sched_clock_read, 32, HZ);
// .rate = HZ/*100*/
static struct clock_data cd = {
	.mult	= NSEC_PER_SEC / HZ,
};

// 2016-08-13
static u32 __read_mostly sched_clock_mask = 0xffffffff;

// 2016-08-13
// 2016-08-20
static u32 notrace jiffy_sched_clock_read(void)
{
	return (u32)(jiffies - INITIAL_JIFFIES/*(-300*HZ))*/);
}

// 2016-08-13
// setup_sched_clock(jiffy_sched_clock_read, 32, HZ);
static u32 __read_mostly (*read_sched_clock)(void) = jiffy_sched_clock_read;

// 2016-08-13
// cyc_to_ns((1ULL << bits) - 1, cd.mult, cd.shift);
// cyc_to_ns(1ULL, cd.mult, cd.shift);
static inline u64 notrace cyc_to_ns(u64 cyc, u32 mult, u32 shift)
{
	return (cyc * mult) >> shift;
}

static unsigned long long notrace sched_clock_32(void)
{
	u64 epoch_ns;
	u32 epoch_cyc;
	u32 cyc;

	if (cd.suspended)
		return cd.epoch_ns;

	/*
	 * Load the epoch_cyc and epoch_ns atomically.  We do this by
	 * ensuring that we always write epoch_cyc, epoch_ns and
	 * epoch_cyc_copy in strict order, and read them in strict order.
	 * If epoch_cyc and epoch_cyc_copy are not equal, then we're in
	 * the middle of an update, and we should repeat the load.
	 */
	do {
		epoch_cyc = cd.epoch_cyc;
		smp_rmb();
		epoch_ns = cd.epoch_ns;
		smp_rmb();
	} while (epoch_cyc != cd.epoch_cyc_copy);

	cyc = read_sched_clock();
	cyc = (cyc - epoch_cyc) & sched_clock_mask;
	return epoch_ns + cyc_to_ns(cyc, cd.mult, cd.shift);
}

/*
 * Atomically update the sched_clock epoch.
 */
// 2016-08-13
// 2016-08-20
// 1) 현재 cycle, epoch ns 시간을 얻음
// 2) clock data값을 설정 
static void notrace update_sched_clock(void)
{
	unsigned long flags;
	u32 cyc;
	u64 ns;

	// 현재 jiffies값을 초기값 보정을 하여 얻어옴
	cyc = read_sched_clock();
	// cyc -> 현재 부팅 기준 epoch 시간(유닉스 기준 시간) ns로 변환
	ns = cd.epoch_ns +
		cyc_to_ns((cyc - cd.epoch_cyc) & sched_clock_mask,
			  cd.mult, cd.shift);
	/*
	 * Write epoch_cyc and epoch_ns in a way that the update is
	 * detectable in cyc_to_fixed_sched_clock().
	 */
	raw_local_irq_save(flags);
	cd.epoch_cyc_copy = cyc;
	smp_wmb();
	cd.epoch_ns = ns;
	smp_wmb();
	cd.epoch_cyc = cyc;
	raw_local_irq_restore(flags);
}

// 2016-08-13
// 2016-08-20
// sched_clock_poll(sched_clock_timer.data)
static void sched_clock_poll(unsigned long wrap_ticks)
{
	mod_timer(&sched_clock_timer, round_jiffies(jiffies + wrap_ticks));
	update_sched_clock();
}

// 2016-08-13
// setup_sched_clock(jiffy_sched_clock_read, 32, HZ);
void __init setup_sched_clock(u32 (*read)(void), int bits, unsigned long rate)
{
	unsigned long r, w;
	u64 res, wrap;
	char r_unit;

	if (cd.rate > rate)
		return;

	BUG_ON(bits > 32);
	WARN_ON(!irqs_disabled());
	read_sched_clock = read;
	sched_clock_mask = (1ULL << bits) - 1;
	cd.rate = rate;

	/* calculate the mult/shift to convert counter ticks to ns. */
	clocks_calc_mult_shift(&cd.mult, &cd.shift, rate, NSEC_PER_SEC, 0);

	r = rate;
	if (r >= 4000000) {
		r /= 1000000;
		r_unit = 'M';
	} else if (r >= 1000) {
		r /= 1000;
		r_unit = 'k';
	} else
		r_unit = ' ';

	/* calculate how many ns until we wrap */
	wrap = cyc_to_ns((1ULL << bits) - 1, cd.mult, cd.shift);
	do_div(wrap, NSEC_PER_MSEC);
	w = wrap;

	/* calculate the ns resolution of this counter */
	res = cyc_to_ns(1ULL, cd.mult, cd.shift);
	pr_info("sched_clock: %u bits at %lu%cHz, resolution %lluns, wraps every %lums\n",
		bits, r, r_unit, res, w);

	/*
	 * Start the timer to keep sched_clock() properly updated and
	 * sets the initial epoch.
	 */
	// w * 9 / 10
	sched_clock_timer.data = msecs_to_jiffies(w - (w / 10));
	// sched_clock epoch
	update_sched_clock();

	/*
	 * Ensure that sched_clock() starts off at 0ns
	 */
	cd.epoch_ns = 0;

	/* Enable IRQ time accounting if we have a fast enough sched_clock */
	if (irqtime > 0 || (irqtime == -1 && rate >= 1000000))
		enable_sched_clock_irqtime();

	pr_debug("Registered %pF as sched_clock source\n", read);
}

unsigned long long __read_mostly (*sched_clock_func)(void) = sched_clock_32;

unsigned long long notrace sched_clock(void)
{
	return sched_clock_func();
}

// 2016-08-13
void __init sched_clock_postinit(void)
{
	/*
	 * If no sched_clock function has been provided at that point,
	 * make it the final one one.
	 */
	if (read_sched_clock == jiffy_sched_clock_read)
		setup_sched_clock(jiffy_sched_clock_read, 32, HZ);	// 2016-08-13

	// 2016-08-13, 여기까지
	// 2016-08-20, 시작
	sched_clock_poll(sched_clock_timer.data);
	// 2016-08-20, 완료
}

static int sched_clock_suspend(void)
{
	sched_clock_poll(sched_clock_timer.data);
	cd.suspended = true;
	return 0;
}

static void sched_clock_resume(void)
{
	cd.epoch_cyc = read_sched_clock();
	cd.epoch_cyc_copy = cd.epoch_cyc;
	cd.suspended = false;
}

static struct syscore_ops sched_clock_ops = {
	.suspend = sched_clock_suspend,
	.resume = sched_clock_resume,
};

static int __init sched_clock_syscore_init(void)
{
	register_syscore_ops(&sched_clock_ops);
	return 0;
}
device_initcall(sched_clock_syscore_init);
