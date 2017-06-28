/*
 * Copyright (C) 2011 CERN (www.cern.ch)
 * Author: Aurelio Colosimo
 * Based on PTPd project v. 2.1.0 (see AUTHORS for details)
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */

#include <ppsi/ppsi.h>

/* Flag Field bits symbolic names (table 57, pag. 151) */
#define FFB_LI61	0x01
#define FFB_LI59	0x02
#define FFB_UTCV	0x04
#define FFB_PTP		0x08
#define FFB_TTRA	0x10
#define FFB_FTRA	0x20

/* ppi->port_idx port is becoming Master. Table 13 (9.3.5) of the spec. */
void bmc_m1(struct pp_instance *ppi)
{
	struct DSParent *parent = DSPAR(ppi);
	struct DSDefault *defds = DSDEF(ppi);

	/* Current data set update */
	DSCUR(ppi)->stepsRemoved = 0;
	clear_time(&DSCUR(ppi)->offsetFromMaster);
	clear_time(&DSCUR(ppi)->meanPathDelay);

	/* Parent data set: we are the parent */
	memset(parent, 0, sizeof(*parent));
	parent->parentPortIdentity.clockIdentity = defds->clockIdentity;
	parent->parentPortIdentity.portNumber = 0;

	/* Copy grandmaster params from our defds (FIXME: is ir right?) */
	parent->grandmasterIdentity = defds->clockIdentity;
	parent->grandmasterClockQuality = defds->clockQuality;
	parent->grandmasterPriority1 = defds->priority1;
	parent->grandmasterPriority2 = defds->priority2;

	/* Time Properties data set */
	DSPRO(ppi)->ptpTimescale = TRUE;
	DSPRO(ppi)->timeSource = INTERNAL_OSCILLATOR;
}

/* ppi->port_idx port is becoming Master. Table 13 (9.3.5) of the spec. */
void bmc_m2(struct pp_instance *ppi)
{
	struct DSParent *parent = DSPAR(ppi);
	struct DSDefault *defds = DSDEF(ppi);

	/* Current data set update */
	DSCUR(ppi)->stepsRemoved = 0;
	clear_time(&DSCUR(ppi)->offsetFromMaster);
	clear_time(&DSCUR(ppi)->meanPathDelay);

	/* Parent data set: we are the parent */
	memset(parent, 0, sizeof(*parent));
	parent->parentPortIdentity.clockIdentity = defds->clockIdentity;
	parent->parentPortIdentity.portNumber = 0;

	/* Copy grandmaster params from our defds (FIXME: is ir right?) */
	parent->grandmasterIdentity = defds->clockIdentity;
	parent->grandmasterClockQuality = defds->clockQuality;
	parent->grandmasterPriority1 = defds->priority1;
	parent->grandmasterPriority2 = defds->priority2;

	/* Time Properties data set */
	DSPRO(ppi)->ptpTimescale = TRUE;
	DSPRO(ppi)->timeSource = INTERNAL_OSCILLATOR;
}

/* ppi->port_idx port is becoming Master. Table 14 (9.3.5) of the spec. */
void bmc_m3(struct pp_instance *ppi)
{
	/* In the default implementation, nothing should be done when a port
	 * goes to master state at m3. This empty function is a placeholder for
	 * extension-specific needs, to be implemented as a hook */
}

