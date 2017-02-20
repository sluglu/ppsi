/*
 * Copyright (C) 2017 GSI (www.gsi.de)
 * Author: Alessandro Rubini
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <ppsi/ppsi.h>
#include "wrpc.h"
#include <errno.h>
#include "shell.h"
#include "syscon.h"

extern struct pp_instance ppi_static;

static int cmd_fault(const char *args[])
{
	struct pp_globals *ppg = ppi_static.glbs;

	if (args[0] && !strcmp(args[0], "drop")) {
		if (args[1])
			fromdec(args[1], &ppg->rxdrop);
		if (args[2])
			fromdec(args[2], &ppg->txdrop);
		ppsi_drop_init(ppg, timer_get_tics());
		pp_printf("dropping %i/1000 rx,  %i/1000 tx\n",
			  ppg->rxdrop, ppg->txdrop);
		return 0;
	}
	pp_printf("Use: \"fault drop [<rxdrop> <txdrop>]\" (0..999)\n");
	return -EINVAL;
}

DEFINE_WRC_COMMAND(fault) = {
        .name = "fault",
        .exec = cmd_fault,
};

