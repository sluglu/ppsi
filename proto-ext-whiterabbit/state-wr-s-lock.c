/*
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Aurelio Colosimo
 * Based on ptp-noposix project (see AUTHORS for details)
 *
 * Released to the public domain
 */

#include <ppsi/ppsi.h>

#define WR_TMO_NAME "WR_SLOCK"
#define WR_TMO_MS WR_S_LOCK_TIMEOUT_MS

int wr_s_lock(struct pp_instance *ppi, void *buf, int len, int new_state)
{
	struct wr_dsport *wrp = WR_DSPOR(ppi);
	int enable = 0;

	if (new_state) {
		wrp->wrStateRetry = WR_STATE_RETRY;
		pp_timeout_set_rename(ppi, wrTmoIdx, WR_TMO_MS*(WR_STATE_RETRY+1),WR_TMO_NAME);
		enable = 1;
	} else {

		int poll_ret = WRH_OPER()->locking_poll(ppi);
		if (poll_ret == WRH_SPLL_READY) {
			wrp->next_state = WRS_LOCKED;
			return 0;
		}

		{ /* Check remaining time */
			int rms=pp_next_delay_1(ppi, wrTmoIdx);
			if ( rms<=(wrp->wrStateRetry*WR_TMO_MS)) {
				WRH_OPER()->locking_disable(ppi);
				if ( rms==0 ) {
					pp_diag(ppi, time, 1, "timeout expired: "WR_TMO_NAME"\n");
					wr_handshake_fail(ppi);
					return 0; /* non-wr already */
				}
				if (wr_handshake_retry(ppi))
					enable = 1;
			}
		}
	}

	if (enable) {
		WRH_OPER()->locking_enable(ppi);
	}

	return 100 ;/* 100ms : We check every 100ms if the PLL is locked */
}
