/*
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Aurelio Colosimo
 * Based on ptp-noposix project (see AUTHORS for details)
 *
 * Released to the public domain
 */

#include <ppsi/ppsi.h>

#define WR_TMO_NAME "WR_CALIBREQ"
#define WR_TMO_MS WR_RESP_CALIB_REQ_TIMEOUT_MS

int wr_resp_calib_req(struct pp_instance *ppi, void *buf, int len, int new_state)
{
	struct wr_dsport *wrp = WR_DSPOR(ppi);

	if (new_state) {
		wrp->wrStateRetry = WR_STATE_RETRY;
		pp_timeout_set_rename(ppi, wrTmoIdx,WR_TMO_MS*(WR_STATE_RETRY+1),WR_TMO_NAME);
	} else {

		if (ppi->received_ptp_header.messageType == PPM_SIGNALING) {
			Enumeration16 wrMsgId;
			MsgSignaling wrsig_msg;

			if ( msg_unpack_wrsig(ppi, buf, &wrsig_msg, &wrMsgId) ) {
				if ( wrMsgId == CALIBRATED) {
				/* Update servo */
				wr_servo_ext_t *se =WRE_SRV(ppi);

				fixedDelta_to_pp_time(wrp->otherNodeDeltaTx,&se->delta_txm);
				fixedDelta_to_pp_time(wrp->otherNodeDeltaRx,&se->delta_rxm);

				wrp->next_state = (wrp->wrMode == WR_MASTER) ?
						WRS_WR_LINK_ON :
						WRS_CALIBRATION;
				} else
					wrp->next_state = WRS_IDLE;
				return 0;
			}
		}

		{ /* Check remaining time */
			int rms=pp_next_delay_1(ppi, wrTmoIdx);
			if ( rms==0 || rms<(wrp->wrStateRetry*WR_TMO_MS)) {
				if (!wr_handshake_retry(ppi)) {
					pp_diag(ppi, time, 1, "timeout expired: "WR_TMO_NAME"\n");
					return 0; /* non-wr already */
				}
			}
		}
	}

	return pp_next_delay_1(ppi,wrTmoIdx)-wrp->wrStateRetry*WR_TMO_MS;
}
