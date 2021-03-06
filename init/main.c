/*
 *  linux/init/main.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  GK 2/5/95  -  Changed to support mounting root fs via NFS
 *  Added initrd & change_root: Werner Almesberger & Hans Lermen, Feb '96
 *  Moan early if gcc is old, avoiding bogus kernels - Paul Gortmaker, May '96
 *  Simplified starting of init:  Michael A. Griffith <grif@acm.org> 
 */

#define DEBUG		/* Enable initcall_debug */

#include <linux/types.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/stackprotector.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/initrd.h>
#include <linux/bootmem.h>
#include <linux/acpi.h>
#include <linux/tty.h>
#include <linux/percpu.h>
#include <linux/kmod.h>
#include <linux/vmalloc.h>
#include <linux/kernel_stat.h>
#include <linux/start_kernel.h>
#include <linux/security.h>
#include <linux/smp.h>
#include <linux/profile.h>
#include <linux/rcupdate.h>
#include <linux/moduleparam.h>
#include <linux/kallsyms.h>
#include <linux/writeback.h>
#include <linux/cpu.h>
#include <linux/cpuset.h>
#include <linux/cgroup.h>
#include <linux/efi.h>
#include <linux/tick.h>
#include <linux/interrupt.h>
#include <linux/taskstats_kern.h>
#include <linux/delayacct.h>
#include <linux/unistd.h>
#include <linux/rmap.h>
#include <linux/mempolicy.h>
#include <linux/key.h>
#include <linux/buffer_head.h>
#include <linux/page_cgroup.h>
#include <linux/debug_locks.h>
#include <linux/debugobjects.h>
#include <linux/lockdep.h>
#include <linux/kmemleak.h>
#include <linux/pid_namespace.h>
#include <linux/device.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/idr.h>
#include <linux/kgdb.h>
#include <linux/ftrace.h>
#include <linux/async.h>
#include <linux/kmemcheck.h>
#include <linux/sfi.h>
#include <linux/shmem_fs.h>
#include <linux/slab.h>
#include <linux/perf_event.h>
#include <linux/file.h>
#include <linux/ptrace.h>
#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/sched_clock.h>
#include <linux/context_tracking.h>
#include <linux/random.h>

#include <asm/io.h>
#include <asm/bugs.h>
#include <asm/setup.h>
#include <asm/sections.h>
#include <asm/cacheflush.h>

#ifdef CONFIG_X86_LOCAL_APIC
#include <asm/smp.h>
#endif

static int kernel_init(void *);

extern void init_IRQ(void);
extern void fork_init(unsigned long);
extern void mca_init(void);
extern void sbus_init(void);
extern void radix_tree_init(void);
#ifndef CONFIG_DEBUG_RODATA // n
// 2018-03-10
static inline void mark_rodata_ro(void) { }
#endif

#ifdef CONFIG_TC
extern void tc_init(void);
#endif

/*
 * Debug helper: via this flag we know that we are in 'early bootup code'
 * where only the boot processor is running with IRQ disabled.  This means
 * two things - IRQ must not be enabled before the flag is cleared and some
 * operations which are not allowed with IRQ disabled are allowed while the
 * flag is set.
 */
// 2015-08-29
bool early_boot_irqs_disabled __read_mostly;

// 2016-07-23
enum system_states system_state __read_mostly;
EXPORT_SYMBOL(system_state);

/*
 * Boot command-line arguments
 */
// 2015-05-02,  CONFIG_INIT_ENV_ARG_LIMIT = 32
#define MAX_INIT_ARGS CONFIG_INIT_ENV_ARG_LIMIT
#define MAX_INIT_ENVS CONFIG_INIT_ENV_ARG_LIMIT

extern void time_init(void);
/* Default late time init is NULL. archs can override this later. */
void (*__initdata late_time_init)(void);
extern void softirq_init(void);

/* Untouched command line saved by arch-specific code. */
char __initdata boot_command_line[COMMAND_LINE_SIZE];
/* Untouched saved command line (eg. for /proc) */
// 2018-03-03
char *saved_command_line;
/* Command line for parameter parsing */
// 2018-03-03
static char *static_command_line;

static char *execute_command;
static char *ramdisk_execute_command;

