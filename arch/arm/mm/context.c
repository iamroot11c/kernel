/*
 *  linux/arch/arm/mm/context.c
 *
 *  Copyright (C) 2002-2003 Deep Blue Solutions Ltd, all rights reserved.
 *  Copyright (C) 2012 ARM Limited
 *
 *  Author: Will Deacon <will.deacon@arm.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/percpu.h>

#include <asm/mmu_context.h>
#include <asm/smp_plat.h>
#include <asm/thread_notify.h>
#include <asm/tlbflush.h>
#include <asm/proc-fns.h>

/*
 * On ARMv6, we have the following structure in the Context ID:
 *
 * 31                         7          0
 * +-------------------------+-----------+
 * |      process ID         |   ASID    |
 * +-------------------------+-----------+
 * |              context ID             |
 * +-------------------------------------+
 *
 * The ASID is used to tag entries in the CPU caches and TLBs.
 * The context ID is used by debuggers and trace logic, and
 * should be unique within all running processes.
 *
 * In big endian operation, the two 32 bit words are swapped if accesed by
 * non 64-bit operations.
 */
// 2016-12-10
#define ASID_FIRST_VERSION	(1ULL << ASID_BITS)
// 2016-12-10
#define NUM_USER_ASIDS		ASID_FIRST_VERSION

static DEFINE_RAW_SPINLOCK(cpu_asid_lock);
// 2016-12-10
static atomic64_t asid_generation = ATOMIC64_INIT(ASID_FIRST_VERSION/*0x400, 256*/);
// 2016-12-10
static DECLARE_BITMAP(asid_map, NUM_USER_ASIDS/*256*/);

// 2016-12-10
static DEFINE_PER_CPU(atomic64_t, active_asids);
// 2016-12-10
static DEFINE_PER_CPU(u64, reserved_asids);
// 2016-12-10
static cpumask_t tlb_flush_pending;

#ifdef CONFIG_ARM_ERRATA_798181
void a15_erratum_get_cpumask(int this_cpu, struct mm_struct *mm,
			     cpumask_t *mask)
{
	int cpu;
	unsigned long flags;
	u64 context_id, asid;

	raw_spin_lock_irqsave(&cpu_asid_lock, flags);
	context_id = mm->context.id.counter;
	for_each_online_cpu(cpu) {
		if (cpu == this_cpu)
			continue;
		/*
		 * We only need to send an IPI if the other CPUs are
		 * running the same ASID as the one being invalidated.
		 */
		asid = per_cpu(active_asids, cpu).counter;
		if (asid == 0)
			asid = per_cpu(reserved_asids, cpu);
		if (context_id == asid)
			cpumask_set_cpu(cpu, mask);
	}
	raw_spin_unlock_irqrestore(&cpu_asid_lock, flags);
}
#endif

#ifdef CONFIG_ARM_LPAE
static void cpu_set_reserved_ttbr0(void)
{
	/*
	 * Set TTBR0 to swapper_pg_dir which contains only global entries. The
	 * ASID is set to 0.
	 */
	cpu_set_ttbr(0, __pa(swapper_pg_dir));
	isb();
}
#else
// 2016-12-10
// TTBR0 = TTBR1;
// isb();
static void cpu_set_reserved_ttbr0(void)
{
	u32 ttb;
	// TTBR stands for Transration Table Base Register
	/* Copy TTBR1 into TTBR0 */
	asm volatile(
	"	mrc	p15, 0, %0, c2, c0, 1		@ read TTBR1\n"
	"	mcr	p15, 0, %0, c2, c0, 0		@ set TTBR0\n"
	: "=r" (ttb));
	isb();
}
#endif

#ifdef CONFIG_PID_IN_CONTEXTIDR
static int contextidr_notifier(struct notifier_block *unused, unsigned long cmd,
			       void *t)
{
	u32 contextidr;
	pid_t pid;
	struct thread_info *thread = t;

	if (cmd != THREAD_NOTIFY_SWITCH)
		return NOTIFY_DONE;

	pid = task_pid_nr(thread->task) << ASID_BITS;
	asm volatile(
	"	mrc	p15, 0, %0, c13, c0, 1\n"
	"	and	%0, %0, %2\n"
	"	orr	%0, %0, %1\n"
	"	mcr	p15, 0, %0, c13, c0, 1\n"
	: "=r" (contextidr), "+r" (pid)
	: "I" (~ASID_MASK));
	isb();

	return NOTIFY_OK;
}

static struct notifier_block contextidr_notifier_block = {
	.notifier_call = contextidr_notifier,
};

static int __init contextidr_notifier_init(void)
{
	return thread_register_notifier(&contextidr_notifier_block);
}
arch_initcall(contextidr_notifier_init);
#endif

