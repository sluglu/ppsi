/*
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Aurelio Colosimo
 * Based on ptp-noposix project (see AUTHORS for details)
 *
 * Released to the public domain
 */

#include <ppsi/ppsi.h>

int wr_s_lock(struct pp_instance *ppi, void *buf, int len)
{
	struct wr_dsport *wrp = WR_DSPOR(ppi);
	int enable = 0;
	int poll_ret;

	if (ppi->is_new_state) {
		wrp->wrStateRetry = WR_STATE_RETRY;
		__pp_timeout_set(ppi, PP_TO_EXT_0, WR_S_LOCK_TIMEOUT_MS*(WR_STATE_RETRY+1));
		enable = 1;
	} else {
		int rms=pp_next_delay_1(ppi, PP_TO_EXT_0);
		if ( rms==0 || rms<(wrp->wrStateRetry*WR_S_LOCK_TIMEOUT_MS)) {
			WRH_OPER()->locking_disable(ppi);
			if (wr_handshake_retry(ppi))
				enable = 1;
			else
				return 0; /* non-wr already */
			}
	}

	if (enable) {
		WRH_OPER()->locking_enable(ppi);
	}

	ppi->next_delay = pp_next_delay_1(ppi,PP_TO_EXT_0)-wrp->wrStateRetry*WR_S_LOCK_TIMEOUT_MS;

	poll_ret = WRH_OPER()->locking_poll(ppi, 0);
	if (poll_ret == WRH_SPLL_READY) {
		ppi->next_state = WRS_LOCKED;
		WRH_OPER()->locking_disable(ppi);
	} else if (poll_ret == WRH_SPLL_CALIB_NOT_READY) {
		/* rxts_calibration not ready, enter the same state without
		 * a delay */
		ppi->next_delay = 0;
	}

	/* Calibration can take time so we restart the BMC timer to avoid aged foreign master removed. */
	pp_timeout_set(ppi, PP_TO_BMC);

	return 0;
}