/*
 * If set, this is an indication to the drivers that reset the underlying
 * device before going ahead with the initialization otherwise driver might
 * rely on the BIOS and skip the reset operation.
 *
 * This is useful if kernel is booting in an unreliable environment.
 * For ex. kdump situaiton where previous kernel has crashed, BIOS has been
 * skipped and devices will be in unknown state.
 */
unsigned int reset_devices;
EXPORT_SYMBOL(reset_devices);

static int __init set_reset_devices(char *str)
{
	reset_devices = 1;
	return 1;
}

__setup("reset_devices", set_reset_devices);

static const char * argv_init[MAX_INIT_ARGS+2] = { "init", NULL, };
const char * envp_init[MAX_INIT_ENVS+2] = { "HOME=/", "TERM=linux", NULL, };
static const char *panic_later, *panic_param;

extern const struct obs_kernel_param __setup_start[], __setup_end[];

// 2015-05-02
static int __init obsolete_checksetup(char *line)
{
	const struct obs_kernel_param *p;
	int had_early_param = 0;

	p = __setup_start;
	do {
		int n = strlen(p->str);
		if (parameqn(line, p->str, n)) {
			if (p->early) {
				/* Already done in parse_early_param?
				 * (Needs exact match on param part).
				 * Keep iterating, as we can have early
				 * params and __setups of same names 8( */
				if (line[n] == '\0' || line[n] == '=')
					had_early_param = 1;
			} else if (!p->setup_func) {
				pr_warn("Parameter %s is obsolete, ignored\n",
					p->str);
				return 1;
			} else if (p->setup_func(line + n))
				return 1;
		}
		p++;
	} while (p < __setup_end);

	return had_early_param;
}

/*
 * This should be approx 2 Bo*oMips to start (note initial shift), and will
 * still work even if initially too large, it will just take slightly longer
 */
// 2017-06-17
// 2017-11-04
unsigned long loops_per_jiffy = (1<<12);

EXPORT_SYMBOL(loops_per_jiffy);

static int __init debug_kernel(char *str)
{
	console_loglevel = 10;
	return 0;
}

static int __init quiet_kernel(char *str)
{
	console_loglevel = 4;
	return 0;
}

early_param("debug", debug_kernel);
early_param("quiet", quiet_kernel);

static int __init loglevel(char *str)
{
	int newlevel;

	/*
	 * Only update loglevel value when a correct setting was passed,
	 * to prevent blind crashes (when loglevel being set to 0) that
	 * are quite hard to debug
	 */
	if (get_option(&str, &newlevel)) {
		console_loglevel = newlevel;
		return 0;
	}

	return -EINVAL;
}

early_param("loglevel", loglevel);

/* Change NUL term back to "=", to make "param" the whole string. */
// 2015-05-02
static int __init repair_env_string(char *param, char *val, const char *unused)
{
	if (val) {
		/* param=val or param="val"? */
		if (val == param+strlen(param)+1)
			val[-1] = '=';
		else if (val == param+strlen(param)+2) {
			val[-2] = '=';
			memmove(val-1, val, strlen(val)+1);
			val--;
		} else
			BUG();
	}
	return 0;
}

/*
 * Unknown boot options get handed to init, unless they look like
 * unused parameters (modprobe will find them in /proc/cmdline).
 */
// 2015-05-02
static int __init unknown_bootoption(char *param, char *val, const char *unused)
{
	repair_env_string(param, val, unused);

	/* Handle obsolete-style parameters */
	if (obsolete_checksetup(param))
		return 0;

	/* Unused module parameter. */
	if (strchr(param, '.') && (!val || strchr(param, '.') < val))
		return 0;

	if (panic_later)
		return 0;

	if (val) {
		/* Environment option */
		unsigned int i;
		//  envp_init[i] NULL인지 아닌지가 탈출조건이다.
		for (i = 0; envp_init[i]; i++) {
			if (i == MAX_INIT_ENVS/*32*/) {
				panic_later = "Too many boot env vars at `%s'";
				panic_param = param;
			}
			if (!strncmp(param, envp_init[i], val - param))
				break;
		}
		envp_init[i] = param;
	} else {
		/* Command line option */
		unsigned int i;
		for (i = 0; argv_init[i]; i++) {
			if (i == MAX_INIT_ARGS) {
				panic_later = "Too many boot init vars at `%s'";
				panic_param = param;
			}
		}
		argv_init[i] = param;
	}
	return 0;
}

