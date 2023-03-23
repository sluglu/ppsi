/*
 * Copyright (C) 2012-2020 CERN (www.cern.ch)
 * Author: Aurelio Colosimo, Adam Wujek
 *
 * Released to the public domain
 */

#include "net.h"
#include <ppsi/ppsi.h>
#include <softpll_ng.h>
#include <hal_exports.h>
#include "wrpc.h"
#include "../proto-ext-whiterabbit/wr-constants.h"
#include "board.h"

int32_t wrpc_get_clock_period(void)
{
	return REF_CLOCK_PERIOD_PS;
}

int wrpc_read_calibration_data(struct pp_instance *ppi,
			       TimeInterval *scaledBitSlide,
			       RelativeDifference *scaledDelayCoefficient,
			       TimeInterval *scaledSfpDeltaTx,
			       TimeInterval *scaledSfpDeltaRx)
{
	struct wrc_port_state state;

	wrpc_get_port_state(&state);

	*scaledDelayCoefficient = (RelativeDifference) state.calib.alpha;

	*scaledBitSlide = picos_to_interval((int64_t)state.calib.bitslide_ps);
	
	/* check if tx is calibrated,
	 * if so read data */
	*scaledSfpDeltaTx = picos_to_interval(state.calib.delta_tx_ps);

	/* check if rx is calibrated,
	 * if so read data */
	*scaledSfpDeltaRx = picos_to_interval(state.calib.delta_rx_ps);

	return WRH_HW_CALIB_OK;
}