/* ppi->port_idx port is synchronized to Ebest Table 16 (9.3.5) of the spec. */
void bmc_s1(struct pp_instance *ppi, MsgHeader *hdr, MsgAnnounce *ann)
{
	struct DSParent *parent = DSPAR(ppi);
	struct DSTimeProperties *prop = DSPRO(ppi);

	/* Current DS */
	DSCUR(ppi)->stepsRemoved = ann->stepsRemoved + 1;

	/* Parent DS */
	parent->parentPortIdentity = hdr->sourcePortIdentity;
	parent->grandmasterIdentity = ann->grandmasterIdentity;
	parent->grandmasterClockQuality = ann->grandmasterClockQuality;
	parent->grandmasterPriority1 = ann->grandmasterPriority1;
	parent->grandmasterPriority2 = ann->grandmasterPriority2;

	/* Timeproperties DS */
	prop->timeSource = ann->timeSource;
	if (prop->currentUtcOffset != ann->currentUtcOffset) {
		pp_diag(ppi, bmc, 1, "New UTC offset: %i\n",
			ann->currentUtcOffset);
		prop->currentUtcOffset = ann->currentUtcOffset;
		ppi->t_ops->set(ppi, NULL);
	}
	prop->currentUtcOffsetValid = ((hdr->flagField[1] & FFB_UTCV) != 0);
	prop->leap59 = ((hdr->flagField[1] & FFB_LI59) != 0);
	prop->leap61 = ((hdr->flagField[1] & FFB_LI61) != 0);
	prop->timeTraceable = ((hdr->flagField[1] & FFB_TTRA) != 0);
	prop->frequencyTraceable = ((hdr->flagField[1] & FFB_FTRA) != 0);
	prop->ptpTimescale = ((hdr->flagField[1] & FFB_PTP) != 0);

	if (pp_hooks.s1)
		pp_hooks.s1(ppi, hdr, ann);
}

void bmc_p1(struct pp_instance *ppi)
{
	/* In the default implementation, nothing should be done when a port
	 * goes to passive state. This empty function is a placeholder for
	 * extension-specific needs, to be implemented as a hook */
}

void bmc_p2(struct pp_instance *ppi)
{
	/* In the default implementation, nothing should be done when a port
	 * goes to passive state. This empty function is a placeholder for
	 * extension-specific needs, to be implemented as a hook */
}

/* Copy local data set into header and ann message. 9.3.4 table 12. */
void bmc_copy_d0(struct pp_instance *ppi, struct pp_frgn_master *m)
{
	int i;
	struct DSDefault *defds = DSDEF(ppi);
	struct PortIdentity *port_id = &m->port_id;
	struct PortIdentity *source_id = &m->source_id;
	int *ann_cnt = m->ann_cnt;
	struct MsgHeader *hdr = &m->hdr;
	struct MsgAnnounce *ann = &m->ann;

	*port_id = DSPOR(ppi)->portIdentity;
	*source_id = DSPOR(ppi)->portIdentity;

	/* this shall be always qualified */
	for (i = 0; i < PP_FOREIGN_MASTER_TIME_WINDOW; i++)
		ann_cnt[i] = 1;

	ann->grandmasterIdentity = defds->clockIdentity;
	ann->grandmasterClockQuality = defds->clockQuality;
	ann->grandmasterPriority1 = defds->priority1;
	ann->grandmasterPriority2 = defds->priority2;
	ann->stepsRemoved = 0;

	hdr->sourcePortIdentity.clockIdentity = defds->clockIdentity;
}

int bmc_idcmp(struct ClockIdentity *a, struct ClockIdentity *b)
{
	return memcmp(a, b, sizeof(*a));
}

int bmc_pidcmp(struct PortIdentity *a, struct PortIdentity *b)
{
	return memcmp(a, b, sizeof(*a));
}

/* compare part2 of the datasets which is the topology, fig 27, page 89 */
int bmc_gm_cmp(struct pp_instance *ppi,
			   struct pp_frgn_master *a,
			   struct pp_frgn_master *b)
{
	int i;
	struct ClockQuality *qa, *qb;
	struct MsgAnnounce *aa = &a->ann;
	struct MsgAnnounce *ab = &b->ann;
	int *ca = a->ann_cnt;
	int *cb = b->ann_cnt;
	int qualifieda = 0;
	int qualifiedb = 0;

	/* bmc_gm_cmp is called several times, so report only at level 2 */
	pp_diag(ppi, bmc, 2, "%s\n", __func__);

	for (i = 0; i < PP_FOREIGN_MASTER_TIME_WINDOW; i++) {
		qualifieda += ca[i];
		qualifiedb += cb[i];
	}

	/* if B is not qualified  9.3.2.5 c) & 9.3.2.3 a) & b)*/
	if ((qualifieda >= PP_FOREIGN_MASTER_THRESHOLD)
	    && (qualifiedb < PP_FOREIGN_MASTER_THRESHOLD)) {
		pp_diag(ppi, bmc, 2, "Dataset B not qualified\n");
		return -1;
	}

