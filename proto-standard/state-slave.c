/*
 * Copyright (C) 2011 CERN (www.cern.ch)
 * Author: Aurelio Colosimo
 * Copyright (C) 2014 GSI (www.gsi.de)
 * Author: Cesar Prados
 * Originally based on PTPd project v. 2.1.0
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */

#include <ppsi/ppsi.h>
#include "common-fun.h"

static int slave_handle_sync(struct pp_instance *ppi, void *buf, int len);
static int slave_handle_followup(struct pp_instance *ppi, void *buf,
			  int len);
static int slave_handle_response(struct pp_instance *ppi, void *buf,
			  int len);
static int slave_handle_announce(struct pp_instance *ppi, void *buf, int len);

static pp_action *actions[] = {
	[PPM_SYNC]			= slave_handle_sync,
	[PPM_DELAY_REQ]		= 0,
#if CONFIG_HAS_P2P
	[PPM_PDELAY_REQ]	= st_com_peer_handle_preq,
	[PPM_PDELAY_RESP]	= st_com_peer_handle_pres,
	[PPM_PDELAY_R_FUP]	= st_com_peer_handle_pres_followup,
#endif
	[PPM_FOLLOW_UP]		= slave_handle_followup,
	[PPM_DELAY_RESP]	= slave_handle_response,
	[PPM_ANNOUNCE]		= slave_handle_announce,
	[PPM_SIGNALING]     = st_com_handle_signaling,
	/* skip management, for binary size */
};

static int slave_handle_sync(struct pp_instance *ppi, void *buf,
			     int len)
{
	MsgHeader *hdr = &ppi->received_ptp_header;
	MsgSync sync;

	if (!msg_from_current_master(ppi))
		return 0;

	if ( ppi->delayMechanism==E2E &&  ppi->t1.scaled_nsecs==0 && ppi->t1.secs==0 ) {
		/* First time we receive the SYNC message in uncalib/slave state
		 * We set the REQUEST time-out to the minDelayReqInterval/2 value (500ms)
		 * in order to provide quickly a DelayReq message
		 */
		__pp_timeout_set(ppi, PP_TO_REQUEST, (1000*(1<<PP_MIN_MIN_DELAY_REQ_INTERVAL))/2);
	}
	/* t2 may be overriden by follow-up, save it immediately */
	ppi->t2 = ppi->last_rcv_time;
	msg_unpack_sync(buf, &sync);

	if ((hdr->flagField[0] & PP_TWO_STEP_FLAG) != 0) {
		ppi->flags |= PPI_FLAG_WAITING_FOR_F_UP;
		ppi->recv_sync_sequence_id = hdr->sequenceId;
		/* for two-step, the stamp comes later */
		ppi->t1 = hdr->cField; /* most likely 0 */
	} else {
		/* one-step follows */
		ppi->flags &= ~PPI_FLAG_WAITING_FOR_F_UP;
		ppi->t1 = sync.originTimestamp;
		pp_time_add(&ppi->t1, &hdr->cField);
		ppi->syncCF = 0;
		if (CONFIG_HAS_P2P && ppi->delayMechanism == P2P)
			pp_servo_got_psync(ppi);
		else
			pp_servo_got_sync(ppi);
	}
	return 0;
}

static int slave_handle_followup(struct pp_instance *ppi, void *buf,
				 int len)
{
	MsgFollowUp follow;
	int ret = 0;

	MsgHeader *hdr = &ppi->received_ptp_header;

	if (!msg_from_current_master(ppi)) {
		pp_error("%s: Follow up message is not from current parent\n",
			__func__);
		return 0;
	}

	if (!(ppi->flags & PPI_FLAG_WAITING_FOR_F_UP)) {
		pp_error("%s: Slave was not waiting a follow up message\n",
			__func__);
		return 0;
	}

	if (ppi->recv_sync_sequence_id != hdr->sequenceId) {
		pp_error("%s: SequenceID %d doesn't match last Sync message "
			 "%d\n", __func__,
			 hdr->sequenceId, ppi->recv_sync_sequence_id);
		return 0;
	}

	msg_unpack_follow_up(buf, &follow);
	ppi->flags &= ~PPI_FLAG_WAITING_FOR_F_UP;
	/* t1 for calculations is T1 + Csyn + Cful -- see README-cfield */
	pp_time_add(&ppi->t1, &follow.preciseOriginTimestamp);
	pp_time_add(&ppi->t1, &hdr->cField);
	ppi->syncCF = hdr->cField.scaled_nsecs; /* for diag about TC */

	/* Call the extension; it may do it all and ask to return */
	if (ppi->ext_hooks->handle_followup)
		ret = ppi->ext_hooks->handle_followup(ppi, &ppi->t1);
	if (ret == 1)
		return 0;
	if (ret < 0)
		return ret;

	if (CONFIG_HAS_P2P && ppi->delayMechanism == P2P)
		pp_servo_got_psync(ppi);
	else
		pp_servo_got_sync(ppi);

	return 0;
}

