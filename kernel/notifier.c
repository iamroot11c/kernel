#include <linux/kdebug.h>
#include <linux/kprobes.h>
#include <linux/export.h>
#include <linux/notifier.h>
#include <linux/rcupdate.h>
#include <linux/vmalloc.h>
#include <linux/reboot.h>

/*
 *	Notifier list for kernel code which wants to be called
 *	at shutdown. This is used to stop any idling DMA operations
 *	and the like.
 */
// 2017-06-10
BLOCKING_NOTIFIER_HEAD(reboot_notifier_list);

/*
 *	Notifier chain core routines.  The exported routines below
 *	are layered on top of these, with appropriate locking added.
 */

// 2015-04-04
// notifier_block은 linked list 형태로 관리되며,
// 우선순위를 고려해서 순서가 결정 된다.
// 내림차순 정렬이다.
// 2016-05-28
//
// 2016-07-23
// nh->head(nl) == slab_notifier
// slab_notifier -> NULL
// notifier_chain_register(&cpu_chain, &rcu_cpu_notify_nb)
// 우선 순위가 동일한 것으로 판단되어서, 결과가 아래와 같을 것으로 예상한다.
// 2016-08-13
// slab_notifier -> rcu_cpu_notify_nb -> radix_tree_callback_nb -> timer_nb -> NULL
// slab_notifier -> rcu_cpu_notify_nb -> radix_tree_callback_nb -> timer_nb -> hrtimers_nb -> NULL
// slab_notifier -> rcu_cpu_notify_nb -> radix_tree_callback_nb -> timer_nb -> hrtimers_nb 
// -> remote_softirq_cpu_notifier -> NULL

// 2016-07-23
// 전달된 chain에 따라서 다르게 구성된다.
// blocking_notifier_chain_register(&pm_chain_head, nb);
//
// 동작 : nl 링크드리스트에 n 노드를 삽입한다.(삽입 기준. 우선 순위 별 내림차순)
static int notifier_chain_register(struct notifier_block **nl,
		struct notifier_block *n)
{
	// 2015-04-04, 현재는 *nl은 null
	// 우선순위 형태로 정렬
	// 동일 우선 순위가 삽입될 때, 새로운 멤버는 가장 마지막에 추가된다.
	// NOTE: 만약, nl이 NULL이라면, *nl은 Segmentation Fault이다.

	// 2016-07-23
	// 이전 분석에 의해서, 아래와 같은 상태이다.
	// nh->head(nl) == slab_notifier
	// slab_notifier -> NULL
	// rcu_cpu_notify_nb.priority == 0
	//
	// nl 링크드 리스트를 순회하면서 
	// 처음 n보다 우선순위가 낮은 head노드( == 마지막으로 우선순위가 높은 노드의 next 노드)
	// 를 얻어온다.
	while ((*nl) != NULL) {
		if (n->priority > (*nl)->priority)
			break;
		// address of '((*nl)->next)'이 assign되고 있다.
		// 그러므로, NULL이 할당되는 것은 아니다.
		nl = &((*nl)->next);
	}

	// n이 맨마지막에 삽입된다면, n->next = NULL;
	// n이 우선순위가 높아서, 중간에 삽입된다면, *nl앞에 삽입된다.
	n->next = *nl;
	
	// *nl = n;
	// n의 
	// (이유 : nl == &(nl_before_node)->next이다.
	// 따라서 *nl = n은 nl 전 노드가 이동할 노드를 n으로 변경하는 동작이다.)
	rcu_assign_pointer(*nl, n);
	return 0;
}

static int notifier_chain_cond_register(struct notifier_block **nl,
		struct notifier_block *n)
{
	while ((*nl) != NULL) {
		if ((*nl) == n)
			return 0;
		if (n->priority > (*nl)->priority)
			break;
		nl = &((*nl)->next);
	}
	n->next = *nl;
	rcu_assign_pointer(*nl, n);
	return 0;
}

static int notifier_chain_unregister(struct notifier_block **nl,
		struct notifier_block *n)
{
	while ((*nl) != NULL) {
		if ((*nl) == n) {
			rcu_assign_pointer(*nl, n->next);
			return 0;
		}
		nl = &((*nl)->next);
	}
	return -ENOENT;
}

