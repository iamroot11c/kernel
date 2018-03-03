/*
 * Copyright (c) 2002-3 Patrick Mochel
 * Copyright (c) 2002-3 Open Source Development Labs
 *
 * This file is released under the GPLv2
 */

#include <linux/device.h>
#include <linux/init.h>
#include <linux/memory.h>

#include "base.h"

/**
 * driver_init - initialize driver model.
 *
 * Call the driver model init functions to initialize their
 * subsystems. Called early from init/main.c.
 */
// 2017-12-23
void __init driver_init(void)
{
	/* These are the core pieces */
	// 2018-01-06
	devtmpfs_init();
	// 2018-01-13, end
	// 2018-01-13, start
	devices_init();
	// 2018-02-03, end
	// 2018-02-03, start
	buses_init();
	// 2018-02-03
	classes_init();
	// 2018-02-03
	firmware_init();
	// 2018-02-03
	hypervisor_init();

	/* These are also core pieces, but must come after the
	 * core core pieces.
	 */
	// 2018-02-03
	platform_bus_init();
	// 2018-03-03
	cpu_dev_init();
	// 2018-03-03, end
	// 2018-03-03, start
	memory_dev_init();	// NOP
	// 2018-03-03, end
}
