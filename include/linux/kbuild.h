#ifndef __LINUX_KBUILD_H
#define __LINUX_KBUILD_H

// 2016-04-23
// __asm__ __volatile__ (asms : output : input : clobber);
// DEFINE(PAGE_SZ,               PAGE_SIZE);
// asm volatile("\n->PAGE_SZ %0 " PAGE_SIZE : : "i" (val))
#define DEFINE(sym, val) \
        asm volatile("\n->" #sym " %0 " #val : : "i" (val))

#define BLANK() asm volatile("\n->" : : )

#define OFFSET(sym, str, mem) \
	DEFINE(sym, offsetof(struct str, mem))

#define COMMENT(x) \
	asm volatile("\n->#" x)

#endif
