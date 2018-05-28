/*
 * Copyright (C) 2011 CERN (www.cern.ch)
 * Author: Aurelio Colosimo
 * Based on PTPd project v. 2.1.0 (see AUTHORS for details)
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */

#include <ppsi/ppsi.h>
#include "common-fun.h"

static int master_handle_delay_request(struct pp_instance *ppi,
				       void *buf, int len);

static pp_action *actions[] = {
	[PPM_SYNC]		= 0,
	[PPM_DELAY_REQ]		= master_handle_delay_request,
#if CONFIG_HAS_P2P
	[PPM_PDELAY_REQ]	= st_com_peer_handle_preq,
	[PPM_PDELAY_RESP]	= st_com_peer_handle_pres,
	[PPM_PDELAY_R_FUP]	= st_com_peer_handle_pres_followup,
#endif
	[PPM_FOLLOW_UP]		= 0,
	[PPM_DELAY_RESP]	= 0,
	[PPM_ANNOUNCE]		= st_com_handle_announce,
	[PPM_SIGNALING]		= st_com_handle_signaling,
	/* skip management, for binary size */
};

static int master_handle_delay_request(struct pp_instance *ppi,
				       void *buf, int len)
{
	if (ppi->state == PPS_MASTER) /* not pre-master */
		msg_issue_delay_resp(ppi, &ppi->last_rcv_time);
	return 0;
}

/*
 * MASTER and PRE_MASTER have many things in common. This function implements
 * both states. We set "pre" internally to 0 or 1.
 */
int pp_master(struct pp_instance *ppi, void *buf, int len)
{
	int msgtype;
	int pre = (ppi->state == PPS_PRE_MASTER);
	int e = 0; /* error var, to check errors in msg handling */

	pp_timeout_set(ppi, PP_TO_FAULT); /* no fault as long as we are
					   * master */

	/* upgrade from pre-master to master */
	if (pre && pp_timeout(ppi, PP_TO_QUALIFICATION)) {
		ppi->next_state = PPS_MASTER;
		/* start sending imediately and reenter */
		pp_timeout_clear(ppi, PP_TO_SYNC_SEND);
		pp_timeout_clear(ppi, PP_TO_ANN_SEND);
		ppi->next_delay = 0;
		return 0;
	}

	if (!pre) {
		/*
		 * ignore errors; we are not getting FAULTY if not
		 * transmitting
		 */
		pp_lib_may_issue_sync(ppi);
		pp_lib_may_issue_announce(ppi);
	}

	/* when the clock is using peer-delay, the master must send it too */
	if (CONFIG_HAS_P2P && ppi->delayMechanism == P2P)
		pp_lib_may_issue_request(ppi);

	/*
	 * An extension can do special treatment of this message type,
	 * possibly returning error or eating the message by returning
	 * PPM_NO_MESSAGE
	 */
	msgtype = ppi->received_ptp_header.messageType;
	if (ppi->ext_hooks->master_msg)
		msgtype = ppi->ext_hooks->master_msg(ppi, buf, len, msgtype);
	if (msgtype < 0) {
		e = msgtype;
		len = 0;
		e = PP_SEND_ERROR; /* well, "error" in general */
		goto out;
	}

	/*
	 * The management of messages is now table-driven
	 */
	if (msgtype < ARRAY_SIZE(actions)
	    && actions[msgtype]) {
		e = actions[msgtype](ppi, buf, len);
	} else {
		if (len && msgtype != PPM_NO_MESSAGE)
			pp_diag(ppi, frames, 1, "Ignored frame %i\n",
				msgtype);
	}

	if (pp_timeout(ppi, PP_TO_FAULT))
		ppi->next_state = PPS_FAULTY;

out:
	switch(e) {
	case PP_SEND_OK: /* 0 */
		/* Why should we switch to slave? Remove this code? */
		if (DSDEF(ppi)->clockQuality.clockClass == PP_CLASS_SLAVE_ONLY
		    || (ppi->role == PPSI_ROLE_SLAVE))
			ppi->next_state = PPS_LISTENING;
		break;
	case PP_SEND_ERROR:
		/* fall through: a lost frame is not the end of the world */
	case PP_SEND_NO_STAMP:
		/* nothing, just keep the ball rolling */
		e = 0;
		break;
	}

	if (pre) {
		if (CONFIG_HAS_P2P && ppi->delayMechanism == PP_P2P_MECH) {
			ppi->next_delay = pp_next_delay_2(ppi,
				PP_TO_QUALIFICATION, PP_TO_REQUEST);
		} else {
			ppi->next_delay = pp_next_delay_1(ppi,
				PP_TO_QUALIFICATION);			
		}		
	} else {
		if (CONFIG_HAS_P2P && ppi->delayMechanism == PP_P2P_MECH) {
			ppi->next_delay = pp_next_delay_3(ppi,
				PP_TO_ANN_SEND, PP_TO_SYNC_SEND, PP_TO_REQUEST);
		} else {
			ppi->next_delay = pp_next_delay_2(ppi,
				PP_TO_ANN_SEND, PP_TO_SYNC_SEND);			
		}		
	}
	
	return e;
}