static int slave_handle_response(struct pp_instance *ppi, void *buf,
				 int len)
{
	int e = 0;
	MsgHeader *hdr = &ppi->received_ptp_header;
	MsgDelayResp resp;	
	
	msg_unpack_delay_resp(buf, &resp);

	if ((bmc_idcmp(&DSPOR(ppi)->portIdentity.clockIdentity,
		    &resp.requestingPortIdentity.clockIdentity) != 0) ||
	    (ppi->sent_seq[PPM_DELAY_REQ] != hdr->sequenceId)   ||
	    (DSPOR(ppi)->portIdentity.portNumber != resp.requestingPortIdentity.portNumber) ||
	    (!msg_from_current_master(ppi))) {
		pp_diag(ppi, frames, 1, "%s : "
			"Delay Resp doesn't match Delay Req (f %x)\n",__func__,
			ppi->flags);
		return 0;
	}

	ppi->t4 = resp.receiveTimestamp;
	pp_time_add(&ppi->t4, &hdr->cField);
	/* WARNING: should be "sub" (see README-cfield::BUG)  */

	pp_timeout_set(ppi, PP_TO_FAULT);
	if (ppi->ext_hooks->handle_resp)
		e = ppi->ext_hooks->handle_resp(ppi);
	else
		pp_servo_got_resp(ppi);
	if (e)
		return e;

	if (DSPOR(ppi)->logMinDelayReqInterval !=
	    hdr->logMessageInterval) {
		DSPOR(ppi)->logMinDelayReqInterval =
			hdr->logMessageInterval;
		/* new value for logMin */
		pp_timeout_init(ppi);
	}
	return 0;
}

static int slave_handle_announce(struct pp_instance *ppi, void *buf, int len)
{
	int ret = 0;
	struct pp_frgn_master frgn_master;
					  
	ret = st_com_handle_announce(ppi, buf, len);
	if (ret)
		return ret;

	/* If externalPortConfiguration option is set, we consider that all
	 * announce messages come from the current master.
	 */
	if (!DSDEF(ppi)->externalPortConfigurationEnabled && !msg_from_current_master(ppi))
		return 0;
	
	/* 9.2.6.11 a) reset timeout */
	pp_timeout_set(ppi, PP_TO_ANN_RECEIPT);
	/* 9.5.3 Figure 29 update data set if announce from current master */
	bmc_store_frgn_master(ppi, &frgn_master, buf, len);
	bmc_s1(ppi, &frgn_master);
	
	return 0;
}

static int slave_execute(struct pp_instance *ppi)
{
	int ret = 0;

	if (ppi->ext_hooks->execute_slave)
		ret = ppi->ext_hooks->execute_slave(ppi);
	if (ret == 1) /* done: just return */
		return 0;
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * SLAVE and UNCALIBRATED have many things in common. This function implements
 * both states. We set "uncalibrated" internally to 0 or 1.
 */
int pp_slave(struct pp_instance *ppi, void *buf, int len)
{
	int e = 0; /* error var, to check errors in msg handling */
	int uncalibrated = (ppi->state == PPS_UNCALIBRATED);
	MsgHeader *hdr = &ppi->received_ptp_header;

	/* upgrade from uncalibrated to slave or back*/
	if (uncalibrated) {
		if ( ppi->ext_hooks->ready_for_slave != NULL )  {
			if ( (*ppi->ext_hooks->ready_for_slave)(ppi) ) {
				ppi->next_state = PPS_SLAVE;
			}
		} else {
	           ppi->next_state = PPS_SLAVE;
		}
	} else {
		/* TODO add implementation specific SYNCHRONIZATION event */
			if (pp_timeout(ppi, PP_TO_FAULT))
				ppi->next_state = PPS_UNCALIBRATED;
	}
	/* Force to stay on desired state if externalPortConfiguration option is enabled */
	if (DSDEF(ppi)->externalPortConfigurationEnabled )
		ppi->next_state = ppi->externalPortConfigurationPortDS.desiredState;

	/* when entering uncalibrated init servo */
	if (uncalibrated && (ppi->is_new_state)) {
		memset(&ppi->t1, 0, sizeof(ppi->t1));
		pp_diag(ppi, bmc, 2, "Entered to uncalibrated, reset servo\n");	
		pp_servo_init(ppi);

		if (ppi->ext_hooks->new_slave)
			e = ppi->ext_hooks->new_slave(ppi, buf, len);
		if (e)
			goto out;
	}

	/* do a delay measurement either in p2p or e2e delay mode */
	pp_lib_may_issue_request(ppi);
	
	/*
	 * The management of messages is now table-driven
	 */
	if (hdr->messageType < ARRAY_SIZE(actions)
	    && actions[hdr->messageType]) {
		e = actions[hdr->messageType](ppi, buf, len);
	} else {
		if (len)
			pp_diag(ppi, frames, 1, "Ignored frame %i\n",
				hdr->messageType);
	}

	/*
	 * This function, common to uncalibrated and slave,
	 * is the core of the slave: hook
	 */
	e = slave_execute(ppi);

	st_com_check_announce_receive_timeout(ppi);

out:
	switch(e) {
	case PP_SEND_OK: /* 0 */
		break;
	case PP_SEND_ERROR:
		/* ignore: a lost frame is not the end of the world */
		break;
	case PP_SEND_NO_STAMP:
		/* nothing, just keep the ball rolling */
		e = 0;
		break;
	}

	ppi->next_delay = pp_next_delay_2(ppi,
		PP_TO_ANN_RECEIPT, PP_TO_REQUEST);
	return e;
}

