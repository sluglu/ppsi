/*
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Aurelio Colosimo
 *
 * Released to the public domain
 */

#include <ppsi/ppsi.h>
#include <math.h>

#define HAL_EXPORT_STRUCTURES
#include <ppsi-wrs.h>

#include <hal_exports.h>

int wrs_read_calibration_data(struct pp_instance *ppi,int32_t *clock_period, TimeInterval *scaledBitSlide,
		RelativeDifference *scaledDelayCoefficient,
		TimeInterval *scaledSfpDeltaTx, TimeInterval *scaledSfpDeltaRx)
{
	struct hal_port_state *p;

	p = pp_wrs_lookup_port(ppi->iface_name);
	if (!p)
		return WRH_HW_CALIB_NOT_FOUND;

	if ( scaledBitSlide )
		*scaledBitSlide=picos_to_interval( (int64_t) p->calib.bitslide_ps);
	if(clock_period)
		*clock_period =  16000; /* REF_CLOCK_PERIOD_PS */
	if ( scaledDelayCoefficient )
		*scaledDelayCoefficient=(RelativeDifference)(p->calib.sfp.alpha * REL_DIFF_TWO_POW_FRACBITS);
	if ( scaledSfpDeltaTx )
		*scaledSfpDeltaTx= picos_to_interval( (int64_t) p->calib.sfp.delta_tx_ps);
	if ( scaledSfpDeltaRx )
		*scaledSfpDeltaRx= picos_to_interval( (int64_t) p->calib.sfp.delta_rx_ps);
	return WRH_HW_CALIB_OK;
}