// 2016-12-10
// flush_context(cpu);
static void flush_context(unsigned int cpu)
{
	int i;
	u64 asid;

	// bitmap 0으로 clear
	/* Update the list of reserved ASIDs and the ASID bitmap. */
	bitmap_clear(asid_map, 0, NUM_USER_ASIDS);
	// cpu 순회하면서, pure asid값 설정
	// 구한 값을 reserved_asids에 설정
	for_each_possible_cpu(i) {
		if (i == cpu) {
			asid = 0;
		} else {
			asid = atomic64_xchg(&per_cpu(active_asids, i), 0);
			/*
			 * If this CPU has already been through a
			 * rollover, but hasn't run another task in
			 * the meantime, we must preserve its reserved
			 * ASID, as this is the only trace we have of
			 * the process it is still running.
			 */
			if (asid == 0)
				asid = per_cpu(reserved_asids, i);
			// set pure asid
			__set_bit(asid & ~ASID_MASK, asid_map);
		}
		// put reserved_asids per cpu
		per_cpu(reserved_asids, i) = asid;
	}

	/* Queue a TLB invalidate and flush the I-cache if necessary. */
	// tlb_flush_pending은 1로 설정되어 있을 것이다.
	cpumask_setall(&tlb_flush_pending);

	// 우리는 해당 사항 없음
	if (icache_is_vivt_asid_tagged())
		__flush_icache_all();
}

// 2016-12-10
static int is_reserved_asid(u64 asid)
{
	int cpu;
	for_each_possible_cpu(cpu)
		if (per_cpu(reserved_asids, cpu) == asid)
			return 1;
	return 0;
}

// 2016-12-10
// asid = new_context(mm, cpu);
static u64 new_context(struct mm_struct *mm, unsigned int cpu)
{
	u64 asid = atomic64_read(&mm->context.id);
	u64 generation = atomic64_read(&asid_generation);

	if (asid != 0 && is_reserved_asid(asid)) {
		/*
		 * Our current ASID was active during a rollover, we can
		 * continue to use it and this was just a false alarm.
		 */
		asid = generation | (asid & ~ASID_MASK); // generation | (pure asid);
	} else {
		/*
		 * Allocate a free ASID. If we can't find one, take a
		 * note of the currently active ASIDs and mark the TLBs
		 * as requiring flushes. We always count from ASID #1,
		 * as we reserve ASID #0 to switch via TTBR0 and indicate
		 * rollover events.
		 */
		// 첫번째, 0인 비트를 찾음
		asid = find_next_zero_bit(asid_map, NUM_USER_ASIDS, 1);
		if (asid == NUM_USER_ASIDS) {
			// generation 값 변경
			generation = atomic64_add_return(ASID_FIRST_VERSION,
							 &asid_generation);
			// 2016-12-10
			flush_context(cpu);
			asid = find_next_zero_bit(asid_map, NUM_USER_ASIDS, 1);
		}
		
		// 핵심기능
		__set_bit(asid, asid_map);
		asid |= generation;
		cpumask_clear(mm_cpumask(mm));
	}

	return asid;
}

// 2016-12-10
// check_and_switch_context(next, tsk);
void check_and_switch_context(struct mm_struct *mm, struct task_struct *tsk)
{
	unsigned long flags;
	unsigned int cpu = smp_processor_id();
	u64 asid;

	if (unlikely(mm->context.vmalloc_seq != init_mm.context.vmalloc_seq))
		__check_vmalloc_seq(mm);

	/*
	 * Required during context switch to avoid speculative page table
	 * walking with the wrong TTBR.
	 */
	// 2016-12-10
	cpu_set_reserved_ttbr0();

	asid = atomic64_read(&mm->context.id);
	/*
	 * 31                         7          0
	 * +-------------------------+-----------+
	 * |      process ID         |   ASID    |
	 * +-------------------------+-----------+
	 * |              context ID             |
	 * +-------------------------------------+
	 */
	// process ID가 0이 아닌지를 확인 && 일전에 저장된 active_asids가 0이 아니라면,
	// goto switch_mm_fastpath;
	if (!((asid ^ atomic64_read(&asid_generation)) >> ASID_BITS)
	    && atomic64_xchg(&per_cpu(active_asids, cpu), asid))
		goto switch_mm_fastpath;

	// spin lock()
	raw_spin_lock_irqsave(&cpu_asid_lock, flags);
	/* Check that our ASID belongs to the current generation. */
	asid = atomic64_read(&mm->context.id);
	// process ID가 1이 아니라면
	if ((asid ^ atomic64_read(&asid_generation)) >> ASID_BITS) {
		// 2016-12-10
		// 여기서 asid는 몇번째 비트인지와 generation값이 or 된 형태이다.
		asid = new_context(mm, cpu);
		atomic64_set(&mm->context.id, asid);
	}
	// 2016-12-10, 여기까지
	
	// 2016-12-17, 여기부터
	if (cpumask_test_and_clear_cpu(cpu, &tlb_flush_pending)) {
		local_flush_bp_all();
		local_flush_tlb_all();
	}

	atomic64_set(&per_cpu(active_asids, cpu), asid);
	cpumask_set_cpu(cpu, mm_cpumask(mm));
	raw_spin_unlock_irqrestore(&cpu_asid_lock, flags);

switch_mm_fastpath:
	cpu_switch_mm(mm->pgd, mm);
}
