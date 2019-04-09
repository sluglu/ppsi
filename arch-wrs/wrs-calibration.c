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

int wrs_read_calibration_data(struct pp_instance *ppi,int32_t *clock_period, uint32_t *bit_slide_ps)
{
	struct hal_port_state *p;

	p = pp_wrs_lookup_port(ppi->iface_name);
	if (!p)
		return WRH_HW_CALIB_NOT_FOUND;

	if(!p->calib.tx_calibrated || !p->calib.rx_calibrated)
		return WRH_HW_CALIB_NOT_FOUND;

	if ( bit_slide_ps )
		*bit_slide_ps=p->calib.bitslide_ps;
	if(clock_period)
		*clock_period =  16000; /* REF_CLOCK_PERIOD_PS */
	return WRH_HW_CALIB_OK;
}
