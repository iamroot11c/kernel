/* Rewritten by Rusty Russell, on the backs of many others...
   Copyright (C) 2001 Rusty Russell, 2002 Rusty Russell IBM.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include <linux/ftrace.h>
#include <linux/memory.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/init.h>

#include <asm/sections.h>
#include <asm/uaccess.h>

/*
 * mutex protecting text section modification (dynamic code patching).
 * some users need to sleep (allocating memory...) while they hold this lock.
 *
 * NOT exported to modules - patching kernel text is a really delicate matter.
 */
DEFINE_MUTEX(text_mutex);

extern struct exception_table_entry __start___ex_table[];
extern struct exception_table_entry __stop___ex_table[];

/* Cleared by build time tools if the table is already sorted. */
u32 __initdata main_extable_sort_needed = 1;

/* Sort the kernel's built-in exception table */
// 2015-05-02, 
// ex는 exception table이라고 추론함.
void __init sort_main_extable(void)
{
	// 아래 범위로 section을 할당했고,
	// 무엇이 존재하는지는, System.map서는 확인되지 않는다.
	// 28174 c0478000 R __start___ex_table
	// 28176 c0478ff8 R __stop___ex_table
	// 
	// table의 크기는 고정이다. 위치도 고정이다.
	// 코드를 보다보면, pushsection 같은 코드를 보게 되는데,
	// 이러한 코드들이, 이 영역이 들어갔다가 나왔다 하지 않을까? 생각해본다.
	// 그리고, 이러한 것들을 정렬해두지 않을까?
	// 
	// 그런데, sort_main_extable은 main에서만 호출하고 있다...
	// 동적으로 변경된다면, 변경될 때 마다, 업데이트가 되어야하지 않을까?
	//
	if (main_extable_sort_needed && __stop___ex_table > __start___ex_table) {
		pr_notice("Sorting __ex_table...\n");
		sort_extable(__start___ex_table, __stop___ex_table);
	}
}

/* Given an address, look for it in the exception tables. */
const struct exception_table_entry *search_exception_tables(unsigned long addr)
{
	const struct exception_table_entry *e;

	e = search_extable(__start___ex_table, __stop___ex_table-1, addr);
	if (!e)
		e = search_module_extables(addr);
	return e;
}

static inline int init_kernel_text(unsigned long addr)
{
	if (addr >= (unsigned long)_sinittext &&
	    addr <= (unsigned long)_einittext)
		return 1;
	return 0;
}

int core_kernel_text(unsigned long addr)
{
	if (addr >= (unsigned long)_stext &&
	    addr <= (unsigned long)_etext)
		return 1;

	if (system_state == SYSTEM_BOOTING &&
	    init_kernel_text(addr))
		return 1;
	return 0;
}

/**
 * core_kernel_data - tell if addr points to kernel data
 * @addr: address to test
 *
 * Returns true if @addr passed in is from the core kernel data
 * section.
 *
 * Note: On some archs it may return true for core RODATA, and false
 *  for others. But will always be true for core RW data.
 */
int core_kernel_data(unsigned long addr)
{
	if (addr >= (unsigned long)_sdata &&
	    addr < (unsigned long)_edata)
		return 1;
	return 0;
}

int __kernel_text_address(unsigned long addr)
{
	if (core_kernel_text(addr))
		return 1;
	if (is_module_text_address(addr))
		return 1;
	/*
	 * There might be init symbols in saved stacktraces.
	 * Give those symbols a chance to be printed in
	 * backtraces (such as lockdep traces).
	 *
	 * Since we are after the module-symbols check, there's
	 * no danger of address overlap:
	 */
	if (init_kernel_text(addr))
		return 1;
	return 0;
}

int kernel_text_address(unsigned long addr)
{
	if (core_kernel_text(addr))
		return 1;
	return is_module_text_address(addr);
}

/*
 * On some architectures (PPC64, IA64) function pointers
 * are actually only tokens to some data that then holds the
 * real function address. As a result, to find if a function
 * pointer is part of the kernel text, we need to do some
 * special dereferencing first.
 */
int func_ptr_is_kernel_text(void *ptr)
{
	unsigned long addr;
	addr = (unsigned long) dereference_function_descriptor(ptr);
	if (core_kernel_text(addr))
		return 1;
	return is_module_text_address(addr);
}
