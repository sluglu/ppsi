/*
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Aurelio Colosimo
 * Based on ptp-noposix project (see AUTHORS for details)
 *
 * Released to the public domain
 */

#include <ppsi/ppsi.h>

/*
 * WRS_PRESENT is the entry point for a WR slave
 *
 * Here we send SLAVE_PRESENT and wait for LOCK. If timeout,
 * re-send SLAVE_PRESENT from WR_STATE_RETRY times
 */
int wr_idle(struct pp_instance *ppi, void *buf, int len, int new_state)
{
	int delay=1000;//INT_MAX;

	if ( ppi->ext_enabled ) {
		/* While the extension is enabled, we expect to have
		 * a working WR connection.
		 */
		struct wr_dsport *wrp = WR_DSPOR(ppi);

		switch (ppi->state) {
		case PPS_MASTER :
		{

			/* Check the reception of a slave present message.
			 * If it arrives we must restart the WR handshake
			 */
			MsgSignaling wrsig_msg;

			if (ppi->received_ptp_header.messageType == PPM_SIGNALING) {
				msg_unpack_wrsig(ppi, buf, &wrsig_msg,
					 &(wrp->msgTmpWrMessageID));

				if (wrp->msgTmpWrMessageID == SLAVE_PRESENT) {
					lstate_set_link_in_progress(ppi);
					wrp->next_state = WRS_M_LOCK;
					delay=0;
				}
			}
			if ( wrp->wrModeOn && wrp->parentWrModeOn )
				lstate_set_link_established(ppi);
			break;
		}
		case PPS_SLAVE :
			if ( !(wrp->wrModeOn && wrp->parentWrModeOn) ) {
				/* Failure detected in the protocol */
				ppi->next_state=PPS_UNCALIBRATED;
				wr_reset_process(ppi,WR_ROLE_NONE);
				lstate_set_link_pdetection(ppi);
			}
			break;
		case PPS_UNCALIBRATED :
			break;
		}
	}
	return delay;
}
