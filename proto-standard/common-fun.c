/*
 * Copyright (C) 2011 CERN (www.cern.ch)
 * Author: Aurelio Colosimo
 * Based on PTPd project v. 2.1.0 (see AUTHORS for details)
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */
#include <ppsi/ppsi.h>
#include "common-fun.h"
#include "../lib/network_types.h"

void *msg_copy_header(MsgHeader *dest, MsgHeader *src)
{
	return memcpy(dest, src, sizeof(MsgHeader));
}

static void *__align_pointer(void *p)
{
	unsigned long ip, align = 0;

	ip = (unsigned long)p;
	if (ip & 3)
		align = 4 - (ip & 3);
	return p + align;
}

void pp_prepare_pointers(struct pp_instance *ppi)
{
	/*
	 * Horrible thing: when we receive vlan, we get standard eth header,
	 * but when we send we must fill the complete vlan header.
	 * So we reserve a different number of bytes.
	 */
	switch(ppi->proto) {
	case PPSI_PROTO_RAW:
		ppi->tx_offset = ETH_HLEN; /* 14, I know! */
		ppi->rx_offset = ETH_HLEN;
	#ifdef CONFIG_ARCH_WRPC
		ppi->tx_offset = 0; /* Currently, wrpc has a separate header */
		ppi->rx_offset = 0;
	#endif
		break;
	case PPSI_PROTO_VLAN:
		ppi->tx_offset = sizeof(struct pp_vlanhdr);
		ppi->rx_offset = ETH_HLEN;
		break;
	case PPSI_PROTO_UDP:
		ppi->tx_offset = 0;
		ppi->rx_offset = 0;
		break;
	}
	ppi->tx_ptp = __align_pointer(ppi->__tx_buffer + ppi->tx_offset);
	ppi->rx_ptp = __align_pointer(ppi->__rx_buffer + ppi->rx_offset);

	/* Now that ptp payload is aligned, get back the header */
	ppi->tx_frame = ppi->tx_ptp - ppi->tx_offset;
	ppi->rx_frame = ppi->rx_ptp - ppi->rx_offset;

	if (0) { /* enable to verify... it works for me though */
		pp_printf("%p -> %p %p\n",
			  ppi->__tx_buffer, ppi->tx_frame, ppi->tx_ptp);
		pp_printf("%p -> %p %p\n",
			  ppi->__rx_buffer, ppi->rx_frame, ppi->rx_ptp);
	}
}

static int presp_call_servo(struct pp_instance *ppi)
{
	int ret = 0;

	if (is_incorrect(&ppi->t4))
		return 0; /* not an error, just no data */

	pp_timeout_set(ppi, PP_TO_FAULT);
	if (pp_hooks.handle_presp)
		ret = pp_hooks.handle_presp(ppi);
	else
		pp_servo_got_presp(ppi);

	return ret;
}

int st_com_check_announce_receive_timeout(struct pp_instance *ppi)
{
	struct pp_globals *ppg = GLBS(ppi);
	int is_gm = 1;
	int i;
	
	if (pp_timeout(ppi, PP_TO_ANN_RECEIPT)) {
		/* 9.2.6.11 b) reset timeout when an announce timeout happended */
		pp_timeout_set(ppi, PP_TO_ANN_RECEIPT);
		if (DSDEF(ppi)->clockQuality.clockClass != PP_CLASS_SLAVE_ONLY
		    && (ppi->role != PPSI_ROLE_SLAVE)) {
			if (!CODEOPT_ONE_PORT() &&  DSDEF(ppi)->numberPorts > 1) {
				for (i = 0; i < ppg->defaultDS->numberPorts; i++) {
					if ((INST(ppg, i)->state == PPS_UNCALIBRATED) ||
						(INST(ppg, i)->state == PPS_SLAVE))
						is_gm = 0;
				}				
				if (is_gm)
					bmc_m1(ppi);
				else
					bmc_m3(ppi);				
			} else
				bmc_m1(ppi);	
			
			ppi->next_state = PPS_MASTER;
		} else {
			ppi->next_state = PPS_LISTENING;
		}
	}
	return 0;
}


