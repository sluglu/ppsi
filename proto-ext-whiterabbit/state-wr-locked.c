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
#define WR_TMO_NAME "WR_LOCKED"
#define WR_TMO_MS WR_LOCKED_TIMEOUT_MS

int wr_locked(struct pp_instance *ppi, void *buf, int len, int new_state)
{
	int sendmsg = 0;
	MsgSignaling wrsig_msg;
	struct wr_dsport *wrp = WR_DSPOR(ppi);

	if (new_state) {
		wrp->wrStateRetry = WR_STATE_RETRY;
		WRH_SRV(ppi)->readyForSync=TRUE; /* We can start synchronization in the servo */
		pp_timeout_set_rename(ppi, wrTmoIdx, WR_TMO_MS*(WR_STATE_RETRY+1),WR_TMO_NAME);
		sendmsg = 1;
	} else {
		if (ppi->received_ptp_header.messageType == PPM_SIGNALING) {

			msg_unpack_wrsig(ppi, buf, &wrsig_msg,
				 &(wrp->msgTmpWrMessageID));

			if (wrp->msgTmpWrMessageID == CALIBRATE) {
				wrp->next_state = WRS_RESP_CALIB_REQ;
				return 0;
			}
		}

		{ /* Check remaining time */
			int rms=pp_next_delay_1(ppi, wrTmoIdx);
			if ( rms==0 || rms<(wrp->wrStateRetry*WR_TMO_MS)) {
				if (wr_handshake_retry(ppi))
					sendmsg = 1;
				else {
					pp_diag(ppi, time, 1, "timeout expired: "WR_TMO_NAME"\n");
					return 0; /* non-wr already */
				}
			}
		}
	}

	if (sendmsg) {
		msg_issue_wrsig(ppi, LOCKED);
	}

	return pp_next_delay_1(ppi,wrTmoIdx)-wrp->wrStateRetry*WR_TMO_MS;
}