	/* if A is not qualified  9.3.2.5 c) & 9.3.2.3 a) & b) */
	if ((qualifiedb >= PP_FOREIGN_MASTER_THRESHOLD)
	    && (qualifieda < PP_FOREIGN_MASTER_THRESHOLD)) {
		pp_diag(ppi, bmc, 2, "Dataset A not qualified\n");
		return 1;
	}

	/* if both are not qualified  9.3.2.5 c) & 9.3.2.3 a) & b) */
	if ((qualifieda < PP_FOREIGN_MASTER_THRESHOLD)
	    && (qualifiedb < PP_FOREIGN_MASTER_THRESHOLD)) {
		pp_diag(ppi, bmc, 2, "Dataset A & B not qualified\n");
		return 0;
	}

	qa = &aa->grandmasterClockQuality;
	qb = &ab->grandmasterClockQuality;

	if (aa->grandmasterPriority1 != ab->grandmasterPriority1) {
		pp_diag(ppi, bmc, 3, "Priority1 A: %i, Priority1 B: %i\n",
			aa->grandmasterPriority1, ab->grandmasterPriority1);
		return aa->grandmasterPriority1 - ab->grandmasterPriority1;
	}

	if (qa->clockClass != qb->clockClass) {
		pp_diag(ppi, bmc, 3, "ClockClass A: %i, ClockClass B: %i\n",
			qa->clockClass, qb->clockClass);
		return qa->clockClass - qb->clockClass;
	}

	if (qa->clockAccuracy != qb->clockAccuracy) {
		pp_diag(ppi, bmc, 3, "ClockAccuracy A: %i, ClockAccuracy B: %i\n",
			qa->clockAccuracy, qb->clockAccuracy);
		return qa->clockAccuracy - qb->clockAccuracy;
	}

	if (qa->offsetScaledLogVariance != qb->offsetScaledLogVariance) {
		pp_diag(ppi, bmc, 3, "Variance A: %i, Variance B: %i\n",
			qa->offsetScaledLogVariance, qb->offsetScaledLogVariance);
		return qa->offsetScaledLogVariance
				- qb->offsetScaledLogVariance;
	}

	if (aa->grandmasterPriority2 != ab->grandmasterPriority2) {
		pp_diag(ppi, bmc, 3, "Priority2 A: %i, Priority2 B: %i\n",
			aa->grandmasterPriority2, ab->grandmasterPriority2);
		return aa->grandmasterPriority2 - ab->grandmasterPriority2;
	}

	pp_diag(ppi, bmc, 3, "GmId A: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x,\n"
		"GmId B: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
		aa->grandmasterIdentity.id[0], aa->grandmasterIdentity.id[1],
		aa->grandmasterIdentity.id[2], aa->grandmasterIdentity.id[3],
		aa->grandmasterIdentity.id[4], aa->grandmasterIdentity.id[5],
		aa->grandmasterIdentity.id[6], aa->grandmasterIdentity.id[7],
		ab->grandmasterIdentity.id[0], ab->grandmasterIdentity.id[1],
		ab->grandmasterIdentity.id[2], ab->grandmasterIdentity.id[3],
		ab->grandmasterIdentity.id[4], ab->grandmasterIdentity.id[5],
		ab->grandmasterIdentity.id[6], ab->grandmasterIdentity.id[7]);
	return bmc_idcmp(&aa->grandmasterIdentity, &ab->grandmasterIdentity);
}

/* compare part2 of the datasets which is the topology, fig 28, page 90 */
int bmc_topology_cmp(struct pp_instance *ppi,
			   struct pp_frgn_master *a,
			   struct pp_frgn_master *b)
{
	int i;
	struct MsgAnnounce *aa = &a->ann;
	struct MsgAnnounce *ab = &b->ann;
	struct PortIdentity *pidtxa = &a->hdr.sourcePortIdentity;
	struct PortIdentity *pidtxb = &b->hdr.sourcePortIdentity;
	struct PortIdentity *pidrxa = &a->source_id;
	struct PortIdentity *pidrxb = &b->source_id;
	int *ca = a->ann_cnt;
	int *cb = b->ann_cnt;
	int qualifieda = 0;
	int qualifiedb = 0;
	int diff;

