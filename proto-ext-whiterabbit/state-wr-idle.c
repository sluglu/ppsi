/*
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Aurelio Colosimo
 * Based on ptp-noposix project (see AUTHORS for details)
 *
 * Released to the public domain
 */

#include <ppsi/ppsi.h>

/*
 * WRS_IDLE is the entry point for the IDLE state
 */
int wr_idle(struct pp_instance *ppi, void *buf, int len, int new_state)
{
	int delay=1000;// default delay
	struct wr_dsport *wrp = WR_DSPOR(ppi);

	if (  ppi->state==PPS_MASTER ) {
		/* Check the reception of a slave present message.
		 * If it arrives we must restart the WR handshake
		 */

		if (ppi->received_ptp_header.messageType == PPM_SIGNALING) {
			Enumeration16 wrMsgId;
			MsgSignaling wrsig_msg;

			if ( msg_unpack_wrsig(ppi, buf, &wrsig_msg,	&wrMsgId) ) {
				if ( wrMsgId == SLAVE_PRESENT) {
					// Start handshake
					wrp->next_state = WRS_M_LOCK;
					delay=0;
				}
				pdstate_enable_extension(ppi); // Enable extension in all cases
			}
		}
	} else {
		if ( ppi->ext_enabled && ppi->state==PPS_SLAVE ) {
			if ( !(wrp->wrModeOn && wrp->parentWrModeOn) ) {
				/* Failure detected in the protocol */
				ppi->next_state=PPS_UNCALIBRATED;
				wr_reset_process(ppi,WR_ROLE_NONE);
			}
		}
	}

	return delay;
}