static int __init init_setup(char *str)
{
	unsigned int i;

	execute_command = str;
	/*
	 * In case LILO is going to boot us with default command line,
	 * it prepends "auto" before the whole cmdline which makes
	 * the shell think it should execute a script with such name.
	 * So we ignore all arguments entered _before_ init=... [MJ]
	 */
	for (i = 1; i < MAX_INIT_ARGS; i++)
		argv_init[i] = NULL;
	return 1;
}
__setup("init=", init_setup);

static int __init rdinit_setup(char *str)
{
	unsigned int i;

	ramdisk_execute_command = str;
	/* See "auto" comment in init_setup */
	for (i = 1; i < MAX_INIT_ARGS; i++)
		argv_init[i] = NULL;
	return 1;
}
__setup("rdinit=", rdinit_setup);

#ifndef CONFIG_SMP
static const unsigned int setup_max_cpus = NR_CPUS;
#ifdef CONFIG_X86_LOCAL_APIC
static void __init smp_init(void)
{
	APIC_init_uniprocessor();
}
#else
#define smp_init()	do { } while (0)
#endif

static inline void setup_nr_cpu_ids(void) { }
static inline void smp_prepare_cpus(unsigned int maxcpus) { }
#endif

/*
 * We need to store the untouched command line for future reference.
 * We also need to store the touched command line since the parameter
 * parsing is performed in place, and we should allow a component to
 * store reference of name/value for future reference.
 */
// 2015-03-07
// 전역 변수에 복사하는 하는 기능을 함.
// command_line, boot_command_line에 대해서
static void __init setup_command_line(char *command_line)
{
	saved_command_line = alloc_bootmem(strlen (boot_command_line)+1);
	static_command_line = alloc_bootmem(strlen (command_line)+1);
	strcpy (saved_command_line, boot_command_line);
	strcpy (static_command_line, command_line);
}

/*
 * We need to finalize in a non-__init function or else race conditions
 * between the root thread and the init thread may cause start_kernel to
 * be reaped by free_initmem before the root thread has proceeded to
 * cpu_idle.
 *
 * gcc-3.4 accidentally inlines this function, so use noinline.
 */

// 2017-09-23
// #define DECLARE_COMPLETION(work) \
//	struct completion work = COMPLETION_INITIALIZER(work)
static __initdata DECLARE_COMPLETION(kthreadd_done);

// 2017-07-22
static noinline void __init_refok rest_init(void)
{
	int pid;

	// 2017-07-22
	rcu_scheduler_starting(); // NOP
	/*
	 * We need to spawn init first so that it obtains pid 1, however
	 * the init task will end up wanting to create kthreads, which, if
	 * we schedule it before we create kthreadd, will OOPS.
	 */
	// 2017-07-22
	// function은 cpu_context.r5에 저장하고 있다.
	// 2017-10-28 kernel_init 함수 분석 시작
	kernel_thread(kernel_init, NULL, CLONE_FS | CLONE_SIGHAND);
	// 2017-09-23, end
	// 2017-09-23
	numa_default_policy();	// NOP
	pid = kernel_thread(kthreadd, NULL, CLONE_FS | CLONE_FILES);
	rcu_read_lock();
	kthreadd_task = find_task_by_pid_ns(pid, &init_pid_ns);
	rcu_read_unlock();
	// kthreadd_done에 연결된 작업을 수행한다
	complete(&kthreadd_done);

	/*
	 * The boot idle thread must execute schedule()
	 * at least once to get things moving:
	 */
	// 2017-09-23
	// current에 idle_sched_class 등록
	init_idle_bootup_task(current);
	schedule_preempt_disabled();
	/* Call into cpu_idle with preempt disabled */
	// 2017-0923
	cpu_startup_entry(CPUHP_ONLINE);
}