	/* bmc_topology_cmp is called several times, so report only at level 2
	 */
	pp_diag(ppi, bmc, 2, "%s\n", __func__);

	for (i = 0; i < PP_FOREIGN_MASTER_TIME_WINDOW; i++) {
		qualifieda += ca[i];
		qualifiedb += cb[i];
	}

	/* if B is not qualified  9.3.2.5 c) & 9.3.2.3 a) & b)*/
	if ((qualifieda >= PP_FOREIGN_MASTER_THRESHOLD)
	    && (qualifiedb < PP_FOREIGN_MASTER_THRESHOLD)) {
		pp_diag(ppi, bmc, 2, "Dataset B not qualified\n");
		return -1;
	}

	/* if A is not qualified  9.3.2.5 c) & 9.3.2.3 a) & b) */
	if ((qualifiedb >= PP_FOREIGN_MASTER_THRESHOLD)
	    && (qualifieda < PP_FOREIGN_MASTER_THRESHOLD)) {
		pp_diag(ppi, bmc, 2, "Dataset A not qualified\n");
		return 1;
	}

	/* if both are not qualified  9.3.2.5 c) & 9.3.2.3 a) & b) */
	if ((qualifieda < PP_FOREIGN_MASTER_THRESHOLD)
	    && (qualifiedb < PP_FOREIGN_MASTER_THRESHOLD)) {
		pp_diag(ppi, bmc, 2, "Dataset A & B not qualified\n");
		return 0;
	}

	diff = aa->stepsRemoved - ab->stepsRemoved;
	if (diff > 1 || diff < -1) {
		pp_diag(ppi, bmc, 3, "StepsRemoved A: %i, StepsRemoved B: %i\n",
			aa->stepsRemoved, ab->stepsRemoved);
		return diff;
	}

	if (diff > 0) {
		if (!bmc_pidcmp(pidtxa, pidrxa)) {
			pp_diag(ppi, bmc, 1, "%s:%i: Error 1\n",
				__func__, __LINE__);
			return 0;
		}
		pp_diag(ppi, bmc, 3, "StepsRemoved A: %i, StepsRemoved B: %i\n",
			aa->stepsRemoved, ab->stepsRemoved);
		return 1;

	}
	if (diff < 0) {
		if (!bmc_pidcmp(pidtxb, pidrxb)) {
			pp_diag(ppi, bmc, 1, "%s:%i: Error 1\n",
				__func__, __LINE__);
			return 0;
		}
		pp_diag(ppi, bmc, 3, "StepsRemoved A: %i, StepsRemoved B: %i\n",
			aa->stepsRemoved, ab->stepsRemoved);
		return -1;
	}
	/* stepsRemoved is equal, compare identities */
	diff = bmc_pidcmp(pidtxa, pidtxb);
	if (diff) {
		pp_diag(ppi, bmc, 3, "TxId A: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x.%04x,\n"
			"TxId B: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x.%04x\n",
			pidtxa->clockIdentity.id[0], pidtxa->clockIdentity.id[1],
			pidtxa->clockIdentity.id[2], pidtxa->clockIdentity.id[3],
			pidtxa->clockIdentity.id[4], pidtxa->clockIdentity.id[5],
			pidtxa->clockIdentity.id[6], pidtxa->clockIdentity.id[7],
			pidtxa->portNumber,
			pidtxb->clockIdentity.id[0], pidtxb->clockIdentity.id[1],
			pidtxb->clockIdentity.id[2], pidtxb->clockIdentity.id[3],
			pidtxb->clockIdentity.id[4], pidtxb->clockIdentity.id[5],
			pidtxb->clockIdentity.id[6], pidtxb->clockIdentity.id[7],
			pidtxb->portNumber);
		return diff;
	}

