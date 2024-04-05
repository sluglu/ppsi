/*
 * Copyright (C) 2014 CERN (www.cern.ch)
 * Author: Alessandro Rubini
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */
#include <ppsi/ppsi.h>

void wr_reset_process(struct pp_instance *ppi, wr_role_t role) {
	struct wr_dsport *wrp = WR_DSPOR(ppi);

	wrp->wrStateTimeout = WR_DEFAULT_STATE_TIMEOUT_MS;
	wrp->calPeriod = WR_DEFAULT_CAL_PERIOD;
	wrp->wrMode = role;
	wrp->wrModeOn=FALSE;
	wrp->calibrated = !WR_DEFAULT_PHY_CALIBRATION_REQUIRED;
	if ( role != WR_SLAVE ) {
		/* Reset parent data */
		/* For a SLAVE, these info are updated when an Announce message is received */
		wrp->parentWrConfig = NON_WR;
		wrp->parentIsWRnode =
				wrp->parentWrModeOn =
						wrp->parentCalibrated = FALSE;
	}
}

/* The handshake failed: go master or slave in normal PTP mode */
void wr_handshake_fail(struct pp_instance *ppi)
{
	struct wr_dsport *wrp = WR_DSPOR(ppi);

	wrp->next_state = WRS_IDLE;

	/* Don't use PTP without WR, try WR one more time */
	if (!ppi->ptp_fallback) {
		wrh_servo_t *s = WRH_SRV(ppi);
		if (s)
			s->doRestart = TRUE;
		pp_diag(ppi, ext, 1, "Handshake failure: PTP fallback disabled."
			" Try WR again as %s\n",
			wrp->wrMode == WR_MASTER ? "master" : "slave");
		return;
	}

	pp_diag(ppi, ext, 1, "Handshake failure: now non-wr %s\n",
		wrp->wrMode == WR_MASTER ? "master" : "slave");

	wr_reset_process(ppi,WR_ROLE_NONE);
	wr_servo_reset(ppi);
	pdstate_disable_extension(ppi);
}


/* One of the steps failed: either retry or fail */
int wr_handshake_retry(struct pp_instance *ppi)
{
	struct wr_dsport *wrp = WR_DSPOR(ppi);

	if (wrp->wrStateRetry > 0) {
		wrp->wrStateRetry--;
		pp_diag(ppi, ext, 1, "Retry on timeout\n");
		return 1; /* yes, retry */
	}
	return 0; /* don't retry, we are over already */
}