/* Check for early params. */
// unused = "early_option"
static int __init do_early_param(char *param, char *val, const char *unused)
{
	const struct obs_kernel_param *p;

    // ex) drivers/tty/serial/8250/8250_early.c
    //     early_param("earlycon", setup_early_serial8250_console);
	for (p = __setup_start; p < __setup_end; p++) {
		if ((p->early && parameq(param, p->str)) ||
		    (strcmp(param, "console") == 0 &&
		     strcmp(p->str, "earlycon") == 0)
		) {
			if (p->setup_func(val) != 0)
				pr_warn("Malformed early option '%s'\n", param);
		}
	}
	/* We accept everything at this stage. */
	return 0;
}

// 2015-04-25
void __init parse_early_options(char *cmdline)
{
	parse_args("early options", cmdline, NULL, 0, 0, 0, do_early_param);
}

/* Arch code calls this early on, or if not, just before other parsing. */
// 2015-04-25
void __init parse_early_param(void)
{
	static __initdata int done = 0;
	static __initdata char tmp_cmdline[COMMAND_LINE_SIZE];

	if (done)
		return;

	/* All fall through to do_early_param. */
	strlcpy(tmp_cmdline, boot_command_line, COMMAND_LINE_SIZE);
	// 2015-04-25
	parse_early_options(tmp_cmdline);
	done = 1;
}

/*
 *	Activate the first processor.
 */

static void __init boot_cpu_init(void)
{
	int cpu = smp_processor_id();
	/* Mark the boot cpu "present", "online" etc for SMP and UP case */
	set_cpu_online(cpu, true);
	set_cpu_active(cpu, true);
	set_cpu_present(cpu, true);
	set_cpu_possible(cpu, true);
}

void __init __weak smp_setup_processor_id(void)
{
}

# if THREAD_SIZE >= PAGE_SIZE
// 2017-06-24
// weak 이므로 다른곳의 함수를 참조할 수 있음
void __init __weak thread_info_cache_init(void)
{
}
#endif

/*
 * Set up kernel memory allocators
 */
// 2015-05-09; 시작
// 2016-06-25 완료
static void __init mm_init(void)
{
	/*
	 * page_cgroup requires contiguous pages,
	 * bigger than MAX_ORDER unless SPARSEMEM.
	 */
	// 2015-05-09; 시작
	page_cgroup_init_flatmem(); // CONFIG_SPARSEMEM 활성화로 
	                            // 아무역활이 없음
				    // 2015-05-09; end
	// 2015-05-09; 시작
	mem_init();
	// 2015-05-16, end
	// 2015-05-15
	kmem_cache_init();
	// 2016-05-28 분석 완료; 
	
	// 2016-05-28 시작; 
	percpu_init_late();
	// 2016-05-28 끝; 
	pgtable_cache_init(); // No OP.
	// 2016-05-28 시작; 
	vmalloc_init();
	// 2016-06-25 완료
}

