/*
 * Tag parsing.
 *
 * Copyright (C) 1995-2001 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * This is the traditional way of passing data to the kernel at boot time.  Rather
 * than passing a fixed inflexible structure to the kernel, we pass a list
 * of variable-sized tags to the kernel.  The first tag must be a ATAG_CORE
 * tag for the list to be recognised (to distinguish the tagged list from
 * a param_struct).  The list is terminated with a zero-length tag (this tag
 * is not parsed in any way).
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/root_dev.h>
#include <linux/screen_info.h>

#include <asm/setup.h>
#include <asm/system_info.h>
#include <asm/page.h>
#include <asm/mach/arch.h>

#include "atags.h"

static char default_command_line[COMMAND_LINE_SIZE] __initdata = CONFIG_CMDLINE;

#ifndef MEM_SIZE
#define MEM_SIZE	(16*1024*1024)
#endif

static struct {
	struct tag_header hdr1;
	struct tag_core   core;
	struct tag_header hdr2;
	struct tag_mem32  mem;
	struct tag_header hdr3;
} default_tags __initdata = {
	{ tag_size(tag_core), ATAG_CORE }, // tag_size (8+12)/4 = 5   ATAG_CORE = 0x54410001
	{ 1, PAGE_SIZE, 0xff }, // PAGE_SIZE = 4096
	{ tag_size(tag_mem32), ATAG_MEM }, // (8+8)/4 = 4 ATAG_MEM = 0x54410002 
	{ MEM_SIZE }, // 16Mega
	{ 0, ATAG_NONE } // ATAG_NONE = 0 
};

static int __init parse_tag_core(const struct tag *tag)
{
	if (tag->hdr.size > 2) {
		if ((tag->u.core.flags & 1) == 0)
			root_mountflags &= ~MS_RDONLY; // root_mountflags = 2^15+1 에서  MS_RDONLY = 1 를 클리어 = 2^15 
		ROOT_DEV = old_decode_dev(tag->u.core.rootdev); // 16bit reoot_dev 를 32bit 로 바꿔줌
	}
	return 0;
}

__tagtable(ATAG_CORE, parse_tag_core);

static int __init parse_tag_mem32(const struct tag *tag)
{
	return arm_add_memory(tag->u.mem.start, tag->u.mem.size);
}

__tagtable(ATAG_MEM, parse_tag_mem32);

#if defined(CONFIG_VGA_CONSOLE) || defined(CONFIG_DUMMY_CONSOLE)
static int __init parse_tag_videotext(const struct tag *tag)
{
	screen_info.orig_x            = tag->u.videotext.x;
	screen_info.orig_y            = tag->u.videotext.y;
	screen_info.orig_video_page   = tag->u.videotext.video_page;
	screen_info.orig_video_mode   = tag->u.videotext.video_mode;
	screen_info.orig_video_cols   = tag->u.videotext.video_cols;
	screen_info.orig_video_ega_bx = tag->u.videotext.video_ega_bx;
	screen_info.orig_video_lines  = tag->u.videotext.video_lines;
	screen_info.orig_video_isVGA  = tag->u.videotext.video_isvga;
	screen_info.orig_video_points = tag->u.videotext.video_points;
	return 0;
}

__tagtable(ATAG_VIDEOTEXT, parse_tag_videotext);
#endif

#ifdef CONFIG_BLK_DEV_RAM
static int __init parse_tag_ramdisk(const struct tag *tag)
{
	extern int rd_size, rd_image_start, rd_prompt, rd_doload;
	// rd_size = 8k 
	
	rd_image_start = tag->u.ramdisk.start;
	rd_doload = (tag->u.ramdisk.flags & 1) == 0; //0=load 1=prompt
	rd_prompt = (tag->u.ramdisk.flags & 2) == 0;

	if (tag->u.ramdisk.size)
		rd_size = tag->u.ramdisk.size;

	return 0;
}

__tagtable(ATAG_RAMDISK, parse_tag_ramdisk);
#endif

static int __init parse_tag_serialnr(const struct tag *tag)
{
	system_serial_low = tag->u.serialnr.low;
	system_serial_high = tag->u.serialnr.high;
	return 0;
}

__tagtable(ATAG_SERIAL, parse_tag_serialnr);

static int __init parse_tag_revision(const struct tag *tag)
{
	system_rev = tag->u.revision.rev;
	return 0;
}

__tagtable(ATAG_REVISION, parse_tag_revision);

static int __init parse_tag_cmdline(const struct tag *tag)
{
#if defined(CONFIG_CMDLINE_EXTEND)
	strlcat(default_command_line, " ", COMMAND_LINE_SIZE);
	strlcat(default_command_line, tag->u.cmdline.cmdline,
		COMMAND_LINE_SIZE);
#elif defined(CONFIG_CMDLINE_FORCE)
	pr_warning("Ignoring tag cmdline (using the default kernel command line)\n");
#else
	strlcpy(default_command_line, tag->u.cmdline.cmdline,
		COMMAND_LINE_SIZE); // COMMAND_LINE_SIZE = 1024 or 512
#endif
	return 0;
}

__tagtable(ATAG_CMDLINE, parse_tag_cmdline);

/*
 * Scan the tag table for this tag, and call its parse function.
 * The tag table is built by the linker from all the __tagtable
 * declarations.
 */