	/* sourcePortIdentity is equal, compare receive port identites, which
	 * is the last decision maker, which has to be different */
	pp_diag(ppi, bmc, 3, "RxId A: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x.%04x,\n"
		"RxId B: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x.%04x\n",
		pidrxa->clockIdentity.id[0], pidrxa->clockIdentity.id[1],
		pidrxa->clockIdentity.id[2], pidrxa->clockIdentity.id[3],
		pidrxa->clockIdentity.id[4], pidrxa->clockIdentity.id[5],
		pidrxa->clockIdentity.id[6], pidrxa->clockIdentity.id[7],
		pidrxa->portNumber,
		pidrxb->clockIdentity.id[0], pidrxb->clockIdentity.id[1],
		pidrxb->clockIdentity.id[2], pidrxb->clockIdentity.id[3],
		pidrxb->clockIdentity.id[4], pidrxb->clockIdentity.id[5],
		pidrxb->clockIdentity.id[6], pidrxb->clockIdentity.id[7],
		pidrxb->portNumber);
	return bmc_pidcmp(pidrxa, pidrxb);
}

/*
 * Data set comparison between two foreign masters. Return similar to
 * memcmp().  However, lower values take precedence, so in A-B (like
 * in comparisons,   > 0 means B wins (and < 0 means A wins).
 */
int bmc_dataset_cmp(struct pp_instance *ppi,
			   struct pp_frgn_master *a,
			   struct pp_frgn_master *b)
{

	struct MsgAnnounce *aa = &a->ann;
	struct MsgAnnounce *ab = &b->ann;

	/* dataset_cmp is called several times, so report only at level 2 */
	pp_diag(ppi, bmc, 2, "%s\n", __func__);

	if (!bmc_idcmp(&aa->grandmasterIdentity, &ab->grandmasterIdentity)) {
		/* Check topology */
		return bmc_topology_cmp(ppi, a, b);
	} else {
		/* Check grandmasters */
		return bmc_gm_cmp(ppi, a, b);
	}
}

