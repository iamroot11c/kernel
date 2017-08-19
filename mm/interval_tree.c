/*
 * mm/interval_tree.c - interval tree for mapping->i_mmap
 *
 * Copyright (C) 2012, Michel Lespinasse <walken@google.com>
 *
 * This file is released under the GPL v2.
 */

#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/rmap.h>
#include <linux/interval_tree_generic.h>

static inline unsigned long vma_start_pgoff(struct vm_area_struct *v)
{
	return v->vm_pgoff;
}

// 2017-08-19
static inline unsigned long vma_last_pgoff(struct vm_area_struct *v)
{
	return v->vm_pgoff + ((v->vm_end - v->vm_start) >> PAGE_SHIFT) - 1;
}

INTERVAL_TREE_DEFINE(struct vm_area_struct, shared.linear.rb,
		     unsigned long, shared.linear.rb_subtree_last,
		     vma_start_pgoff, vma_last_pgoff,, vma_interval_tree)

// 2017-08-19
/* Insert node immediately after prev in the interval tree */
// 1) prev에서 node를 삽입할 노드 주소를 찾아 삽입한다.
// 2) node 정보를 root에 삽입한다.
void vma_interval_tree_insert_after(struct vm_area_struct *node,
				    struct vm_area_struct *prev,
				    struct rb_root *root)
{
	struct rb_node **link;
	struct vm_area_struct *parent;
	unsigned long last = vma_last_pgoff(node);

	VM_BUG_ON(vma_start_pgoff(node) != vma_start_pgoff(prev));

	if (!prev->shared.linear.rb.rb_right) {
		parent = prev;
		link = &prev->shared.linear.rb.rb_right;
	} else {
		// 지금 동작은 parent 기준 가장 마지막 자식  왼 쪽 노드를 얻어오는 것으로 보임
		parent = rb_entry(prev->shared.linear.rb.rb_right,
				  struct vm_area_struct, shared.linear.rb);
		if (parent->shared.linear.rb_subtree_last < last)
			parent->shared.linear.rb_subtree_last = last;
		while (parent->shared.linear.rb.rb_left) {
			parent = rb_entry(parent->shared.linear.rb.rb_left,
				struct vm_area_struct, shared.linear.rb);
			if (parent->shared.linear.rb_subtree_last < last)
				parent->shared.linear.rb_subtree_last = last;
		}
		link = &parent->shared.linear.rb.rb_left;
	}

	node->shared.linear.rb_subtree_last = last;
	rb_link_node(&node->shared.linear.rb, &parent->shared.linear.rb, link);
	rb_insert_augmented(&node->shared.linear.rb, root,
			    &vma_interval_tree_augment);
}

static inline unsigned long avc_start_pgoff(struct anon_vma_chain *avc)
{
	return vma_start_pgoff(avc->vma);
}

static inline unsigned long avc_last_pgoff(struct anon_vma_chain *avc)
{
	return vma_last_pgoff(avc->vma);
}

// 2017-08-19
INTERVAL_TREE_DEFINE(struct anon_vma_chain, rb, unsigned long, rb_subtree_last,
		     avc_start_pgoff, avc_last_pgoff,
		     static inline, __anon_vma_interval_tree)

// 2017-08-19
// root에 anon_vma_chain값을 삽입
// 자세한 함수 구현은 INTERVAL_TREE_DEFINE 매크로 참조
void anon_vma_interval_tree_insert(struct anon_vma_chain *node,
				   struct rb_root *root)
{
#ifdef CONFIG_DEBUG_VM_RB // not set
	node->cached_vma_start = avc_start_pgoff(node);
	node->cached_vma_last = avc_last_pgoff(node);
#endif
	__anon_vma_interval_tree_insert(node, root);
}

void anon_vma_interval_tree_remove(struct anon_vma_chain *node,
				   struct rb_root *root)
{
	__anon_vma_interval_tree_remove(node, root);
}

// 2015-08-15
// &anon_vma->rb_root, pgoff, pgoff
struct anon_vma_chain *
anon_vma_interval_tree_iter_first(struct rb_root *root,
				  unsigned long first, unsigned long last)
{
	// __anon_vma_interval_tree_iter_first는
	// 매트로 INTERVAL_TREE_DEFINE에 의해서 생성된다.
	return __anon_vma_interval_tree_iter_first(root, first, last);
}

struct anon_vma_chain *
anon_vma_interval_tree_iter_next(struct anon_vma_chain *node,
				 unsigned long first, unsigned long last)
{
	return __anon_vma_interval_tree_iter_next(node, first, last);
}

#ifdef CONFIG_DEBUG_VM_RB
void anon_vma_interval_tree_verify(struct anon_vma_chain *node)
{
	WARN_ON_ONCE(node->cached_vma_start != avc_start_pgoff(node));
	WARN_ON_ONCE(node->cached_vma_last != avc_last_pgoff(node));
}
#endif
