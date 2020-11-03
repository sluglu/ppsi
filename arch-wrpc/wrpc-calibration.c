/*
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Aurelio Colosimo
 *
 * Released to the public domain
 */

#include <dev/endpoint.h>
#include <ppsi/ppsi.h>
#include <softpll_ng.h>
#include <hal_exports.h>
#include "wrpc.h"
#include "../proto-ext-whiterabbit/wr-constants.h"
#include "board.h"

extern int32_t sfp_alpha;

int wrpc_read_calibration_data(struct pp_instance *ppi,
	uint32_t *deltaTx, uint32_t *deltaRx, int32_t *fix_alpha,
	int32_t *clock_period)
{
	struct hal_port_state state;

	wrpc_get_port_state(&state, ppi->iface_name);

	/* check if the data is available */
	if (fix_alpha)
		/* take local alpha instead of HAL */
		*fix_alpha = sfp_alpha;

	if (clock_period)
		*clock_period = state.clock_period;

	/* check if tx is calibrated,
	 * if so read data */
	if (state.calib.tx_calibrated) {
		if (deltaTx)
			*deltaTx = state.calib.delta_tx_phy
					+ state.calib.sfp.delta_tx_ps
					+ state.calib.delta_tx_board;
	} else
		return WRH_HW_CALIB_NOT_FOUND;

	/* check if rx is calibrated,
	 * if so read data */
	if (state.calib.rx_calibrated) {
		if (deltaRx)
			*deltaRx = state.calib.delta_rx_phy
					+ state.calib.sfp.delta_rx_ps
					+ state.calib.delta_rx_board;
	} else
		return WRH_HW_CALIB_NOT_FOUND;

	return WRH_HW_CALIB_OK;
}
