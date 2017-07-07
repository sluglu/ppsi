/*
 * Copyright (C) 2011 CERN (www.cern.ch)
 * Author: Aurelio Colosimo
 * Based on PTPd project v. 2.1.0 (see AUTHORS for details)
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */

#include <ppsi/ppsi.h>
#include "common-fun.h"

static pp_action *actions[] = {
	[PPM_SYNC]			= 0,
	[PPM_DELAY_REQ]		= 0,
#if CONFIG_HAS_P2P
	[PPM_PDELAY_REQ]	= st_com_peer_handle_preq,
	[PPM_PDELAY_RESP]	= st_com_peer_handle_pres,
	[PPM_PDELAY_R_FUP]	= st_com_peer_handle_pres_followup,
#endif
	[PPM_FOLLOW_UP]		= 0,
	[PPM_DELAY_RESP]	= 0,
	[PPM_ANNOUNCE]		= pp_lib_handle_announce,
	/* skip signaling and management, for binary size */
};

int pp_listening(struct pp_instance *ppi, unsigned char *pkt, int plen)
{
	int e = 0; /* error var, to check errors in msg handling */
	MsgHeader *hdr = &ppi->received_ptp_header;

	pp_timeout_set(ppi, PP_TO_FAULT); /* no fault as long as we listen */
	if (pp_hooks.listening)
		e = pp_hooks.listening(ppi, pkt, plen);
	if (e)
		goto out;

	/* when the clock is using peer-delay, listening must send it too */
	if (CONFIG_HAS_P2P && ppi->mech == PP_P2P_MECH)
		e  = pp_lib_may_issue_request(ppi);
	/*
	 * The management of messages is now table-driven
	 */
	if (hdr->messageType < ARRAY_SIZE(actions)
	    && actions[hdr->messageType]) {
		e = actions[hdr->messageType](ppi, pkt, plen);
	} else {
		if (plen)
			pp_diag(ppi, frames, 1, "Ignored frame %i\n",
				hdr->messageType);
	}

	st_com_check_announce_receive_timeout(ppi);

	if (pp_timeout(ppi, PP_TO_FAULT))
		ppi->next_state = PPS_FAULTY;

out:
	if (e != 0)
		ppi->next_state = PPS_FAULTY;

	ppi->next_delay = pp_next_delay_1(ppi, PP_TO_ANN_RECEIPT);

	return e;
}
