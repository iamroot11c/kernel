#ifndef _LINUX_PFN_H_
#define _LINUX_PFN_H_

#ifndef __ASSEMBLY__
#include <linux/types.h>
#endif

#define PFN_ALIGN(x)	(((unsigned long)(x) + (PAGE_SIZE - 1)) & PAGE_MASK)
#define PFN_UP(x)	(((x) + PAGE_SIZE-1) >> PAGE_SHIFT)	// 올림. 예) (5000 + 4095) >> 12 = 2
#define PFN_DOWN(x)	((x) >> PAGE_SHIFT)					// 버림. 예) (5000) >> 12 = 1
#define PFN_PHYS(x)	((phys_addr_t)(x) << PAGE_SHIFT)

#endif