asmlinkage void __init start_kernel(void)
{
	char * command_line;
	extern const struct kernel_param __start___param[], __stop___param[];

	/*
	 * Need to run as early as possible, to initialize the
	 * lockdep hash:
	 */
	lockdep_init();
	smp_setup_processor_id();
	debug_objects_early_init();

	/*
	 * Set up the the initial canary ASAP:
	 */
	boot_init_stack_canary();

	cgroup_init_early();

	local_irq_disable();	//irq disable이 핵심
				//20140712
	early_boot_irqs_disabled = true;

/*
 * Interrupts are still disabled. Do necessary setups, then
 * enable them
 */
	boot_cpu_init();
	page_address_init();
	pr_notice("%s", linux_banner);
	setup_arch(&command_line);	//함수 내부 진행중 20140712
	// 2015-02-28; 여기까지

	// 2015-03-07, start
	// do nothing
	mm_init_owner(&init_mm, &init_task);	// CONFIG_MM_OWNER not define
	// 2015-03-07, end
	
	// do nothing
	mm_init_cpumask(&init_mm);

	// 2015-03-07, start
	setup_command_line(command_line);
	// 2015-03-07, end

	// 2015-03-07, start
	setup_nr_cpu_ids();	// 2015-03-07, 진행중
	                        // 2015-03-14, 완료

	// 2015-03-14, 시작
	setup_per_cpu_areas();  
	// 2015-03-21 분석 완료
	// 2015-03-21 시작
	// 확인하기로는 부팅 시 이 함수가 set_my_cpu_offset함수를 부르는 가장 마지막 로직
	smp_prepare_boot_cpu();	/* arch-specific boot-cpu hooks */
	// 2015-03-21 분석 완료
	// 2015-03-21 분석 시작
	build_all_zonelists(NULL, NULL);
	// 2015-04-04, end
	
	// 2015-04-04, start 
	// 2015-04-04, lru_add_drain_cpu 진행중
	// 2015-04-04, pagevec_lru_move_fn 기본만 봄.
	page_alloc_init();	
	// 2015-04-25, end

	// Kernel command line: console=ttySAC3,115200n8 debug earlyprintk ip=192.168.1.196:192.168.1.2:192.168.1.254:255.255.255.0:::none:192.168.1.254
	pr_notice("Kernel command line: %s\n", boot_command_line);
	parse_early_param();
	// 2015-05-02, unknown_bootoption 살펴봄 
	parse_args("Booting kernel", static_command_line, __start___param,
		   __stop___param - __start___param,
		   -1, -1, &unknown_bootoption);

	// 2015-04-25, start
	jump_label_init();	// Do nothings.
	// 2015-04-25, end

	/*
	 * These use large bootmem allocations and must precede
	 * kmem_cache_init()
	 */
	// 2015-04-25, start
	setup_log_buf(0);
	// 2015-04-25, end
	// 2015-04-25, start
	pidhash_init();
	// 2015-04-25, end
	// 2015-04-25, 여기까지
	vfs_caches_init_early();
	// 2015-04-25
	// 2015-05-02, start
	sort_main_extable();
	// 2015-05-02, end
	// 2015-05-02, 여기까지

	// 2015-05-09; 시작	
	trap_init(); // 아무 동작을하지 않는 함수
	             // 2015-05-09, end
	// 2015-05-09; 시작	
	mm_init();
	// 2016-06-25 완료

	/*
	 * Set up the scheduler prior starting any interrupts (such as the
	 * timer interrupt). Full topology setup happens at smp_init()
	 * time - but meanwhile we still have a functioning scheduler.
	 */
	// 2016-06-25 시작
	sched_init();
	// 2016-07-01 완료
	/*
	 * Disable preemption - early bootup scheduling is extremely
	 * fragile until we cpu_idle() for the first time.
	 */
	// 2016-07-09, 시작
	preempt_disable();
	if (WARN(!irqs_disabled(), "Interrupts were enabled *very* early, fixing it\n"))
		local_irq_disable();
	// 2016-07-09, 완료

	// 2016-07-09
	idr_init_cache();
	// 2016-07-16 완료
	
	// 2016-07-16 시작
	rcu_init();
	// 2016-07-23 end

	// 2016-07-23 start
	tick_nohz_init();	// NOP
	// 2016-07-23 end

	// 2016-07-23 start
	context_tracking_init();	// NOP
	// 2016-07-23 end

	// 2016-07-23 start
	radix_tree_init();
	// 2016-07-23 end
	// 2016-07-23, 여기까지
	
	// 2016-08-06 시작
	/* init some links before init_ISA_irqs() */
	// 2016-08-06 시작
	// irq 개수만큼 struct irq_desc 생성 및 radix tree에 추가
	early_irq_init();
	// 2016-08-06 완료
	// 2016-08-06 시작
	init_IRQ();
	// 2016-08-06 완료
	// 2016-08-06 시작
	tick_init();
	// 2016-08-06 완료
	// 2016-08-06 시작
	init_timers();
	// 2016-08-13 완료
	// 2016-08-13 start
	hrtimers_init();
	// 2016-08-13 end
	// 2016-08-13 start
	softirq_init();
	// 2016-08-13 end
	// 2016-08-13 start
	timekeeping_init();
	// 2016-08-13 end
	// 2016-08-13 start
	// arch별로 init
	time_init();
	// 2016-08-13 end
	// 2016-08-13 start
	sched_clock_postinit();
	// 2016-08-20 end
	// 2016-08-20 start
	perf_event_init();
	// 2017-06-10, end 
	// 2017-06-10, 여기까지
	profile_init(); // NOP
	// 2017-06-10, 여기까지
	// 2017-06-17, 여기부터
	call_function_init();
	// 2017-06-17
	// 이 시점에서 하드웨어 인터럽트가 설정되지 않은 것을 기대
	WARN(!irqs_disabled(), "Interrupts were enabled early\n");
	early_boot_irqs_disabled = false;
	// IRQ 활성화
	local_irq_enable();

	// 2017-06-17
	kmem_cache_init_late();

	/*
	 * HACK ALERT! This is early. We're enabling the console before
	 * we've done PCI setups etc, and console_init() must be aware of
	 * this. But we do want output early, in case something goes wrong.
	 */
	// 2017-06-17
	console_init();
	if (panic_later)
		panic(panic_later, panic_param);

	// 2017-06-17
	lockdep_info(); // NOP

	/*
	 * Need to run this when irqs are enabled, because it wants
	 * to self-test [hard/soft]-irqs on/off lock inversion bugs
	 * too:
	 */
	// 2017-06-17
	locking_selftest(); // NOP

#ifdef CONFIG_BLK_DEV_INITRD
	// glance
	if (initrd_start && !initrd_below_start_ok &&
	    page_to_pfn(virt_to_page((void *)initrd_start)) < min_low_pfn) {
		pr_crit("initrd overwritten (0x%08lx < 0x%08lx) - disabling it.\n",
		    page_to_pfn(virt_to_page((void *)initrd_start)),
		    min_low_pfn);
		initrd_start = 0;
	}
#endif
	// 2017-06-17
	page_cgroup_init();
	// 2017-06-17
	debug_objects_mem_init(); // NOP
	kmemleak_init(); // NOP
	// 2017-06-17
	// zone의 page 관리 객체(pageset) 초기화
	setup_per_cpu_pageset();
	numa_policy_init(); // NOP
	// late_time_init 함수 포인터는 미설정된 것으로 파악
	if (late_time_init)
		late_time_init();
	// 2017-06-17
	sched_clock_init();
	// 2017-06-17
	calibrate_delay();
	// 2017-06-17 여기까지
	
	// 2017-06-24 시작
	pidmap_init();
	anon_vma_init();
#ifdef CONFIG_X86 // not define
	if (efi_enabled(EFI_RUNTIME_SERVICES))
		efi_enter_virtual_mode();
#endif
	thread_info_cache_init(); // No OP.
	cred_init();
	fork_init(totalram_pages);
	proc_caches_init();
	buffer_init();
	key_init(); // No OP.
	security_init(); // No OP.
	dbg_late_init(); // No OP.
	// 2017-06-24 시작
	vfs_caches_init(totalram_pages);
	// 2017-07-01, end
	// 2017-07-01, start 
	signals_init();
	// 2017-07-01, end
	/* rootfs populating might need page-writeback */
	// 2017-07-01, start 
	page_writeback_init();
	// 2017-07-01, end
	// 2017-07-01, 여기까지
	// 2017-07-08, start
#ifdef CONFIG_PROC_FS
	// 2017-07-15
	proc_root_init();
	// 2017-07-15
#endif
	cgroup_init();	// NOP
	cpuset_init();	// NOP
	taskstats_init_early();	// NOP
	delayacct_init();	// NOP

	// 2017-07-15, start
	// address alliasing 체크
	check_bugs();
	// 2017-07-15, end

	// NOP
	acpi_early_init(); /* before LAPIC and SMP init */
	// NOP
	sfi_init_late();

	if (efi_enabled(EFI_RUNTIME_SERVICES)) {
		efi_late_init();	// NOP
		efi_free_boot_services();	// NOP
	}

	ftrace_init();	// NOP
	// 2017-07-15, 여기까지

	// 2017-07-22 시작
	/* Do the rest non-__init'ed, we're now alive */
	rest_init();
}