/* State decision algorithm 9.3.3 Fig 26 */
static int bmc_state_decision(struct pp_instance *ppi)
{
	int cmpres;
	struct pp_frgn_master d0;
	struct DSParent *parent = DSPAR(ppi);
	struct pp_globals *ppg = GLBS(ppi);
	struct pp_instance *ppi_best;
	struct pp_frgn_master *erbest = &ppi->frgn_master[ppi->frgn_rec_best];
	struct pp_frgn_master *ebest;

	/* bmc_state_decision is called several times, so report only at
	 * level 2 */
	pp_diag(ppi, bmc, 2, "%s\n", __func__);

	if (ppi->role == PPSI_ROLE_SLAVE) {
		/* if on this conigured port is ebest it will be taken as
		 * parent */
		ebest = erbest;
		goto slave_s1;
	}

	if ((!ppi->frgn_rec_num) && (ppi->state == PPS_LISTENING))
		return PPS_LISTENING;

	/* copy local information to a foreign_master structure */
	bmc_copy_d0(ppi, &d0);


	if (ppi->role == PPSI_ROLE_MASTER) {
		/* if there is a better master show these values */
		if (ppg->ebest_idx >= 0) {
			/* don't update parent dataset */
			goto master_m3;
		} else {
			/* provide our info */
			goto master_m1;
		}
	}


	/* if there is a foreign master take it otherwise just go to master */
	if (ppg->ebest_idx >= 0) {
		ppi_best = INST(ppg, ppg->ebest_idx);
		ebest = &ppi_best->frgn_master[ppi_best->frgn_rec_best];
		pp_diag(ppi, bmc, 2, "Taking real Ebest at port %i foreign "
			"master %i/%i\n", (ppg->ebest_idx+1),
			ppi_best->frgn_rec_best, ppi_best->frgn_rec_num);
	} else {
		/* directly go to master state */
		pp_diag(ppi, bmc, 2, "No real Ebest\n");
		goto master_m1;
	}

	if (DSDEF(ppi)->clockQuality.clockClass < 128) {
		/* dataset_cmp D0 with Erbest */
		cmpres = bmc_dataset_cmp(ppi, &d0, erbest);
		if (cmpres < 0)
			goto master_m1;
		if (cmpres > 0)
			goto passive_p1;
	} else {
		/* dataset_cmp D0 with Ebest */
		cmpres = bmc_dataset_cmp(ppi, &d0, ebest);
		if (cmpres < 0)
			goto master_m2;
		if (cmpres > 0) {
			if (DSDEF(ppi)->numberPorts == 1)
				goto slave_s1; /* directly skip to ordinary
						* clock handling */
			else
				goto check_boundary_clk;
		}
	}
	pp_diag(ppi, bmc, 1, "%s: error\n", __func__);

	return PPS_FAULTY;

check_boundary_clk:
	/* If this port is the Ebest */
	if (ppi->port_idx == GLBS(ppi)->ebest_idx)
		goto slave_s1;

	/* bmc_gm_cmp Ebest with Erbest */
	cmpres = bmc_gm_cmp(ppi, ebest, erbest);
	if (cmpres < 0)
		goto master_m3;

	/* topology_cmp Ebest with Erbest */
	cmpres = bmc_topology_cmp(ppi, ebest, erbest);
	if (cmpres < 0)
		goto passive_p2;
	if (cmpres > 0)
		goto master_m3;

	pp_diag(ppi, bmc, 1, "%s: error\n", __func__);

	return PPS_FAULTY;

passive_p1:
	pp_diag(ppi, bmc, 1, "%s: passive p1\n", __func__);
	if (DSDEF(ppi)->clockQuality.clockClass == PP_CLASS_SLAVE_ONLY)
		return PPS_LISTENING;
	bmc_p1(ppi);
	return PPS_PASSIVE;

passive_p2:
	pp_diag(ppi, bmc, 1, "%s: passive p2\n", __func__);
	if (DSDEF(ppi)->clockQuality.clockClass == PP_CLASS_SLAVE_ONLY)
		return PPS_LISTENING;
	bmc_p2(ppi);
	return PPS_PASSIVE;

master_m1:
	pp_diag(ppi, bmc, 1, "%s: master m1\n", __func__);
	if (DSDEF(ppi)->clockQuality.clockClass == PP_CLASS_SLAVE_ONLY)
		return PPS_LISTENING;
	bmc_m1(ppi);
	if (ppi->state != PPS_MASTER) {
		/* if not already in pre master state start qualification */
		if (ppi->state != PPS_PRE_MASTER) {
			/* 9.2.6.11 a) timeout 0 */
			pp_timeout_clear(ppi, PP_TO_QUALIFICATION);
		}
		return PPS_PRE_MASTER;
	} else
		return PPS_MASTER;

master_m2:
	pp_diag(ppi, bmc, 1, "%s: master m2\n", __func__);
	if (DSDEF(ppi)->clockQuality.clockClass == PP_CLASS_SLAVE_ONLY)
		return PPS_LISTENING;
	bmc_m2(ppi);
	if (ppi->state != PPS_MASTER) {
		/* if not already in pre master state start qualification */
		if (ppi->state != PPS_PRE_MASTER) {
			/* 9.2.6.11 a) timeout 0 */
			pp_timeout_clear(ppi, PP_TO_QUALIFICATION);
		}
		return PPS_PRE_MASTER;
	} else
		return PPS_MASTER;

master_m3:
	pp_diag(ppi, bmc, 1, "%s: master m3\n", __func__);
	if (DSDEF(ppi)->clockQuality.clockClass == PP_CLASS_SLAVE_ONLY)
		return PPS_LISTENING;
	bmc_m3(ppi);
	if (ppi->state != PPS_MASTER) {
		/* if not already in pre master state start qualification */
		if (ppi->state != PPS_PRE_MASTER) {
			/* 9.2.6.11 b) timeout steps removed+1 */
			pp_timeout_set(ppi, PP_TO_QUALIFICATION);
		}
		return PPS_PRE_MASTER;
	} else
		return PPS_MASTER;

slave_s1:
	pp_diag(ppi, bmc, 1, "%s: slave s1\n", __func__);
	/* only update parent dataset if best master is on this port */
	if (ppi->port_idx == GLBS(ppi)->ebest_idx) {
		/* check if we have a new master */
		cmpres = bmc_pidcmp(&parent->parentPortIdentity,
				&ebest->hdr.sourcePortIdentity);
		bmc_s1(ppi, &ebest->hdr, &ebest->ann);
	} else {
		/* set that to zero in the case we don't have the master master
		 * on that port */
		cmpres = 0;
	}
	/* if we are not comming from the slave state we go to uncalibrated
	 * first */
	if (ppi->state != PPS_SLAVE) {
		return PPS_UNCALIBRATED;
	} else {
		/* if the master changed we go to uncalibrated*/
		if (cmpres) {
			pp_diag(ppi, bmc, 1,
				"new master, change to uncalibrated\n");
			return PPS_UNCALIBRATED;
		} else
			return PPS_SLAVE;
	}
}

