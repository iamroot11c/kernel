#ifndef _LINUX_NUMA_H
#define _LINUX_NUMA_H


#ifdef CONFIG_NODES_SHIFT // CONFIG_NODES_SHIFT 미 정의
#define NODES_SHIFT     CONFIG_NODES_SHIFT
#else
#define NODES_SHIFT     0
#endif

// 2016-01-16;
#define MAX_NUMNODES    (1 << NODES_SHIFT) // 1

#define	NUMA_NO_NODE	(-1)

#endif /* _LINUX_NUMA_H */
