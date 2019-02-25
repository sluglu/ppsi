/*
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Aurelio Colosimo
 * Based on ptp-noposix project (see AUTHORS for details)
 *
 * Released to the public domain
 */

#include <ppsi/ppsi.h>

int wr_resp_calib_req(struct pp_instance *ppi, void *buf, int len)
{
	struct wr_dsport *wrp = WR_DSPOR(ppi);
	MsgSignaling wrsig_msg;
	int e = 0, enable = 0;
	int send_pattern = (wrp->otherNodeCalSendPattern != 0);

	if (ppi->is_new_state) {
		wrp->wrStateRetry = WR_STATE_RETRY;
		pp_timeout_set_rename(ppi, wrTmoIdx,WR_RESP_CALIB_REQ_TIMEOUT_MS*(WR_STATE_RETRY+1),"WR_CALIBREQ");
		enable = 1;
	} else {
		int rms=pp_next_delay_1(ppi, wrTmoIdx);
		if ( rms==0 || rms<(wrp->wrStateRetry*WR_RESP_CALIB_REQ_TIMEOUT_MS)) {
			if (send_pattern)
				WRH_OPER()->calib_pattern_disable(ppi);
			if (wr_handshake_retry(ppi))
				enable = 1;
			else
				return 0; /* non-wr already */
		}
	}

	if (enable) { /* first or retry */
		if (send_pattern)
			WRH_OPER()->calib_pattern_enable(ppi, 0, 0, 0);
	}

	if (ppi->received_ptp_header.messageType == PPM_SIGNALING) {

		msg_unpack_wrsig(ppi, buf, &wrsig_msg,
			 &(wrp->msgTmpWrMessageID));

		if (wrp->msgTmpWrMessageID == CALIBRATED) {
			if (send_pattern)
				WRH_OPER()->calib_pattern_disable(ppi);
			if (wrp->wrMode == WR_MASTER)
				ppi->next_state = WRS_WR_LINK_ON;
			else
				ppi->next_state = WRS_CALIBRATION;
		}
	}

	ppi->next_delay = pp_next_delay_1(ppi,wrTmoIdx)-wrp->wrStateRetry*WR_RESP_CALIB_REQ_TIMEOUT_MS;
	return e;
}