/* Call all constructor functions linked into the kernel. */
static void __init do_ctors(void)
{
#ifdef CONFIG_CONSTRUCTORS	// =n
	ctor_fn_t *fn = (ctor_fn_t *) __ctors_start;

	for (; fn < (ctor_fn_t *) __ctors_end; fn++)
		(*fn)();
#endif
}

// 2017-11-04
bool initcall_debug;
core_param(initcall_debug, initcall_debug, bool, 0644);

static int __init_or_module do_one_initcall_debug(initcall_t fn)
{
	ktime_t calltime, delta, rettime;
	unsigned long long duration;
	int ret;

	pr_debug("calling  %pF @ %i\n", fn, task_pid_nr(current));
	calltime = ktime_get();
	ret = fn();
	rettime = ktime_get();
	delta = ktime_sub(rettime, calltime);
	duration = (unsigned long long) ktime_to_ns(delta) >> 10;
	pr_debug("initcall %pF returned %d after %lld usecs\n",
		 fn, ret, duration);

	return ret;
}

// 2017-11-04
int __init_or_module do_one_initcall(initcall_t fn)
{
	int count = preempt_count();
	int ret;
	char msgbuf[64];

	if (initcall_debug)
		ret = do_one_initcall_debug(fn);
	else
		ret = fn();

	msgbuf[0] = 0;

	if (preempt_count() != count) {
		sprintf(msgbuf, "preemption imbalance ");
		preempt_count() = count;
	}
	if (irqs_disabled()) {
		strlcat(msgbuf, "disabled interrupts ", sizeof(msgbuf));
		local_irq_enable();
	}
	WARN(msgbuf[0], "initcall %pF returned with %s\n", fn, msgbuf);

	return ret;
}