/**
 * notifier_call_chain - Informs the registered notifiers about an event.
 *	@nl:		Pointer to head of the blocking notifier chain
 *	@val:		Value passed unmodified to notifier function
 *	@v:		Pointer passed unmodified to notifier function
 *	@nr_to_call:	Number of notifier functions to be called. Don't care
 *			value of this parameter is -1.
 *	@nr_calls:	Records the number of notifications sent. Don't care
 *			value of this field is NULL.
 *	@returns:	notifier_call_chain returns the value returned by the
 *			last notifier function called.
 */
// 2016-01-30
// ret = notifier_call_chain(&nh->head, 0, &freed, -1, NULL);
// 2016-12-17
// notifier_call_chain(thread_notify_head->head, 
//                     #THREAD_NOTIFY_SWITCH/*2*/,
//                     task_thread_info(next),
//                     -1, NULL);
static int __kprobes notifier_call_chain(struct notifier_block **nl,
					unsigned long val, void *v,
					int nr_to_call,	int *nr_calls)
{
	int ret = NOTIFY_DONE;
	struct notifier_block *nb, *next_nb;

	nb = rcu_dereference_raw(*nl);

	// 초기 : nr_to_call(-1)
	// 
	// 2016-12-17
	// 아직 thread_notify 리스트가 구성되지 않음
	// 등록은 thread_register_notifier() 함수를 통해 이루어지는데
	// 이 함수가 아직 호출되지 않음
	while (nb && nr_to_call) {
		next_nb = rcu_dereference_raw(nb->next);

#ifdef CONFIG_DEBUG_NOTIFIERS // not define
		if (unlikely(!func_ptr_is_kernel_text(nb->notifier_call))) {
			WARN(1, "Invalid notifier called!");
			nb = next_nb;
			continue;
		}
#endif
		// 등록한 function 호출
		// 무엇을 등록했는지는 모르겠음
		ret = nb->notifier_call(nb, val, v);

		// 2016-01-30, nr_calls : NULL임
		if (nr_calls)
			(*nr_calls)++;

		if ((ret & NOTIFY_STOP_MASK) == NOTIFY_STOP_MASK)
			break;
		nb = next_nb;
		nr_to_call--;
	}
	return ret;
}

/*
 *	Atomic notifier chain routines.  Registration and unregistration
 *	use a spinlock, and call_chain is synchronized by RCU (no locks).
 */

/**
 *	atomic_notifier_chain_register - Add notifier to an atomic notifier chain
 *	@nh: Pointer to head of the atomic notifier chain
 *	@n: New entry in notifier chain
 *
 *	Adds a notifier to an atomic notifier chain.
 *
 *	Currently always returns zero.
 */
int atomic_notifier_chain_register(struct atomic_notifier_head *nh,
		struct notifier_block *n)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&nh->lock, flags);
	ret = notifier_chain_register(&nh->head, n);
	spin_unlock_irqrestore(&nh->lock, flags);
	return ret;
}
EXPORT_SYMBOL_GPL(atomic_notifier_chain_register);

/**
 *	atomic_notifier_chain_unregister - Remove notifier from an atomic notifier chain
 *	@nh: Pointer to head of the atomic notifier chain
 *	@n: Entry to remove from notifier chain
 *
 *	Removes a notifier from an atomic notifier chain.
 *
 *	Returns zero on success or %-ENOENT on failure.
 */
int atomic_notifier_chain_unregister(struct atomic_notifier_head *nh,
		struct notifier_block *n)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&nh->lock, flags);
	ret = notifier_chain_unregister(&nh->head, n);
	spin_unlock_irqrestore(&nh->lock, flags);
	synchronize_rcu();
	return ret;
}
EXPORT_SYMBOL_GPL(atomic_notifier_chain_unregister);