static void bmc_age_frgn_master(struct pp_instance *ppi)
{
	int i, j;
	int qualified;

	/* bmc_age_frgn_master is called several times, so report only at
	 * level 2 */
	pp_diag(ppi, bmc, 2, "%s\n", __func__);

	for (i = 0; i < ppi->frgn_rec_num; i++) {
		/* get qualification */
		qualified = 0;
		for (j = 0; j < PP_FOREIGN_MASTER_TIME_WINDOW; j++)
			qualified += ppi->frgn_master[i].ann_cnt[j];

		/* shift qualification */
		for (j = 1; j < PP_FOREIGN_MASTER_TIME_WINDOW; j++)
			ppi->frgn_master[i].ann_cnt[
				      (PP_FOREIGN_MASTER_TIME_WINDOW - j)] =
				ppi->frgn_master[i].ann_cnt[
				      (PP_FOREIGN_MASTER_TIME_WINDOW - j - 1)];

		/* clear lowest */
		ppi->frgn_master[i].ann_cnt[0] = 0;

		/* remove aged out and shift foreign masters*/
		if (qualified == 0) {
			for (j = i; j < PP_NR_FOREIGN_RECORDS; j++) {
				if (j < (ppi->frgn_rec_num-1)) {
					/* overwrite and shift next foreign
					 * master in */
					memcpy(&ppi->frgn_master[j],
					       &ppi->frgn_master[(j+1)],
					       sizeof(ppi->frgn_master[j]));
				} else {
					/* clear the last (and others) since
					 * shifted */
					memset(&ppi->frgn_master[j], 0,
					       sizeof(ppi->frgn_master[j]));
				}
			}

			pp_diag(ppi, bmc, 1, "Aged out foreign master %i/%i\n",
				i, ppi->frgn_rec_num);

			/* one less and restart at the shifted one */
			ppi->frgn_rec_num--;
			i--;
		}
	}
}

/* Check if any port is in initilaizing state */
static int bmc_any_port_initializing(struct pp_globals *ppg)
{
	int i;
	struct pp_instance *ppi;

	/* bmc_any_port_initializing is called several times, so report only at
	 * level 2 */
	pp_diag(INST(ppg, 0), bmc, 2, "%s\n", __func__);

	for (i = 0; i < ppg->defaultDS->numberPorts; i++) {

		ppi = INST(ppg, i);

		if ((WR_DSPOR(ppi)->linkUP)
		    && (ppi->state == PPS_INITIALIZING)) {
			pp_diag(ppi, bmc, 2, "The first port in INITIALIZING "
				"state is %i\n", i);
			return 1;
		}
	}
	return 0;
}

/* Find Erbest, 9.3.2.2 */
static void bmc_update_erbest(struct pp_globals *ppg)
{
	int i, j, best;
	struct pp_instance *ppi;
	struct pp_frgn_master *frgn_master;
	struct PortIdentity *frgn_master_pid;

	/* bmc_update_erbest is called several times, so report only at
	 * level 2 */
	pp_diag(INST(ppg, 0), bmc, 2, "%s\n", __func__);

	for (i = 0; i < ppg->defaultDS->numberPorts; i++) {

		ppi = INST(ppg, i);
		frgn_master = ppi->frgn_master;

		if (ppi->frgn_rec_num > 0) {
			/* Only if port is not in the FAULTY or DISABLED
			 * state 9.2.6.8 */
			if ((ppi->state != PPS_FAULTY)
			    && (ppi->state != PPS_DISABLED)) {
				for (j = 1, best = 0; j < ppi->frgn_rec_num;
				     j++)
					if (bmc_dataset_cmp(ppi,
					      &frgn_master[j],
					      &frgn_master[best]
					    ) < 0)
						best = j;

				pp_diag(ppi, bmc, 1, "Best foreign master is "
					"at index %i/%i\n", best,
					ppi->frgn_rec_num);

				frgn_master_pid = &frgn_master[best].hdr.sourcePortIdentity;
				pp_diag(ppi, bmc, 3, "SourePortId = %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x.%04x\n",
					frgn_master_pid->clockIdentity.id[0], frgn_master_pid->clockIdentity.id[1],
					frgn_master_pid->clockIdentity.id[2], frgn_master_pid->clockIdentity.id[3],
					frgn_master_pid->clockIdentity.id[4], frgn_master_pid->clockIdentity.id[5],
					frgn_master_pid->clockIdentity.id[6], frgn_master_pid->clockIdentity.id[7],
					frgn_master_pid->portNumber);

				ppi->frgn_rec_best = best;

			} else {
				ppi->frgn_rec_num = 0;
				ppi->frgn_rec_best = 0;
				memset(&ppi->frgn_master, 0,
				       sizeof(ppi->frgn_master));
			}
		} else {
			/* lets just set the first one */
			ppi->frgn_rec_best = 0;
		}
	}
}

