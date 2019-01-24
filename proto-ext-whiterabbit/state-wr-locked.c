/*
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Aurelio Colosimo
 * Based on ptp-noposix project (see AUTHORS for details)
 *
 * Released to the public domain
 */

#include <ppsi/ppsi.h>

/* 
 * WR slave: got here from WRS_S_LOCK: send LOCKED, wait for CALIBRATE.
 * On timeout resend.
 */
int wr_locked(struct pp_instance *ppi, void *buf, int len)
{
	int e = 0, sendmsg = 0;
	MsgSignaling wrsig_msg;
	struct wr_dsport *wrp = WR_DSPOR(ppi);

	if (ppi->is_new_state) {
		wrp->wrStateRetry = WR_STATE_RETRY;
		sendmsg = 1;
	} else if (pp_timeout(ppi, PP_TO_EXT_0)) {
		if (wr_handshake_retry(ppi))
			sendmsg = 1;
		else
			return 0; /* non-wr already */
	}

	if (sendmsg) {
		__pp_timeout_set(ppi, PP_TO_EXT_0, WR_LOCKED_TIMEOUT_MS);
		e = msg_issue_wrsig(ppi, LOCKED);
	}

	if (ppi->received_ptp_header.messageType == PPM_SIGNALING) {

		msg_unpack_wrsig(ppi, buf, &wrsig_msg,
			 &(wrp->msgTmpWrMessageID));

		if (wrp->msgTmpWrMessageID == CALIBRATE)
			ppi->next_state = WRS_RESP_CALIB_REQ;
	}

	ppi->next_delay = wrp->wrStateTimeout;

	return e;
}