/**
 *	__atomic_notifier_call_chain - Call functions in an atomic notifier chain
 *	@nh: Pointer to head of the atomic notifier chain
 *	@val: Value passed unmodified to notifier function
 *	@v: Pointer passed unmodified to notifier function
 *	@nr_to_call: See the comment for notifier_call_chain.
 *	@nr_calls: See the comment for notifier_call_chain.
 *
 *	Calls each function in a notifier chain in turn.  The functions
 *	run in an atomic context, so they must not block.
 *	This routine uses RCU to synchronize with changes to the chain.
 *
 *	If the return value of the notifier can be and'ed
 *	with %NOTIFY_STOP_MASK then atomic_notifier_call_chain()
 *	will return immediately, with the return value of
 *	the notifier function which halted execution.
 *	Otherwise the return value is the return value
 *	of the last notifier function called.
 */
// 2016-12-17
// __atomic_notifier_call_chain(thread_notify_head, 
//                            #THREAD_NOTIFY_SWITCH/*2*/,
//                            task_thread_info(next),
//                            -1, NULL);
int __kprobes __atomic_notifier_call_chain(struct atomic_notifier_head *nh,
					unsigned long val, void *v,
					int nr_to_call, int *nr_calls)
{
	int ret;

	rcu_read_lock();
	ret = notifier_call_chain(&nh->head, val, v, nr_to_call, nr_calls);
	rcu_read_unlock();
	return ret;
}
EXPORT_SYMBOL_GPL(__atomic_notifier_call_chain);

// 2016-12-17
// atomic_notifier_call_chain(thread_notify_head, 
//                            #THREAD_NOTIFY_SWITCH/*2*/,
//                            task_thread_info(next));
int __kprobes atomic_notifier_call_chain(struct atomic_notifier_head *nh,
		unsigned long val, void *v)
{
	return __atomic_notifier_call_chain(nh, val, v, -1, NULL);
}
EXPORT_SYMBOL_GPL(atomic_notifier_call_chain);

/*
 *	Blocking notifier chain routines.  All access to the chain is
 *	synchronized by an rwsem.
 */

/**
 *	blocking_notifier_chain_register - Add notifier to a blocking notifier chain
 *	@nh: Pointer to head of the blocking notifier chain
 *	@n: New entry in notifier chain
 *
 *	Adds a notifier to a blocking notifier chain.
 *	Must be called in process context.
 *
 *	Currently always returns zero.
 */
// 2016-07-23
// 동기화를 고려한 chain register
int blocking_notifier_chain_register(struct blocking_notifier_head *nh,
		struct notifier_block *n)
{
	int ret;

	/*
	 * This code gets used during boot-up, when task switching is
	 * not yet working and interrupts must remain disabled.  At
	 * such times we must not call down_write().
	 */
	// booting 상태라면, 동기화를 고려하지 않는다.
	if (unlikely(system_state == SYSTEM_BOOTING))
		return notifier_chain_register(&nh->head, n);

	down_write(&nh->rwsem);
	ret = notifier_chain_register(&nh->head, n);
	up_write(&nh->rwsem);
	return ret;
}
EXPORT_SYMBOL_GPL(blocking_notifier_chain_register);

/**
 *	blocking_notifier_chain_cond_register - Cond add notifier to a blocking notifier chain
 *	@nh: Pointer to head of the blocking notifier chain
 *	@n: New entry in notifier chain
 *
 *	Adds a notifier to a blocking notifier chain, only if not already
 *	present in the chain.
 *	Must be called in process context.
 *
 *	Currently always returns zero.
 */
int blocking_notifier_chain_cond_register(struct blocking_notifier_head *nh,
		struct notifier_block *n)
{
	int ret;

	down_write(&nh->rwsem);
	ret = notifier_chain_cond_register(&nh->head, n);
	up_write(&nh->rwsem);
	return ret;
}
EXPORT_SYMBOL_GPL(blocking_notifier_chain_cond_register);

/**
 *	blocking_notifier_chain_unregister - Remove notifier from a blocking notifier chain
 *	@nh: Pointer to head of the blocking notifier chain
 *	@n: Entry to remove from notifier chain
 *
 *	Removes a notifier from a blocking notifier chain.
 *	Must be called from process context.
 *
 *	Returns zero on success or %-ENOENT on failure.
 */
int blocking_notifier_chain_unregister(struct blocking_notifier_head *nh,
		struct notifier_block *n)
{
	int ret;

