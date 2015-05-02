#ifndef _LINUX_SORT_H
#define _LINUX_SORT_H

#include <linux/types.h>

// 2015-05-02
void sort(void *base, size_t num, size_t size,
	  int (*cmp)(const void *, const void *),
	  void (*swap)(void *, void *, int));

#endif