extern initcall_t __initcall_start[];
extern initcall_t __initcall0_start[];
extern initcall_t __initcall1_start[];
extern initcall_t __initcall2_start[];
extern initcall_t __initcall3_start[];
extern initcall_t __initcall4_start[];
extern initcall_t __initcall5_start[];
extern initcall_t __initcall6_start[];
extern initcall_t __initcall7_start[];
extern initcall_t __initcall_end[];

// 2018-03-03
static initcall_t *initcall_levels[] __initdata = {
	__initcall0_start,
	__initcall1_start,
	__initcall2_start,
	__initcall3_start,
	__initcall4_start,
	__initcall5_start,
	__initcall6_start,
	__initcall7_start,
	__initcall_end,
};

/* Keep these in sync with initcalls in include/linux/init.h */
// 2018-03-03
static char *initcall_level_names[] __initdata = {
	"early",
	"core",
	"postcore",
	"arch",
	"subsys",
	"fs",
	"device",
	"late",
};

// 2018-03-03
static void __init do_initcall_level(int level)
{
	extern const struct kernel_param __start___param[], __stop___param[];
	initcall_t *fn;

	strcpy(static_command_line, saved_command_line);
	parse_args(initcall_level_names[level],
		   static_command_line, __start___param,
		   __stop___param - __start___param,
		   level, level,
		   &repair_env_string);

	for (fn = initcall_levels[level]; fn < initcall_levels[level+1]; fn++)
		do_one_initcall(*fn);
}

// 2018-03-03
static void __init do_initcalls(void)
{
	int level;

	for (level = 0; level < ARRAY_SIZE(initcall_levels) - 1; level++)
		do_initcall_level(level);
}

/*
 * Ok, the machine is now initialized. None of the devices
 * have been touched yet, but the CPU subsystem is up and
 * running, and memory and process management works.
 *
 * Now we can finally start doing some real work..
 */
// 2017-12-16
static void __init do_basic_setup(void)
{
	// NOP
	cpuset_init_smp();
	usermodehelper_init();
	// 2017-12-23, end
	// 2017-12-23, start
	shmem_init();
	// 2017-12-23, end
	// 2017-12-23, start
	// 2017-12-23, 여기까지
	driver_init();
	// 2018-03-03, end
	// 2018-03-03, start
	init_irq_proc();
	// 2018-03-03, end
	// 2018-03-03, start
	do_ctors();	// NOP
	// 2018-03-03, end
	// 2018-03-03, start
	usermodehelper_enable();
	// 2018-03-03, end
	// 2018-03-03, start
	do_initcalls();
	// 2018-03-03, end
	// 2018-03-03, start
	random_int_secret_init();
	// 2018-03-03, end
}

// 2017-11-04
static void __init do_pre_smp_initcalls(void)
{
	// 함수 포인터의 포인터 type
	initcall_t *fn;

	for (fn = __initcall_start; fn < __initcall0_start; fn++)
		do_one_initcall(*fn);
}

/*
 * This function requests modules which should be loaded by default and is
 * called twice right after initrd is mounted and right before init is
 * exec'd.  If such modules are on either initrd or rootfs, they will be
 * loaded before control is passed to userland.
 */