	/*
	 * This code gets used during boot-up, when task switching is
	 * not yet working and interrupts must remain disabled.  At
	 * such times we must not call down_write().
	 */
	if (unlikely(system_state == SYSTEM_BOOTING))
		return notifier_chain_unregister(&nh->head, n);

	down_write(&nh->rwsem);
	ret = notifier_chain_unregister(&nh->head, n);
	up_write(&nh->rwsem);
	return ret;
}
EXPORT_SYMBOL_GPL(blocking_notifier_chain_unregister);

/**
 *	__blocking_notifier_call_chain - Call functions in a blocking notifier chain
 *	@nh: Pointer to head of the blocking notifier chain
 *	@val: Value passed unmodified to notifier function
 *	@v: Pointer passed unmodified to notifier function
 *	@nr_to_call: See comment for notifier_call_chain.
 *	@nr_calls: See comment for notifier_call_chain.
 *
 *	Calls each function in a notifier chain in turn.  The functions
 *	run in a process context, so they are allowed to block.
 *
 *	If the return value of the notifier can be and'ed
 *	with %NOTIFY_STOP_MASK then blocking_notifier_call_chain()
 *	will return immediately, with the return value of
 *	the notifier function which halted execution.
 *	Otherwise the return value is the return value
 *	of the last notifier function called.
 *
 *      NOTIFY_STOP_MASK가 리턴되면 바로 리턴
 *      리턴값은, 가장 마지막에 호출된 the last notifier function이다.
 */
