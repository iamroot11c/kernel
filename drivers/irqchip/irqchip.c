/*
 * Copyright (C) 2012 Thomas Petazzoni
 *
 * Thomas Petazzoni <thomas.petazzoni@free-electrons.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/init.h>
#include <linux/of_irq.h>

#include "irqchip.h"

/*
 * This special of_device_id is the sentinel at the end of the
 * of_device_id[] array of all irqchips. It is automatically placed at
 * the end of the array by the linker, thanks to being part of a
 * special section.
 */
static const struct of_device_id
irqchip_of_match_end __used __section(__irqchip_of_end);

// 2016-08-06
// include/asm-generic/vmlinux.lds.h에서 
// IRQCHIP_OF_MATCH_TABLE 매크로를 통해 정의
// __irqchip_of_table의 구성은 drivers/irqchip/exynos-combiner.c 파일 안의
//
// IRQCHIP_DECLARE(exynos4210_combiner, "samsung,exynos4210-combiner",
//                 combiner_of_init);
//
// 과 같이 구성이 됨
// 
// #define IRQCHIP_DECLARE(name,compstr,fn)                \
//      static const struct of_device_id irqchip_of_match_##name    \
//      __used __section(__irqchip_of_table)                \
//      = { .compatible = compstr, .data = fn }
//
// static const struct of_device_id irqchip_of_match_exynos4210_combiner
//  __used __section(__irqchip_of_table)
//   = { .compatible = "samsung,exynos4210-combiner",
//       .data = combiner_of_init };
extern struct of_device_id __irqchip_begin[];

// 2016-08-06
void __init irqchip_init(void)
{
	of_irq_init(__irqchip_begin);
}