/* Find Ebest, 9.3.2.2 */
static void bmc_update_ebest(struct pp_globals *ppg)
{
	int i, best;
	struct pp_instance *ppi, *ppi_best;
	struct PortIdentity *frgn_master_pid;

	/* bmc_update_ebest is called several times, so report only at
	 * level 2 */
	pp_diag(INST(ppg, 0), bmc, 2, "%s\n", __func__);

	for (i = 1, best = 0; i < ppg->defaultDS->numberPorts; i++) {

		ppi_best = INST(ppg, best);
		ppi = INST(ppg, i);

		if ((ppi->frgn_rec_num > 0)
		    && (bmc_dataset_cmp(ppi,
			  &ppi->frgn_master[ppi->frgn_rec_best],
			  &ppi_best->frgn_master[ppi_best->frgn_rec_best]
			) < 0))
				best = i;
	}

	/* check if best master is qualified */
	ppi_best = INST(ppg, best);
	if (ppi_best->frgn_rec_num == 0) {
		pp_diag(ppi_best, bmc, 2, "No Ebest at port %i\n", (best+1));
		best = -1;
	} else {
		pp_diag(ppi_best, bmc, 1, "Best foreign master is at port "
			"%i\n", (best+1));
		frgn_master_pid = &ppi_best->frgn_master[ppi_best->frgn_rec_best].hdr.sourcePortIdentity;
		pp_diag(ppi_best, bmc, 3, "SourePortId = %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x.%04x\n",
			frgn_master_pid->clockIdentity.id[0], frgn_master_pid->clockIdentity.id[1],
			frgn_master_pid->clockIdentity.id[2], frgn_master_pid->clockIdentity.id[3],
			frgn_master_pid->clockIdentity.id[4], frgn_master_pid->clockIdentity.id[5],
			frgn_master_pid->clockIdentity.id[6], frgn_master_pid->clockIdentity.id[7],
			frgn_master_pid->portNumber);
	}

	if (ppg->ebest_idx != best) {
		ppg->ebest_idx = best;
		ppg->ebest_updated = 1;
	}
}

int bmc(struct pp_instance *ppi)
{
	struct pp_globals *ppg = GLBS(ppi);
	int next_state;

	/* bmc is called several times, so report only at level 2 */
	pp_diag(ppi, bmc, 2, "%s\n", __func__);

	/* Age table only based on timeouts*/
	if (pp_timeout(ppi, PP_TO_BMC)) {
		bmc_age_frgn_master(ppi);
		/* restart timer, shall occur at
		   least once per annnounce interval 9.2.6.8
		 */
		pp_timeout_set(ppi, PP_TO_BMC);
	}

	/* Only if port is not any port is in the INITIALIZING state 9.2.6.8 */
	if (bmc_any_port_initializing(ppg)) {
		pp_diag(ppi, bmc, 2, "A Port is in intializing\n");
		return ppi->state;
	}

	/* Calculate Erbest of all ports Figure 25 */
	bmc_update_erbest(ppg);

	/* Calulate Ebest Figure 25 */
	bmc_update_ebest(ppg);

	/* Make state decision */
	next_state = bmc_state_decision(ppi);
	
	/* Extra states handled here */
	if (pp_hooks.bmc_state_decision)
		next_state = pp_hooks.bmc_state_decision(ppi, next_state);
	
	return next_state;
}