// 2016-01-30
// __blocking_notifier_call_chain(nh, 0, v, -1, NULL);
// 2016-09-24
// __blocking_notifier_call_chain(&dev->bus->p->bus_notifier,
//                                      BUS_NOTIFY_DEL_DEVICE, dev, -1, NULL);
int __blocking_notifier_call_chain(struct blocking_notifier_head *nh,
				   unsigned long val, void *v,
				   int nr_to_call, int *nr_calls)
{
	int ret = NOTIFY_DONE;

	/*
	 * We check the head outside the lock, but if this access is
	 * racy then it does not matter what the result of the test
	 * is, we re-check the list after having taken the lock anyway:
	 */
        // rcu_dereference_raw() 차주에 보자(2016-01-30)
	if (rcu_dereference_raw(nh->head)) {
		down_read(&nh->rwsem);
		ret = notifier_call_chain(&nh->head, val, v, nr_to_call,
					nr_calls);
		up_read(&nh->rwsem);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(__blocking_notifier_call_chain);

// 2016-01-30
// 2016-09-24
// blocking_notifier_call_chain(&dev->bus->p->bus_notifier,
//                                     BUS_NOTIFY_DEL_DEVICE, dev);
// 2017-04-22
// blocking_notifier_call_chain(c->notifiers,
//                               (unsigned long)curr_value, NULL);
// blocking_notifier_call_chain(&dev_pm_notifiers,
//                               (unsigned long)value, req);
// 2017-05-13
// blocking_notifier_call_chain(&dev->bus->p->bus_notifier,
//                                BUS_NOTIFY_UNBIND_DRIVER,
//                                                        dev);
// blocking_notifier_call_chain(&dev->bus->p->bus_notifier,
//				BUS_NOTIFY_UNBOUND_DRIVER/*0x00000006*/,
//				dev);
int blocking_notifier_call_chain(struct blocking_notifier_head *nh,
		unsigned long val, void *v)
{
	return __blocking_notifier_call_chain(nh, val, v, -1, NULL);
}
EXPORT_SYMBOL_GPL(blocking_notifier_call_chain);

/*
 *	Raw notifier chain routines.  There is no protection;
 *	the caller must provide it.  Use at your own risk!
 */

/**
 *	raw_notifier_chain_register - Add notifier to a raw notifier chain
 *	@nh: Pointer to head of the raw notifier chain
 *	@n: New entry in notifier chain
 *
 *	Adds a notifier to a raw notifier chain.
 *	All locking must be provided by the caller.
 *
 *	Currently always returns zero.
 */
// 2015-04-04
// 2016-05-28
// raw_notifier_chain_register(&cpu_chain, &slab_notifier)
// 2016-07-23
// raw_notifier_chain_register(&cpu_chain, &rcu_cpu_notify_nb)
// raw_notifier_chain_register(&cpu_chain, &radix_tree_callback_nb)
// 2016-08-13
// raw_notifier_chain_register(&cpu_chain, &timer_nb);
// raw_notifier_chain_register(&cpu_chain, &hrtimers_nb);
// raw_notifier_chain_register(&cpu_chain, &remote_softirq_cpu_notifier);
int raw_notifier_chain_register(struct raw_notifier_head *nh,
		struct notifier_block *n)
{
	return notifier_chain_register(&nh->head, n);
}
EXPORT_SYMBOL_GPL(raw_notifier_chain_register);

/**
 *	raw_notifier_chain_unregister - Remove notifier from a raw notifier chain
 *	@nh: Pointer to head of the raw notifier chain
 *	@n: Entry to remove from notifier chain
 *
 *	Removes a notifier from a raw notifier chain.
 *	All locking must be provided by the caller.
 *
 *	Returns zero on success or %-ENOENT on failure.
 */
int raw_notifier_chain_unregister(struct raw_notifier_head *nh,
		struct notifier_block *n)
{
	return notifier_chain_unregister(&nh->head, n);
}
EXPORT_SYMBOL_GPL(raw_notifier_chain_unregister);

/**
 *	__raw_notifier_call_chain - Call functions in a raw notifier chain
 *	@nh: Pointer to head of the raw notifier chain
 *	@val: Value passed unmodified to notifier function
 *	@v: Pointer passed unmodified to notifier function
 *	@nr_to_call: See comment for notifier_call_chain.
 *	@nr_calls: See comment for notifier_call_chain
 *
 *	Calls each function in a notifier chain in turn.  The functions
 *	run in an undefined context.
 *	All locking must be provided by the caller.
 *
 *	If the return value of the notifier can be and'ed
 *	with %NOTIFY_STOP_MASK then raw_notifier_call_chain()
 *	will return immediately, with the return value of
 *	the notifier function which halted execution.
 *	Otherwise the return value is the return value
 *	of the last notifier function called.
 */
int __raw_notifier_call_chain(struct raw_notifier_head *nh,
			      unsigned long val, void *v,
			      int nr_to_call, int *nr_calls)
{
	return notifier_call_chain(&nh->head, val, v, nr_to_call, nr_calls);
}
EXPORT_SYMBOL_GPL(__raw_notifier_call_chain);

int raw_notifier_call_chain(struct raw_notifier_head *nh,
		unsigned long val, void *v)
{
	return __raw_notifier_call_chain(nh, val, v, -1, NULL);
}
EXPORT_SYMBOL_GPL(raw_notifier_call_chain);

/*
 *	SRCU notifier chain routines.    Registration and unregistration
 *	use a mutex, and call_chain is synchronized by SRCU (no locks).
 */

/**
 *	srcu_notifier_chain_register - Add notifier to an SRCU notifier chain
 *	@nh: Pointer to head of the SRCU notifier chain
 *	@n: New entry in notifier chain
 *
 *	Adds a notifier to an SRCU notifier chain.
 *	Must be called in process context.
 *
 *	Currently always returns zero.
 */
int srcu_notifier_chain_register(struct srcu_notifier_head *nh,
		struct notifier_block *n)
{
	int ret;

	/*
	 * This code gets used during boot-up, when task switching is
	 * not yet working and interrupts must remain disabled.  At
	 * such times we must not call mutex_lock().
	 */
	if (unlikely(system_state == SYSTEM_BOOTING))
		return notifier_chain_register(&nh->head, n);

	mutex_lock(&nh->mutex);
	ret = notifier_chain_register(&nh->head, n);
	mutex_unlock(&nh->mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(srcu_notifier_chain_register);

/**
 *	srcu_notifier_chain_unregister - Remove notifier from an SRCU notifier chain
 *	@nh: Pointer to head of the SRCU notifier chain
 *	@n: Entry to remove from notifier chain
 *
 *	Removes a notifier from an SRCU notifier chain.
 *	Must be called from process context.
 *
 *	Returns zero on success or %-ENOENT on failure.
 */
int srcu_notifier_chain_unregister(struct srcu_notifier_head *nh,
		struct notifier_block *n)
{
	int ret;

	/*
	 * This code gets used during boot-up, when task switching is
	 * not yet working and interrupts must remain disabled.  At
	 * such times we must not call mutex_lock().
	 */
	if (unlikely(system_state == SYSTEM_BOOTING))
		return notifier_chain_unregister(&nh->head, n);

	mutex_lock(&nh->mutex);
	ret = notifier_chain_unregister(&nh->head, n);
	mutex_unlock(&nh->mutex);
	synchronize_srcu(&nh->srcu);
	return ret;
}
EXPORT_SYMBOL_GPL(srcu_notifier_chain_unregister);

/**
 *	__srcu_notifier_call_chain - Call functions in an SRCU notifier chain
 *	@nh: Pointer to head of the SRCU notifier chain
 *	@val: Value passed unmodified to notifier function
 *	@v: Pointer passed unmodified to notifier function
 *	@nr_to_call: See comment for notifier_call_chain.
 *	@nr_calls: See comment for notifier_call_chain
 *
 *	Calls each function in a notifier chain in turn.  The functions
 *	run in a process context, so they are allowed to block.
 *
 *	If the return value of the notifier can be and'ed
 *	with %NOTIFY_STOP_MASK then srcu_notifier_call_chain()
 *	will return immediately, with the return value of
 *	the notifier function which halted execution.
 *	Otherwise the return value is the return value
 *	of the last notifier function called.
 */
int __srcu_notifier_call_chain(struct srcu_notifier_head *nh,
			       unsigned long val, void *v,
			       int nr_to_call, int *nr_calls)
{
	int ret;
	int idx;

	idx = srcu_read_lock(&nh->srcu);
	ret = notifier_call_chain(&nh->head, val, v, nr_to_call, nr_calls);
	srcu_read_unlock(&nh->srcu, idx);
	return ret;
}
EXPORT_SYMBOL_GPL(__srcu_notifier_call_chain);

int srcu_notifier_call_chain(struct srcu_notifier_head *nh,
		unsigned long val, void *v)
{
	return __srcu_notifier_call_chain(nh, val, v, -1, NULL);
}
EXPORT_SYMBOL_GPL(srcu_notifier_call_chain);

/**
 *	srcu_init_notifier_head - Initialize an SRCU notifier head
 *	@nh: Pointer to head of the srcu notifier chain
 *
 *	Unlike other sorts of notifier heads, SRCU notifier heads require
 *	dynamic initialization.  Be sure to call this routine before
 *	calling any of the other SRCU notifier routines for this head.
 *
 *	If an SRCU notifier head is deallocated, it must first be cleaned
 *	up by calling srcu_cleanup_notifier_head().  Otherwise the head's
 *	per-cpu data (used by the SRCU mechanism) will leak.
 */
void srcu_init_notifier_head(struct srcu_notifier_head *nh)
{
	mutex_init(&nh->mutex);
	if (init_srcu_struct(&nh->srcu) < 0)
		BUG();
	nh->head = NULL;
}
EXPORT_SYMBOL_GPL(srcu_init_notifier_head);

static ATOMIC_NOTIFIER_HEAD(die_chain);

int notrace __kprobes notify_die(enum die_val val, const char *str,
	       struct pt_regs *regs, long err, int trap, int sig)
{
	struct die_args args = {
		.regs	= regs,
		.str	= str,
		.err	= err,
		.trapnr	= trap,
		.signr	= sig,

	};
	return atomic_notifier_call_chain(&die_chain, val, &args);
}

int register_die_notifier(struct notifier_block *nb)
{
	vmalloc_sync_all();
	return atomic_notifier_chain_register(&die_chain, nb);
}
EXPORT_SYMBOL_GPL(register_die_notifier);

int unregister_die_notifier(struct notifier_block *nb)
{
	return atomic_notifier_chain_unregister(&die_chain, nb);
}
EXPORT_SYMBOL_GPL(unregister_die_notifier);
