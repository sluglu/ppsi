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

/* Called by this file, basically when an announce is got, all states */
static void __lib_add_foreign(struct pp_instance *ppi, unsigned char *buf)
{
	int i, worst, sel;
	struct pp_frgn_master frgn_master;
	MsgHeader *hdr = &ppi->received_ptp_header;

	/* if we are a configured master don't add*/
	if (ppi->role == PPSI_ROLE_MASTER)
		return;
	/*
	 * header and announce field of each Foreign Master are
	 * useful to run Best Master Clock Algorithm
	 */
	msg_copy_header(&frgn_master.hdr, hdr);
	msg_unpack_announce(buf, &frgn_master.ann);

	/* Copy new foreign master data set from announce message */
	memcpy(&frgn_master.port_id,
	       &hdr->sourcePortIdentity, sizeof(hdr->sourcePortIdentity));

	/* Copy the source port identity */
	memcpy(&frgn_master.source_id,
	       &DSPOR(ppi)->portIdentity, sizeof(DSPOR(ppi)->portIdentity));

	/* Check if announce from a port from this clock 9.3.2.5 a) */
	if (!memcmp(&hdr->sourcePortIdentity.clockIdentity,
		    &DSDEF(ppi)->clockIdentity,
		    sizeof(DSDEF(ppi)->clockIdentity)))
		return;

	/* Check if announce has steps removed larger than 255 9.3.2.5 d) */
	if (frgn_master.ann.stepsRemoved >= 255)
		return;

	/* Check if foreign master is already known */
	for (i = 0; i < ppi->frgn_rec_num; i++) {
		if (!memcmp(&hdr->sourcePortIdentity,
			    &ppi->frgn_master[i].port_id,
			    sizeof(hdr->sourcePortIdentity))) {

			pp_diag(ppi, bmc, 2, "Foreign Master %i updated\n", i);

			/* update the number of announce received if correct
			 * sequence number 9.3.2.5 b) */
			if (hdr->sequenceId
				  == (ppi->frgn_master[i].hdr.sequenceId + 1))
				ppi->frgn_master[i].ann_cnt[0]++;

			/* already in Foreign master data set, update info */
			msg_copy_header(&ppi->frgn_master[i].hdr, hdr);
			msg_unpack_announce(buf, &ppi->frgn_master[i].ann);
			return;
		}
	}

	/* set qualification timeouts as valid to compare against worst*/
	for (i = 0; i < PP_FOREIGN_MASTER_TIME_WINDOW; i++)
		frgn_master.ann_cnt[i] = 1;

	/* New foreign master */
	if (ppi->frgn_rec_num < PP_NR_FOREIGN_RECORDS) {
		/* there is space for a new one */
		sel = ppi->frgn_rec_num;
		ppi->frgn_rec_num++;

	} else {
		/* find the worst to replace */
		for (i = 1, worst = 0; i < ppi->frgn_rec_num; i++)
			if (bmc_dataset_cmp(ppi, &ppi->frgn_master[i],
					    &ppi->frgn_master[worst]) > 0)
				worst = i;

		/* check if worst is better than the new one, and skip the new
		 * one if so */
		if (bmc_dataset_cmp(ppi, &ppi->frgn_master[worst], &frgn_master)
			< 0) {
				pp_diag(ppi, bmc, 1, "%s:%i: New foreign "
					"master worse than worst in the full "
					"table, skipping\n",
			__func__, __LINE__);
			return;
		}

		sel = worst;
	}

	/* clear qualification timeouts */
	for (i = 0; i < PP_FOREIGN_MASTER_TIME_WINDOW; i++)
		frgn_master.ann_cnt[i] = 0;

	/* This is the first one qualified 9.3.2.5 e)*/
	frgn_master.ann_cnt[0] = 1;

	/* Copy the temporary foreign master entry */
	memcpy(&ppi->frgn_master[sel],
	       &frgn_master, sizeof(frgn_master));

	pp_diag(ppi, bmc, 1, "New foreign Master %i added\n", sel);
}

int pp_lib_handle_announce(struct pp_instance *ppi, unsigned char *buf, int len)
{
	__lib_add_foreign(ppi, buf);

	pp_timeout_set(ppi, PP_TO_ANN_RECEIPT);

	if (pp_hooks.handle_announce)
		return pp_hooks.handle_announce(ppi);
	return 0;
}
