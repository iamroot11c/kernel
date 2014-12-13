/*
 *  linux/arch/arm/lib/testclearbit.S
 *
 *  Copyright (C) 1995-1996 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/linkage.h>
#include <asm/assembler.h>
#include "bitops.h"
                .text

/*
* 2014-12-13, testop은 매크로이며, 정의는 arch/arm/lib/bitops.h에 있음
*/
testop	_test_and_clear_bit, bicne, strne
