/*
 * Copyright (C) 2011 CERN (www.cern.ch)
 * Author: Aurelio Colosimo
 * Copyright (C) 2014 GSI (www.gsi.de)
 * Author: Alessandro Rubini
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */
#include <ppsi/ppsi.h>

/* Local functions that build to nothing when Kconfig selects 0/1 vlans */
static int pp_vlan_issue_announce(struct pp_instance *ppi)
{
	int i, vlan = 0;

	if (CONFIG_VLAN_ARRAY_SIZE && ppi->nvlans == 1)
		vlan = ppi->vlans[0];

	if (CONFIG_VLAN_ARRAY_SIZE <= 1 || ppi->nvlans <= 1) {
		ppi->peer_vid = vlan;
		return msg_issue_announce(ppi);
	}

	/*
	 * If Kconfig selected 0/1 vlans, this code is not built.
	 * If we have several vlans, we replace peer_vid and proceed;
	 */
	for (i = 0; i < ppi->nvlans; i++) {
		ppi->peer_vid = ppi->vlans[i];
		msg_issue_announce(ppi);
		/* ignore errors: each vlan is separate */
	}
	return 0;
}

static int pp_vlan_issue_sync_followup(struct pp_instance *ppi)
{
	int i, vlan = 0;

	if (CONFIG_VLAN_ARRAY_SIZE && ppi->nvlans == 1)
		vlan = ppi->vlans[0];

	if (CONFIG_VLAN_ARRAY_SIZE <= 1 || ppi->nvlans <= 1) {
		ppi->peer_vid = vlan;
		return msg_issue_sync_followup(ppi);
	}

	/*
	 * If Kconfig selected 0/1 vlans, this code is not built.
	 * If we have several vlans, we replace peer_vid and proceed;
	 */
	for (i = 0; i < ppi->nvlans; i++) {
		ppi->peer_vid = ppi->vlans[i];
		msg_issue_sync_followup(ppi);
		/* ignore errors: each vlan is separate */
	}
	return 0;
}

/*
 * The following set of functions help the states in the state machine.
 * Ideally, we should manage to get to a completely table-driven fsm
 * implementation based on these helpers
 */

int pp_lib_may_issue_sync(struct pp_instance *ppi)
{
	int e;

	if (!pp_timeout(ppi, PP_TO_SYNC_SEND))
		return 0;

	pp_timeout_set(ppi, PP_TO_SYNC_SEND);
	e = pp_vlan_issue_sync_followup(ppi);
	if (e)
		pp_diag(ppi, frames, 1, "could not send sync\n");
	return e;
}

int pp_lib_may_issue_announce(struct pp_instance *ppi)
{
	int e;

	if (!pp_timeout(ppi, PP_TO_ANN_SEND))
		return 0;

	pp_timeout_set(ppi, PP_TO_ANN_SEND);
	e = pp_vlan_issue_announce(ppi);
	if (e)
		pp_diag(ppi, frames, 1, "could not send announce\n");
	return e;
}

int pp_lib_may_issue_request(struct pp_instance *ppi)
{
	int e = 0;

	if (!pp_timeout(ppi, PP_TO_REQUEST))
		return 0;

	pp_timeout_set(ppi, PP_TO_REQUEST);
	e = msg_issue_request(ppi); /* FIXME: what about multiple vlans? */
	ppi->t3 = ppi->last_snt_time;
	if (e == PP_SEND_ERROR) {
		pp_diag(ppi, frames, 1, "could not send request\n");
		return e;
	}
	return 0;
}