// 2018-03-03
void __init load_default_modules(void)
{
	load_default_elevator_module();
}

// 2018-03-10
static int run_init_process(const char *init_filename)
{
	argv_init[0] = init_filename;
	return do_execve(init_filename,
		(const char __user *const __user *)argv_init,
		(const char __user *const __user *)envp_init);
}

static noinline void __init kernel_init_freeable(void);

// 2017-10-28
static int __ref kernel_init(void *unused)
{
	kernel_init_freeable();
	// 2018-03-03, end
	/* need to finish all async __init code before freeing the memory */
	// 2018-03-03, start
	async_synchronize_full();
	// 2018-03-03, end, 여기까지
	// 2018-03-10 시작
	free_initmem();
	// NOP
	mark_rodata_ro();
	system_state = SYSTEM_RUNNING;
	// NOP
	numa_default_policy();

	// 2018-03-10
	flush_delayed_fput();
	// 2018-03-10

	// 2018-03-10
	// ramdisk_execute_command == "/init"
	if (ramdisk_execute_command) {
		if (!run_init_process(ramdisk_execute_command))
			return 0;
		pr_err("Failed to execute %s\n", ramdisk_execute_command);
	}

	/*
	 * We try each of these until one succeeds.
	 *
	 * The Bourne shell can be used instead of init if we are
	 * trying to recover a really broken machine.
	 */
	if (execute_command) {
		if (!run_init_process(execute_command))
			return 0;
		pr_err("Failed to execute %s.  Attempting defaults...\n",
			execute_command);
	}
	if (!run_init_process("/sbin/init") ||
	    !run_init_process("/etc/init") ||
	    !run_init_process("/bin/init") ||
	    !run_init_process("/bin/sh"))
		return 0;

	panic("No init found.  Try passing init= option to kernel. "
	      "See Linux Documentation/init.txt for guidance.");
}

// 2017-10-28
static noinline void __init kernel_init_freeable(void)
{
	/*
	 * Wait until kthreadd is all set-up.
	 */
	wait_for_completion(&kthreadd_done);

	/* Now the scheduler is fully set up and can do blocking allocations */
	gfp_allowed_mask = __GFP_BITS_MASK;

	/*
	 * init can allocate pages on any node
	 */
	// NOP
	set_mems_allowed(node_states[N_MEMORY]);
	/*
	 * init can run on any cpu.
	 */
	// CPU 정보 설정 및 현재 태스크를 목적지 cpu의 런큐로 이동
	// 이동 완료가 된 이후 함수가 리턴되는 것을 보장 (즉 현재와 다른 CPU에서 수행되는 함수에 대해 태스크를 수행)
	set_cpus_allowed_ptr(current, cpu_all_mask);

	cad_pid = task_pid(current);

	// 2017-10-28 시작
	smp_prepare_cpus(setup_max_cpus);
	// 2017-11-04, end

	// 2017-11-04, start
	do_pre_smp_initcalls();
	// 2017-11-04, end
	// 2017-11-04, start
	lockup_detector_init();	// NOP

	// 2017-11-04
	smp_init();
	// 2017-11-11 완료
	// 2017-11-11 start
	sched_init_smp();
	// 2017-12-09, end
	// 2017-12-09 여기까지

	// 2017-12-16 시작
	do_basic_setup();
	// 2018-03-03 end

	// 2018-03-03
	/* Open the /dev/console on the rootfs, this should never fail */
	if (sys_open((const char __user *) "/dev/console", O_RDWR, 0) < 0)
		pr_err("Warning: unable to open an initial console.\n");

	(void) sys_dup(0);
	(void) sys_dup(0);
	/*
	 * check if there is an early userspace init.  If yes, let it do all
	 * the work
	 */

	if (!ramdisk_execute_command)
		ramdisk_execute_command = "/init";

	if (sys_access((const char __user *) ramdisk_execute_command, 0) != 0) {
		ramdisk_execute_command = NULL;
		prepare_namespace();
	}

	/*
	 * Ok, we have completed the initial bootup, and
	 * we're essentially up and running. Get rid of the
	 * initmem segments and start the user-mode stuff..
	 */

	/* rootfs is available now, try loading default modules */
	// 2018-03-03
	load_default_modules();
}
