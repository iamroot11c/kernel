#ifndef _LINUX_THREADS_H
#define _LINUX_THREADS_H


/*
 * The default limit for the nr of threads is now in
 * /proc/sys/kernel/threads-max.
 */

/*
 * Maximum supported processors.  Setting this smaller saves quite a
 * bit of memory.  Use nr_cpu_ids instead of this except for static bitmaps.
 */
#ifndef CONFIG_NR_CPUS
/* FIXME: This should be fixed in the arch's Kconfig */
#define CONFIG_NR_CPUS	1
#endif

/* Places which use this should consider cpumask_var_t. */
// 2016-04-09, 현재 분석 기준으로는 2개
#define NR_CPUS		CONFIG_NR_CPUS

#define MIN_THREADS_LEFT_FOR_ROOT 4

/*
 * This controls the default maximum pid allocated to a process
 */
// 2017-06-24
#define PID_MAX_DEFAULT (CONFIG_BASE_SMALL/*0*/ ? 0x1000 : 0x8000)

/*
 * A maximum of 4 million PIDs should be enough for a while.
 * [NOTE: PID/TIDs are limited to 2^29 ~= 500+ million, see futex.h.]
 */
// 2017-06-25
#define PID_MAX_LIMIT (CONFIG_BASE_SMALL/*0*/ ? PAGE_SIZE * 8 : \
	(sizeof(long) > 4 ? 4 * 1024 * 1024 : PID_MAX_DEFAULT)) // 0x8000
// 2015-03-07,  0x8000

/*
 * Define a minimum number of pids per cpu.  Heuristically based
 * on original pid max of 32k for 32 cpus.  Also, increase the
 * minimum settable value for pid_max on the running system based
 * on similar defaults.  See kernel/pid.c:pidmap_init() for details.
 */
// 2016-06-24
#define PIDS_PER_CPU_DEFAULT	1024
#define PIDS_PER_CPU_MIN	8

#endif