int st_com_peer_handle_pres(struct pp_instance *ppi, void *buf,
			    int len)
{
	MsgPDelayResp resp;
	MsgHeader *hdr = &ppi->received_ptp_header;
	int e = 0;

	/* if not in P2P mode, just return */
	if (!CONFIG_HAS_P2P || ppi->mech != PP_P2P_MECH)
		return 0;
	
	msg_unpack_pdelay_resp(buf, &resp);

	if ((bmc_idcmp(&DSPOR(ppi)->portIdentity.clockIdentity,
		    &resp.requestingPortIdentity.clockIdentity) == 0) &&
	    ((ppi->sent_seq[PPM_PDELAY_REQ]) ==
	     hdr->sequenceId) &&
	    (DSPOR(ppi)->portIdentity.portNumber ==
	     resp.requestingPortIdentity.portNumber) &&
	    (msg_from_current_master(ppi))) {

		ppi->t4 = resp.requestReceiptTimestamp;
		pp_time_add(&ppi->t4, &hdr->cField);
		/* WARNING: should be "sub" (see README-cfield::BUG)  */
		ppi->t6 = ppi->last_rcv_time;
		if ((hdr->flagField[0] & PP_TWO_STEP_FLAG) != 0)
			ppi->flags |= PPI_FLAG_WAITING_FOR_RF_UP;
		else {
			ppi->flags &= ~PPI_FLAG_WAITING_FOR_RF_UP;
			/*
			 * Make sure responseOriginTimestamp is forced to 0
			 * for one-step responders
			 */
			memset(&ppi->t5, 0, sizeof(ppi->t5));
		}

		if (!(hdr->flagField[0] & PP_TWO_STEP_FLAG))
			e = presp_call_servo(ppi);
	} else {
		pp_diag(ppi, frames, 2, "pp_pclock : "
			"PDelay Resp doesn't match PDelay Req\n");
	}
	return e;
}

int st_com_peer_handle_pres_followup(struct pp_instance *ppi,
				     void *buf, int len)
{
	MsgHeader *hdr = &ppi->received_ptp_header;
	MsgPDelayRespFollowUp respFllw;
	int e = 0;

	/* if not in P2P mode, just return */
	if (!CONFIG_HAS_P2P || ppi->mech != PP_P2P_MECH)
		return 0;
	
	msg_unpack_pdelay_resp_follow_up(buf, &respFllw);

	if ((bmc_idcmp(&DSPOR(ppi)->portIdentity.clockIdentity,
		    &respFllw.requestingPortIdentity.clockIdentity) == 0) &&
	    ((ppi->sent_seq[PPM_PDELAY_REQ]) ==
	     hdr->sequenceId) &&
	    (DSPOR(ppi)->portIdentity.portNumber ==
	     respFllw.requestingPortIdentity.portNumber) &&
	    (msg_from_current_master(ppi))) {

		ppi->t5 = respFllw.responseOriginTimestamp;
		pp_time_add(&ppi->t5, &hdr->cField);

		e = presp_call_servo(ppi);
	} else {
		pp_diag(ppi, frames, 2, "%s: "
			"PDelay Resp F-up doesn't match PDelay Req\n",
			__func__);
	}
	return e;
}

int st_com_peer_handle_preq(struct pp_instance *ppi, void *buf,
			    int len)
{
	int e = 0;

	/* if not in P2P mode, just return */
	if (!CONFIG_HAS_P2P || ppi->mech != PP_P2P_MECH)
		return 0;
	
	if (pp_hooks.handle_preq)
		e = pp_hooks.handle_preq(ppi);
	if (e)
		return e;

	msg_issue_pdelay_resp(ppi, &ppi->last_rcv_time);
	msg_issue_pdelay_resp_followup(ppi, &ppi->last_snt_time);

	return 0;
}

int st_com_handle_announce(struct pp_instance *ppi, void *buf, int len)
{
	bmc_add_frgn_master(ppi, buf, len);

	if (pp_hooks.handle_announce)
		return pp_hooks.handle_announce(ppi);
	return 0;
}

int __send_and_log(struct pp_instance *ppi, int msglen, int chtype)
{
	int msgtype = ((char *)ppi->tx_ptp)[0] & 0xf;
	struct pp_time *t = &ppi->last_snt_time;
	int ret;

	ret = ppi->n_ops->send(ppi, ppi->tx_frame, msglen + ppi->tx_offset,
			       msgtype);
	if (ret == PP_SEND_DROP)
		return 0; /* don't report as error, nor count nor log as sent */
	if (ret < msglen) {
		pp_diag(ppi, frames, 1, "%s(%d) Message can't be sent\n",
			pp_msgtype_info[msgtype].name, msgtype);
		return PP_SEND_ERROR;
	}
	/* FIXME: diagnosticst should be looped back in the send method */
	pp_diag(ppi, frames, 1, "SENT %02d bytes at %d.%09d.%03d (%s)\n",
		msglen, (int)t->secs, (int)(t->scaled_nsecs >> 16),
		((int)(t->scaled_nsecs & 0xffff) * 1000) >> 16,
		pp_msgtype_info[msgtype].name);
	if (chtype == PP_NP_EVT && is_incorrect(&ppi->last_snt_time))
		return PP_SEND_NO_STAMP;

	/* count sent packets */
	ppi->ptp_tx_count++;

	return 0;
}