static int __init parse_tag(const struct tag *tag)
{
	extern struct tagtable __tagtable_begin, __tagtable_end;
	struct tagtable *t;

	for (t = &__tagtable_begin; t < &__tagtable_end; t++)
		if (tag->hdr.tag == t->tag) {
			t->parse(tag);
			break;
		}

	return t < &__tagtable_end;
}

/*
 * Parse all tags in the list, checking both the global and architecture
 * specific tag tables.
 */
static void __init parse_tags(const struct tag *t)
{
	for (; t->hdr.size; t = tag_next(t))
		if (!parse_tag(t))
			printk(KERN_WARNING
				"Ignoring unrecognised tag 0x%08x\n",
				t->hdr.tag);
}

static void __init squash_mem_tags(struct tag *tag)
{
	for (; tag->hdr.size; tag = tag_next(tag))
		if (tag->hdr.tag == ATAG_MEM)
			tag->hdr.tag = ATAG_NONE;
}

const struct machine_desc * __init
setup_machine_tags(phys_addr_t __atags_pointer, unsigned int machine_nr)
{
	struct tag *tags = (struct tag *)&default_tags;
	const struct machine_desc *mdesc = NULL, *p;
	char *from = default_command_line;

	default_tags.mem.start = PHYS_OFFSET; // 0x40000000 exynos 
										  // mem size 는default_tags 에서 16 M 로 초기화되어있음

	/*
	 * locate machine in the list of supported machines.
	 */
	for_each_machine_desc(p)
		if (machine_nr == p->nr) {
			printk("Machine: %s\n", p->name);
			mdesc = p;
			break;
		}

	if (!mdesc) {
		early_print("\nError: unrecognized/unsupported machine ID"
			    " (r1 = 0x%08x).\n\n", machine_nr);
		dump_machine_table(); /* does not return */
	}

	if (__atags_pointer)
		tags = phys_to_virt(__atags_pointer); // tag pointer 를 가상주소로 
	else if (mdesc->atag_offset)
		tags = (void *)(PAGE_OFFSET + mdesc->atag_offset); //0xc0000000 + atag_offset = atag_offset 의 가상주소
	// 위 두가지 경우에 해당사항이 없으면 default_tags를 사용함

#if defined(CONFIG_DEPRECATED_PARAM_STRUCT)
	/*
	 * If we have the old style parameters, convert them to
	 * a tag list.
	 */
	if (tags->hdr.tag != ATAG_CORE)
		convert_to_tag_list(tags);
#endif
	if (tags->hdr.tag != ATAG_CORE) { //0x54410001  예외처리
		early_print("Warning: Neither atags nor dtb found\n");
		tags = (struct tag *)&default_tags;
	}

	// __mach_desc_EXYNOS5_DT 에fixup은 정의되어있지 않음 아래if문은 실행되지않음
	// arch/arm/mach-exynos/mach-exynos5-dt.c 참고	
	// fixup은 memory subsystem이 초기화되기 전에 실행된다.
	if (mdesc->fixup)  		
		mdesc->fixup(tags, &from, &meminfo);

	if (tags->hdr.tag == ATAG_CORE) {
		if (meminfo.nr_banks != 0) 
			squash_mem_tags(tags); // nr_banks 가 있으면 기존의 tags 메모리  정보를뿌셔버림 
		save_atags(tags);
		parse_tags(tags); 
		// arch/arm/include/asm/setup.h 
		//#define __tag __used __attribute__((__section__(".taglist.init")))
		//#define __tagtable(tag, fn) 
		//static const struct tagtable __tagtable_##fn __tag = { tag, fn }
		//위 define을 사용해서 아래struct 변수를 생성하고 tag이름과 function pointer를 설정함
		//ex)arch/arm/kernel/atags_parse.c __tagtable(ATAG_RAMDISK, parse_tag_ramdisk);
		//__tagtable_parse_tag_cmdline
		//__tagtable_parse_tag_revision
		//__tagtable_parse_tag_serialnr
		//__tagtable_parse_tag_ramdisk
		//__tagtable_parse_tag_videotext
		//__tagtable_parse_tag_mem32
		//__tagtable_parse_tag_core
		//__tagtable_parse_tag_initrd2
		//__tagtable_parse_tag_initrd
		//__smpalt_begin

	}

	/* parse_early_param needs a boot_command_line */
	strlcpy(boot_command_line, from, COMMAND_LINE_SIZE);

	return mdesc;
}
