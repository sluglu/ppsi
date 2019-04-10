#ifndef __HAL_EXPORTS_C
#define __HAL_EXPORTS_C

#include <minipc.h>
#include <hal_exports.h>


/* Export structures, shared by server and client for argument matching */

struct minipc_pd __rpcdef_check_running = {
	.name = "check_running",
	.retval = MINIPC_ARG_ENCODE(MINIPC_ATYPE_INT, int),
	.args = {
		 MINIPC_ARG_END,
		 },
};

//int halexp_reset_port(const char *port_name);
struct minipc_pd __rpcdef_reset_port = {
	.name = "reset_port",
	.retval = MINIPC_ARG_ENCODE(MINIPC_ATYPE_INT, int),
	.args = {
		 MINIPC_ARG_ENCODE(MINIPC_ATYPE_STRING, char *),
		 MINIPC_ARG_END,
		 },
};

//int halexp_calibration_cmd(const char *port_name, int command, int on_off);
struct minipc_pd __rpcdef_calibration_cmd = {
	.name = "calibration_cmd",
	.retval = MINIPC_ARG_ENCODE(MINIPC_ATYPE_INT, int),
	.args = {
		 MINIPC_ARG_ENCODE(MINIPC_ATYPE_STRING, char *),
		 MINIPC_ARG_ENCODE(MINIPC_ATYPE_INT, int),
		 MINIPC_ARG_ENCODE(MINIPC_ATYPE_INT, int),
		 MINIPC_ARG_END,
		 },
};

//int halexp_lock_cmd(const char *port_name, int command, int priority);
struct minipc_pd __rpcdef_lock_cmd = {
	.name = "lock_cmd",
	.retval = MINIPC_ARG_ENCODE(MINIPC_ATYPE_INT, int),
	.args = {
		 MINIPC_ARG_ENCODE(MINIPC_ATYPE_STRING, char *),
		 MINIPC_ARG_ENCODE(MINIPC_ATYPE_INT, int),
		 MINIPC_ARG_ENCODE(MINIPC_ATYPE_INT, int),
		 MINIPC_ARG_END,
		 },
};

//int halexp_pps_cmd(int cmd, hexp_pps_params_t *params);
struct minipc_pd __rpcdef_pps_cmd = {
	.name = "pps_cmd",
	.retval = MINIPC_ARG_ENCODE(MINIPC_ATYPE_INT, int),
	.args = {
		 MINIPC_ARG_ENCODE(MINIPC_ATYPE_INT, int),
		 MINIPC_ARG_ENCODE(MINIPC_ATYPE_STRUCT, hexp_pps_params_t),
		 MINIPC_ARG_END,
		 },
};

struct minipc_pd __rpcdef_get_timing_state = {
	.name = "get_timing_state",
	.retval = MINIPC_ARG_ENCODE(MINIPC_ATYPE_STRUCT, hexp_timing_state_t),
	.args = {
		 MINIPC_ARG_END,
		 },
};

#endif
