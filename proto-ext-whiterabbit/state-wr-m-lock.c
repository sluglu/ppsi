/*
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Aurelio Colosimo
 * Based on ptp-noposix project (see AUTHORS for details)
 *
 * Released to the public domain
 */

#include <ppsi/ppsi.h>

/*
 * This the entry point for a WR master: send "LOCK" and wait
 * for "LOCKED". On timeout retry sending, for WR_STATE_RETRY times.
 */
int wr_m_lock(struct pp_instance *ppi, void *buf, int len)
{
	int e = 0, sendmsg = 0;
	MsgSignaling wrsig_msg;
	struct wr_dsport *wrp = WR_DSPOR(ppi);

	if (ppi->is_new_state) {
		wrp->wrStateRetry = WR_STATE_RETRY;
		__pp_timeout_set(ppi, PP_TO_EXT_0, WR_M_LOCK_TIMEOUT_MS*(WR_STATE_RETRY+1));
		sendmsg = 1;
	} else {
		int rms=pp_next_delay_1(ppi, PP_TO_EXT_0);
		if ( rms==0 || rms<(wrp->wrStateRetry*WR_M_LOCK_TIMEOUT_MS)) {
			if (wr_handshake_retry(ppi))
				sendmsg = 1;
			else
				return 0; /* non-wr already */
		}
	}

	if (sendmsg) {
		e = msg_issue_wrsig(ppi, LOCK);
	}

	if (ppi->received_ptp_header.messageType == PPM_SIGNALING) {
		msg_unpack_wrsig(ppi, buf, &wrsig_msg,
			 &(wrp->msgTmpWrMessageID));

		if (wrp->msgTmpWrMessageID == LOCKED)
			ppi->next_state = WRS_CALIBRATION;
	}

	
	ppi->next_delay = pp_next_delay_1(ppi,PP_TO_EXT_0)-wrp->wrStateRetry*WR_M_LOCK_TIMEOUT_MS;

	return e;
}
