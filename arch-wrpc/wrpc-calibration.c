/*
 * Copyright (C) 2012-2020 CERN (www.cern.ch)
 * Author: Aurelio Colosimo, Adam Wujek
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

extern int64_t sfp_alpha;

int wrpc_read_calibration_data(
			       struct pp_instance *ppi,
			       int32_t *clock_period,
			       TimeInterval *scaledBitSlide,
			       RelativeDifference *scaledDelayCoefficient,
			       TimeInterval *scaledSfpDeltaTx,
			       TimeInterval *scaledSfpDeltaRx)
{
	struct wrc_port_state state;

	if (wrpc_get_port_state(&state, ppi->iface_name))
		return WRH_HW_CALIB_NOT_FOUND;

	if (scaledDelayCoefficient)
		*scaledDelayCoefficient= (RelativeDifference) sfp_alpha;

	if (scaledBitSlide)
		*scaledBitSlide = picos_to_interval((int64_t)state.calib.bitslide_ps);
	
	if (clock_period)
		*clock_period = state.clock_period;

	/* check if tx is calibrated,
	 * if so read data */
	if (scaledSfpDeltaTx) {
		*scaledSfpDeltaTx = picos_to_interval(state.calib.delta_tx_ps);
	}

	/* check if rx is calibrated,
	 * if so read data */
	if (scaledSfpDeltaRx) {
		*scaledSfpDeltaRx = picos_to_interval(state.calib.delta_rx_ps);
	}

	return WRH_HW_CALIB_OK;
}
