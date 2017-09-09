#ifndef _LINUX_PID_NS_H
#define _LINUX_PID_NS_H

#include <linux/sched.h>
#include <linux/bug.h>
#include <linux/mm.h>
#include <linux/workqueue.h>
#include <linux/threads.h>
#include <linux/nsproxy.h>
#include <linux/kref.h>

// 2017-06-24
// 2017-09-09
struct pidmap {
       atomic_t nr_free;
       void *page;
};

#define BITS_PER_PAGE		(PAGE_SIZE * 8) // 4096 * 8 = 32768
#define BITS_PER_PAGE_MASK	(BITS_PER_PAGE-1)   // 32767, 0x7fff
#define PIDMAP_ENTRIES		((PID_MAX_LIMIT/*0x8000(32768)*/+BITS_PER_PAGE-1)/BITS_PER_PAGE) // (32768 + 32767) / 32768 = 1// 2^15

struct bsd_acct_struct;

// 2015-03-07
// 2016-03-05
// 2017-06-24
struct pid_namespace {
	struct kref kref;
    // 2017-09-09
	struct pidmap pidmap[PIDMAP_ENTRIES/*1*/];
	int last_pid;
	unsigned int nr_hashed;
    // 2017-09-09
	struct task_struct *child_reaper;
	struct kmem_cache *pid_cachep;
	unsigned int level;
	struct pid_namespace *parent;
#ifdef CONFIG_PROC_FS // set
	struct vfsmount *proc_mnt;
	struct dentry *proc_self;
#endif
#ifdef CONFIG_BSD_PROCESS_ACCT // not set
	struct bsd_acct_struct *bacct;
#endif
	struct user_namespace *user_ns;
	struct work_struct proc_work;
	kgid_t pid_gid;
	int hide_pid;
	int reboot;	/* group exit code if this pidns was rebooted */
	unsigned int proc_inum;
};

extern struct pid_namespace init_pid_ns;

// 2017-06-24
#define PIDNS_HASH_ADDING (1U << 31) // 0x1000_0000

#ifdef CONFIG_PID_NS    // =y
// 2017-09-09
static inline struct pid_namespace *get_pid_ns(struct pid_namespace *ns)
{
    // ref count 증가
	if (ns != &init_pid_ns)
		kref_get(&ns->kref);
	return ns;
}

extern struct pid_namespace *copy_pid_ns(unsigned long flags,
	struct user_namespace *user_ns, struct pid_namespace *ns);
extern void zap_pid_ns_processes(struct pid_namespace *pid_ns);
extern int reboot_pid_ns(struct pid_namespace *pid_ns, int cmd);
extern void put_pid_ns(struct pid_namespace *ns);

#else /* !CONFIG_PID_NS */
#include <linux/err.h>

static inline struct pid_namespace *get_pid_ns(struct pid_namespace *ns)
{
	return ns;
}

static inline struct pid_namespace *copy_pid_ns(unsigned long flags,
	struct user_namespace *user_ns, struct pid_namespace *ns)
{
	if (flags & CLONE_NEWPID)
		ns = ERR_PTR(-EINVAL);
	return ns;
}

static inline void put_pid_ns(struct pid_namespace *ns)
{
}

static inline void zap_pid_ns_processes(struct pid_namespace *ns)
{
	BUG();
}

static inline int reboot_pid_ns(struct pid_namespace *pid_ns, int cmd)
{
	return 0;
}
#endif /* CONFIG_PID_NS */

extern struct pid_namespace *task_active_pid_ns(struct task_struct *tsk);
void pidhash_init(void);
void pidmap_init(void);

#endif /* _LINUX_PID_NS_H */
