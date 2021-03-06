#ifndef _LINUX_HUGETLB_INLINE_H
#define _LINUX_HUGETLB_INLINE_H

#ifdef CONFIG_HUGETLB_PAGE  // =n

#include <linux/mm.h>

static inline int is_vm_hugetlb_page(struct vm_area_struct *vma)
{
	return !!(vma->vm_flags & VM_HUGETLB);
}

#else

// 2015-08-15
// 2017-08-19
static inline int is_vm_hugetlb_page(struct vm_area_struct *vma)
{
	return 0;
}

#endif

#endif
