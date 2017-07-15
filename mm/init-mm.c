#include <linux/mm_types.h>
#include <linux/rbtree.h>
#include <linux/rwsem.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/cpumask.h>

#include <linux/atomic.h>
#include <asm/pgtable.h>
#include <asm/mmu.h>

#ifndef INIT_MM_CONTEXT
#define INIT_MM_CONTEXT(name)
#endif

// 2017-07-15
struct mm_struct init_mm = {
	.mm_rb		= RB_ROOT/* {NULL} */,
	.pgd		= swapper_pg_dir,   // swapper_pg_dir[PTRS_PER_PGD];
	.mm_users	= ATOMIC_INIT(2),   // 2로 초기화 하네요.
	.mm_count	= ATOMIC_INIT(1),   // 1로 초기화 함.
	.mmap_sem	= __RWSEM_INITIALIZER(init_mm.mmap_sem),
	.page_table_lock =  __SPIN_LOCK_UNLOCKED(init_mm.page_table_lock), // spinlock init
	.mmlist		= LIST_HEAD_INIT(init_mm.mmlist),
	INIT_MM_CONTEXT(init_mm)   // 코드가 없다..., 뭐죠?
};
