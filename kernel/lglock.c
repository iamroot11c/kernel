/* See include/linux/lglock.h for description */
#include <linux/module.h>
#include <linux/lglock.h>
#include <linux/cpu.h>
#include <linux/string.h>

/*
 * Note there is no uninit, so lglocks cannot be defined in
 * modules (but it's fine to use them from there)
 * Could be added though, just undo lg_lock_init
 */
// 2017-06-24
// lg_lock_init(&files_lglock, "files_lglock")
// lg_lock_init(&vfsmount_lock, "vfsmount_lock")
void lg_lock_init(struct lglock *lg, char *name)
{
	LOCKDEP_INIT_MAP(&lg->lock_dep_map, name, &lg->lock_key, 0); // No OP.
}
EXPORT_SYMBOL(lg_lock_init);

// 2018-03-10
void lg_local_lock(struct lglock *lg)
{
	arch_spinlock_t *lock;

	preempt_disable();
	// NOP
	lock_acquire_shared(&lg->lock_dep_map, 0, 0, NULL, _RET_IP_);
	lock = this_cpu_ptr(lg->lock);
	arch_spin_lock(lock);
}
EXPORT_SYMBOL(lg_local_lock);

void lg_local_unlock(struct lglock *lg)
{
	arch_spinlock_t *lock;

	lock_release(&lg->lock_dep_map, 1, _RET_IP_);
	lock = this_cpu_ptr(lg->lock);
	arch_spin_unlock(lock);
	preempt_enable();
}
EXPORT_SYMBOL(lg_local_unlock);

void lg_local_lock_cpu(struct lglock *lg, int cpu)
{
	arch_spinlock_t *lock;

	preempt_disable();
	lock_acquire_shared(&lg->lock_dep_map, 0, 0, NULL, _RET_IP_);
	lock = per_cpu_ptr(lg->lock, cpu);
	arch_spin_lock(lock);
}
EXPORT_SYMBOL(lg_local_lock_cpu);

void lg_local_unlock_cpu(struct lglock *lg, int cpu)
{
	arch_spinlock_t *lock;

	lock_release(&lg->lock_dep_map, 1, _RET_IP_);
	lock = per_cpu_ptr(lg->lock, cpu);
	arch_spin_unlock(lock);
	preempt_enable();
}
EXPORT_SYMBOL(lg_local_unlock_cpu);

void lg_global_lock(struct lglock *lg)
{
	int i;

	preempt_disable();
	lock_acquire_exclusive(&lg->lock_dep_map, 0, 0, NULL, _RET_IP_);
	for_each_possible_cpu(i) {
		arch_spinlock_t *lock;
		lock = per_cpu_ptr(lg->lock, i);
		arch_spin_lock(lock);
	}
}
EXPORT_SYMBOL(lg_global_lock);

void lg_global_unlock(struct lglock *lg)
{
	int i;

	lock_release(&lg->lock_dep_map, 1, _RET_IP_);
	for_each_possible_cpu(i) {
		arch_spinlock_t *lock;
		lock = per_cpu_ptr(lg->lock, i);
		arch_spin_unlock(lock);
	}
	preempt_enable();
}
EXPORT_SYMBOL(lg_global_unlock);
