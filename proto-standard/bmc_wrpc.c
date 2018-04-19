
/*
 * Copyright (C) 2011 CERN (www.cern.ch)
 * Author: Aurelio Colosimo
 * Based on PTPd project v. 2.1.0 (see AUTHORS for details)
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */

#include <ppsi/ppsi.h>

#ifdef CONFIG_ARCH_WRPC

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
	struct DSTimeProperties *prop = DSPRO(ppi);
	int ret = 0;
	int offset, leap59, leap61;

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
	/* based on the clock class we set the frequency traceable flags */
	if ((defds->clockQuality.clockClass < PP_PTP_CLASS_GM_UNLOCKED) ||
	    (defds->clockQuality.clockClass < PP_ARB_CLASS_GM_UNLOCKED))
		prop->frequencyTraceable = TRUE;
	else
		prop->frequencyTraceable = FALSE;

	switch (defds->clockQuality.clockClass) {
		case PP_PTP_CLASS_GM_LOCKED:
		case PP_PTP_CLASS_GM_HOLDOVER:
			prop->ptpTimescale = TRUE;
			prop->timeSource = GPS;
			break;
		case PP_PTP_CLASS_GM_UNLOCKED:
			prop->ptpTimescale = TRUE;
			prop->timeSource = INTERNAL_OSCILLATOR;
			break;
		case PP_ARB_CLASS_GM_LOCKED:
		case PP_ARB_CLASS_GM_HOLDOVER:
			prop->ptpTimescale = FALSE;
			prop->timeSource = GPS;
			break;
		case PP_ARB_CLASS_GM_UNLOCKED:
			prop->ptpTimescale = FALSE;
			prop->timeSource = INTERNAL_OSCILLATOR;
			break;
		default:
			/* FIXME: if we don't know better we stay with this */
			prop->ptpTimescale = TRUE;
			prop->timeSource = INTERNAL_OSCILLATOR;
			break;
	}
	
	if (prop->ptpTimescale) {
		ret = ppi->t_ops->get_utc_offset(ppi, &offset, &leap59, &leap61);
		if (ret) {
			offset = PP_DEFAULT_UTC_OFFSET;
			pp_diag(ppi, bmc, 1,
				"Could not get UTC offset from system, taking default: %i\n",
				offset);
		
		}
		
		if (prop->currentUtcOffset != offset) {
			pp_diag(ppi, bmc, 1, "New UTC offset: %i\n",
				offset);
			prop->currentUtcOffset = offset;
			ppi->t_ops->set(ppi, NULL);
		}
		
		if (ret)
		{
			prop->timeTraceable = FALSE;
			prop->currentUtcOffsetValid = FALSE;
			prop->leap59 = FALSE;
			prop->leap61 = FALSE;
		}
		else
		{
			prop->timeTraceable = TRUE;
			prop->currentUtcOffsetValid = TRUE;
			prop->leap59 = (leap59 != 0);
			prop->leap61 = (leap61 != 0);
		}
	} else {
		/* 9.4 for ARB just take the value when built */
		prop->currentUtcOffset = PP_DEFAULT_UTC_OFFSET;
		/* always false */
		prop->timeTraceable = FALSE;
		prop->currentUtcOffsetValid = FALSE;
		prop->leap59 = FALSE;
		prop->leap61 = FALSE;
	}
}

/* ppi->port_idx port is becoming Master. Table 14 (9.3.5) of the spec. */
void bmc_m3(struct pp_instance *ppi)
{
	/* In the default implementation, nothing should be done when a port
	 * goes to master state at m3. This empty function is a placeholder for
	 * extension-specific needs, to be implemented as a hook */
}

/* ppi->port_idx port is synchronized to Ebest Table 16 (9.3.5) of the spec. */
void bmc_s1(struct pp_instance *ppi,
			struct pp_frgn_master *frgn_master)
{
	struct DSParent *parent = DSPAR(ppi);
	struct DSTimeProperties *prop = DSPRO(ppi);
	int ret = 0;
	int offset, leap59, leap61;
	int hours, minutes, seconds;

	/* Current DS */
	DSCUR(ppi)->stepsRemoved = frgn_master->stepsRemoved + 1;

	/* Parent DS */
	parent->parentPortIdentity = frgn_master->sourcePortIdentity;
	parent->grandmasterIdentity = frgn_master->grandmasterIdentity;
	parent->grandmasterClockQuality = frgn_master->grandmasterClockQuality;
	parent->grandmasterPriority1 = frgn_master->grandmasterPriority1;
	parent->grandmasterPriority2 = frgn_master->grandmasterPriority2;

	/* Timeproperties DS */
	prop->ptpTimescale = ((frgn_master->flagField[1] & FFB_PTP) != 0);
	
	if (prop->ptpTimescale) {
		ret = ppi->t_ops->get_utc_time(ppi, &hours, &minutes, &seconds);
		if (ret) {
			pp_diag(ppi, bmc, 1, 
				"Could not get UTC time from system, taking received flags\n");
			prop->leap59 = ((frgn_master->flagField[1] & FFB_LI59) != 0);
			prop->leap61 = ((frgn_master->flagField[1] & FFB_LI61) != 0);
			prop->currentUtcOffset = frgn_master->currentUtcOffset;
		} else {
			if (hours >= 12) {
				/* stop 2 announce intervals before midnight */
				if ((hours == 23) && (minutes == 59) && 
				    (seconds >= (60 - (2 * (1 << ppi->portDS->logAnnounceInterval))))) {
					pp_diag(ppi, bmc, 2, 
						"Approaching midnight, not updating leap flags\n");			
					ret = ppi->t_ops->get_utc_offset(ppi, &offset, &leap59, &leap61);
					if (ret) {
						pp_diag(ppi, bmc, 1, 
							"Could not get UTC offset from system\n");
					} else {
						pp_diag(ppi, bmc, 3, 
							"Current UTC flags, "
							"offset: %i, "
							"leap59: %i, "
							"leap61: %i\n",
							offset, leap59, leap61);		
					}
				} else {
					ret = ppi->t_ops->get_utc_offset(ppi, &offset, &leap59, &leap61);
					if (ret) {
						pp_diag(ppi, bmc, 1, 
							"Could not get UTC flags from system, taking received flags\n");
						prop->leap59 = ((frgn_master->flagField[1] & FFB_LI59) != 0);
						prop->leap61 = ((frgn_master->flagField[1] & FFB_LI61) != 0);
						prop->currentUtcOffset = frgn_master->currentUtcOffset;
						
					} else {
						pp_diag(ppi, bmc, 3, 
							"Current UTC flags, "
							"offset: %i, "
							"leap59: %i, "
							"leap61: %i\n",
							offset, leap59, leap61);		

						if (((leap59 != 0) != ((frgn_master->flagField[1] & FFB_LI59) != 0)) ||
						    ((leap61 != 0) != ((frgn_master->flagField[1] & FFB_LI61) != 0)) ||
						    (offset != frgn_master->currentUtcOffset)) {			
							prop->leap59 = ((frgn_master->flagField[1] & FFB_LI59) != 0);
							prop->leap61 = ((frgn_master->flagField[1] & FFB_LI61) != 0);
							prop->currentUtcOffset = frgn_master->currentUtcOffset;

							if (prop->leap59)
								leap59 = 1;
							else
								leap59 = 0;

							if (prop->leap61)
								leap61 = 1;
							else
								leap61 = 0;

							offset = prop->currentUtcOffset;

							pp_diag(ppi, bmc, 1, 
								"UTC flags changed, "
								"offset: %i, "
								"leap59: %i, "
								"leap61: %i\n",
							        offset, leap59, leap61);		
							
							ret = ppi->t_ops->set_utc_offset(ppi, offset, leap59, leap61);
							if (ret) {
								pp_diag(ppi, bmc, 1, 
									"Could not set UTC offset on system\n");
							}
						}
					}
				}

			} else {			
				/* stop for 2 announce intervals after midnight */
				if ((hours == 00) && (minutes == 00) && 
				    (seconds <= (0 + (2 * (1 << ppi->portDS->logAnnounceInterval))))) {
					pp_diag(ppi, bmc, 2, 
						"short after midnight, taking local offset\n");			
					ret = ppi->t_ops->get_utc_offset(ppi, &offset, &leap59, &leap61);
					if (ret) {
						pp_diag(ppi, bmc, 1, 
							"Could not get UTC offset from system\n");
					} else {
						pp_diag(ppi, bmc, 3, 
							"Current UTC flags, "
							"offset: %i, "
							"leap59: %i, "
							"leap61: %i\n",
							offset, leap59, leap61);		
						prop->currentUtcOffset = offset;
					}
					prop->leap59 = FALSE;
					prop->leap61 = FALSE;
				} else {	
					if (prop->currentUtcOffset != frgn_master->currentUtcOffset) {
						pp_diag(ppi, bmc, 1, "New UTC offset in the middle of the day: %i\n",
							frgn_master->currentUtcOffset);
						prop->currentUtcOffset = frgn_master->currentUtcOffset;
						ppi->t_ops->set(ppi, NULL);
					}
					prop->leap59 = ((frgn_master->flagField[1] & FFB_LI59) != 0);
					prop->leap61 = ((frgn_master->flagField[1] & FFB_LI61) != 0);
				}
			}
		}
	} else {
		/* just take what we get */
		prop->leap59 = ((frgn_master->flagField[1] & FFB_LI59) != 0);
		prop->leap61 = ((frgn_master->flagField[1] & FFB_LI61) != 0);
		prop->currentUtcOffset = frgn_master->currentUtcOffset;
	}
	prop->currentUtcOffsetValid = ((frgn_master->flagField[1] & FFB_UTCV) != 0);
	prop->timeTraceable = ((frgn_master->flagField[1] & FFB_TTRA) != 0);
	prop->frequencyTraceable = ((frgn_master->flagField[1] & FFB_FTRA) != 0);
	prop->ptpTimescale = ((frgn_master->flagField[1] & FFB_PTP) != 0);
	prop->timeSource = frgn_master->timeSource;

	if (pp_hooks.s1)
		pp_hooks.s1(ppi, frgn_master);
}

/* Copy local data set into header and ann message. 9.3.4 table 12. */
void bmc_setup_local_frgn_master(struct pp_instance *ppi,
			   struct pp_frgn_master *frgn_master)
{
	int i;

	/* this shall be always qualified */
	for (i = 0; i < PP_FOREIGN_MASTER_TIME_WINDOW; i++)
		frgn_master->foreignMasterAnnounceMessages[i] = 1;

	memcpy(&frgn_master->receivePortIdentity,
	       &DSPOR(ppi)->portIdentity, sizeof(PortIdentity));
	frgn_master->sequenceId = 0;

	memcpy(&frgn_master->sourcePortIdentity,
		   &DSPOR(ppi)->portIdentity, sizeof(PortIdentity));
	memset(&frgn_master->flagField,
		   0, sizeof(frgn_master->flagField));
	frgn_master->currentUtcOffset = 0; //TODO get this from somewhere
	frgn_master->grandmasterPriority1 = DSDEF(ppi)->priority1;
	memcpy(&frgn_master->grandmasterClockQuality,
	       &DSDEF(ppi)->clockQuality, sizeof(ClockQuality));
	frgn_master->grandmasterPriority2 = DSDEF(ppi)->priority2;
	memcpy(&frgn_master->grandmasterIdentity,
		   &DSDEF(ppi)->clockIdentity, sizeof(ClockIdentity));
	frgn_master->stepsRemoved = 0;
	frgn_master->timeSource = INTERNAL_OSCILLATOR; //TODO get this from somewhere

	frgn_master->ext_specific = 0;
}

int bmc_idcmp(struct ClockIdentity *a, struct ClockIdentity *b)
{
	return memcmp(a, b, sizeof(*a));
}

int bmc_pidcmp(struct PortIdentity *a, struct PortIdentity *b)
{
	int ret;
	
	ret = bmc_idcmp(&a->clockIdentity, &b->clockIdentity);
	if (ret != 0)
		return ret;

	return a->portNumber - b->portNumber;
}

/* State decision algorithm 9.3.3 Fig 26 */
static int bmc_state_decision(struct pp_instance *ppi)
{
	int i;
	int cmpres;
	struct pp_frgn_master d0;
	struct DSParent *parent = DSPAR(ppi);
	struct pp_globals *ppg = GLBS(ppi);
	struct pp_frgn_master *erbest = &ppi->frgn_master[ppi->frgn_rec_best];
	struct pp_frgn_master *ebest;
	int qualified = 0;

	/* bmc_state_decision is called several times, so report only at
	 * level 2 */
	pp_diag(ppi, bmc, 2, "%s\n", __func__);

	/* check if the erbest is qualified */
	if (ppi->frgn_rec_num) {
		for (i = 0; i < PP_FOREIGN_MASTER_TIME_WINDOW; i++)
			qualified += erbest->foreignMasterAnnounceMessages[i];
	}

	if (ppi->role == PPSI_ROLE_SLAVE) {
		if ((!ppi->frgn_rec_num) || (qualified < PP_FOREIGN_MASTER_THRESHOLD))
    			return PPS_LISTENING;
		else {     
			/* if this is the slave port of the whole system then go to slave otherwise stay in listening*/
			if (ppi->port_idx == ppg->ebest_idx) {
				/* if on this configured port is ebest it will be taken as
				 * parent */
				ebest = erbest;
				goto slave_s1;
			} else
    				return PPS_LISTENING;			
		}
	}


	if (((!ppi->frgn_rec_num) || (qualified < PP_FOREIGN_MASTER_THRESHOLD)) && (ppi->state == PPS_LISTENING))
		return PPS_LISTENING;

	/* copy local information to a foreign_master structure */
	bmc_setup_local_frgn_master(ppi, &d0);


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

	// At this point the MASTER and SLAVE roles are covered
	pp_diag(ppi, bmc, 1, "%s: error\n", __func__);

	return PPS_FAULTY;

master_m1:
	pp_diag(ppi, bmc, 1, "%s: master m1\n", __func__);
	if (DSDEF(ppi)->clockQuality.clockClass == PP_CLASS_SLAVE_ONLY) {
		/* 9.2.6.11 c) reset ANNOUNCE RECEIPT timeout when entering*/
		if (ppi->state != PPS_LISTENING)
			pp_timeout_set(ppi, PP_TO_ANN_RECEIPT);
		return PPS_LISTENING;
	}
	bmc_m1(ppi);
	if ((ppi->state != PPS_MASTER) &&
		(ppi->state != PPS_PRE_MASTER)) {
		/* 9.2.6.10 a) timeout 0 */
		pp_timeout_clear(ppi, PP_TO_QUALIFICATION);
		return PPS_PRE_MASTER;
	} else {
		/* the decision to go from PPS_PRE_MASTER to PPS_MASTER is
		 * done outside the BMC, so just return the current state */
		return ppi->state;
	}

master_m3:
	pp_diag(ppi, bmc, 1, "%s: master m3\n", __func__);
	if (DSDEF(ppi)->clockQuality.clockClass == PP_CLASS_SLAVE_ONLY) {
		/* 9.2.6.11 c) reset ANNOUNCE RECEIPT timeout when entering*/
		if (ppi->state != PPS_LISTENING)
			pp_timeout_set(ppi, PP_TO_ANN_RECEIPT);
		return PPS_LISTENING;
	}
	bmc_m3(ppi);
	if ((ppi->state != PPS_MASTER) &&
		(ppi->state != PPS_PRE_MASTER)) {
		/* timeout reinit */
		pp_timeout_init(ppi);
		/* 9.2.6.11 b) timeout steps removed+1*/
		pp_timeout_set(ppi, PP_TO_QUALIFICATION);
		return PPS_PRE_MASTER;
	} else {
		/* the decision to go from PPS_PRE_MASTER to PPS_MASTER is
		 * done outside the BMC, so just return the current state */
		return ppi->state;
	}

slave_s1:
	pp_diag(ppi, bmc, 1, "%s: slave s1\n", __func__);
	/* only update parent dataset if best master is on this port */
	if (ppi->port_idx == GLBS(ppi)->ebest_idx) {
		/* check if we have a new master */
		cmpres = bmc_pidcmp(&parent->parentPortIdentity,
				&ebest->sourcePortIdentity);
		bmc_s1(ppi, ebest);
	} else {
		/* set that to zero in the case we don't have the master
		 * on that port */
		cmpres = 0;
	}
	/* if we are not comming from the slave state we go to uncalibrated
	 * first */
	if ((ppi->state != PPS_SLAVE) &&
		(ppi->state != PPS_UNCALIBRATED)) {
		/* 9.2.6.11 c) reset ANNOUNCE RECEIPT timeout when entering*/
		pp_timeout_set(ppi, PP_TO_ANN_RECEIPT);
		return PPS_UNCALIBRATED;
	} else {
		/* if the master changed we go to uncalibrated*/
		if (cmpres) {
			pp_diag(ppi, bmc, 1,
				"new master, change to uncalibrated\n");
			/* 9.2.6.11 c) reset ANNOUNCE RECEIPT timeout when entering*/
			if (ppi->state != PPS_UNCALIBRATED)
				pp_timeout_set(ppi, PP_TO_ANN_RECEIPT);
			return PPS_UNCALIBRATED;
		} else {
			/* 9.2.6.11 c) reset ANNOUNCE RECEIPT timeout when entering*/
			if (ppi->state != PPS_SLAVE)
				pp_timeout_set(ppi, PP_TO_ANN_RECEIPT);
			/* the decision to go from UNCALIBRATED to SLAVEW is
			 * done outside the BMC, so just return the current state */
			return ppi->state;
		}
	}
}

void bmc_store_frgn_master(struct pp_instance *ppi,
		       struct pp_frgn_master *frgn_master, void *buf, int len)
{
	int i;
	MsgHeader *hdr = &ppi->received_ptp_header;
	MsgAnnounce ann;

	/* clear qualification timeouts */
	for (i = 0; i < PP_FOREIGN_MASTER_TIME_WINDOW; i++)
		frgn_master->foreignMasterAnnounceMessages[i] = 0;

	/*
	 * header and announce field of each Foreign Master are
	 * useful to run Best Master Clock Algorithm
	 */
	msg_unpack_announce(buf, &ann);

	memcpy(&frgn_master->receivePortIdentity,
	       &DSPOR(ppi)->portIdentity, sizeof(PortIdentity));
	frgn_master->sequenceId = hdr->sequenceId;

	memcpy(&frgn_master->sourcePortIdentity,
		   &hdr->sourcePortIdentity, sizeof(PortIdentity));
	memcpy(&frgn_master->flagField,
		   &hdr->flagField, sizeof(frgn_master->flagField));
	frgn_master->currentUtcOffset = ann.currentUtcOffset;
	frgn_master->grandmasterPriority1 = ann.grandmasterPriority1;
	memcpy(&frgn_master->grandmasterClockQuality,
	       &ann.grandmasterClockQuality, sizeof(ClockQuality));
	frgn_master->grandmasterPriority2 = ann.grandmasterPriority2;
	memcpy(&frgn_master->grandmasterIdentity,
		   &ann.grandmasterIdentity, sizeof(ClockIdentity));
	frgn_master->stepsRemoved = ann.stepsRemoved;
	frgn_master->timeSource = ann.timeSource;

	//TODO do we need a hook for this?
	frgn_master->ext_specific = ann.ext_specific;

}

void bmc_add_frgn_master(struct pp_instance *ppi, void *buf,
			    int len)
{
	int i, j, sel;
	struct pp_frgn_master frgn_master;
	MsgHeader *hdr;
	struct PortIdentity *pid;

	pp_diag(ppi, bmc, 2, "%s\n", __func__);

	/* if we are a configured master don't add*/
	if (ppi->role == PPSI_ROLE_MASTER)
		return;

	/* if in DISABLED, INITIALIZING or FAULTY ignore announce */
	if ((ppi->state == PPS_DISABLED) ||
		(ppi->state == PPS_FAULTY) ||
		(ppi->state == PPS_INITIALIZING))
		return;

	/*
	 * header and announce field of each Foreign Master are
	 * useful to run Best Master Clock Algorithm
	 */
	bmc_store_frgn_master(ppi, &frgn_master, buf, len);

	hdr = &ppi->received_ptp_header;
	pid = &hdr->sourcePortIdentity;

	pp_diag(ppi, bmc, 3, "Foreign Master Port Id: "
		"%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x.%04x,\n",
		pid->clockIdentity.id[0], pid->clockIdentity.id[1],
		pid->clockIdentity.id[2], pid->clockIdentity.id[3],
		pid->clockIdentity.id[4], pid->clockIdentity.id[5],
		pid->clockIdentity.id[6], pid->clockIdentity.id[7],
		pid->portNumber);


	/* Check if announce from a port from this clock 9.3.2.5 a) */
	if (!bmc_idcmp(&pid->clockIdentity,
			&DSDEF(ppi)->clockIdentity)) {
		pp_diag(ppi, bmc, 2, "Announce frame from this clock\n");
		return;
	}

	/* Check if announce has steps removed larger than 255 9.3.2.5 d) */
	if (frgn_master.stepsRemoved >= 255) {
		pp_diag(ppi, bmc, 2, "Announce frame steps removed"
			"larger or equal 255: %i\n",
			frgn_master.stepsRemoved);
		return;
	}

	/* Check if foreign master is already known */
	for (i = 0; i < ppi->frgn_rec_num; i++) {
		if (!bmc_pidcmp(pid,
			    &ppi->frgn_master[i].sourcePortIdentity)) {

			pp_diag(ppi, bmc, 2, "Foreign Master %i updated\n", i);

			/* fill in number of announce received */
			for (j = 0; j < PP_FOREIGN_MASTER_TIME_WINDOW; j++) {
				frgn_master.foreignMasterAnnounceMessages[j] =
					ppi->frgn_master[i].foreignMasterAnnounceMessages[j];
			}
			/* update the number of announce received if correct
			 * sequence number 9.3.2.5 b) */
			if (hdr->sequenceId
				  == (ppi->frgn_master[i].sequenceId + 1))
				frgn_master.foreignMasterAnnounceMessages[0]++;

			/* already in Foreign master data set, update info */
			memcpy(&ppi->frgn_master[i], &frgn_master,
				   sizeof(frgn_master));
			return;
		}
	}

	/* set qualification timeouts as valid to compare against worst*/
	for (i = 0; i < PP_FOREIGN_MASTER_TIME_WINDOW; i++)
		frgn_master.foreignMasterAnnounceMessages[i] = 1;

	/* New foreign master */
	sel=0; // works only when PP_NR_FOREIGN_RECORDS=1
	if (ppi->frgn_rec_num < PP_NR_FOREIGN_RECORDS) {
		/* there is space for a new one */
		ppi->frgn_rec_num++;
	}

	/* clear qualification timeouts */
	for (i = 0; i < PP_FOREIGN_MASTER_TIME_WINDOW; i++)
		frgn_master.foreignMasterAnnounceMessages[i] = 0;

	/* This is the first one qualified 9.3.2.5 e)*/
	frgn_master.foreignMasterAnnounceMessages[0] = 1;

	/* Copy the temporary foreign master entry */
	memcpy(&ppi->frgn_master[sel], &frgn_master,
		   sizeof(frgn_master));

	pp_diag(ppi, bmc, 1, "New foreign Master %i added\n", sel);
}

static void bmc_flush_frgn_master(struct pp_instance *ppi)
{
	pp_diag(ppi, bmc, 2, "%s\n", __func__);

	memset(ppi->frgn_master, 0, sizeof(ppi->frgn_master));
	ppi->frgn_rec_num = 0;
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
			qualified += ppi->frgn_master[i].foreignMasterAnnounceMessages[j];

		/* shift qualification */
		for (j = 1; j < PP_FOREIGN_MASTER_TIME_WINDOW; j++)
			ppi->frgn_master[i].foreignMasterAnnounceMessages[
				      (PP_FOREIGN_MASTER_TIME_WINDOW - j)] =
				ppi->frgn_master[i].foreignMasterAnnounceMessages[
				      (PP_FOREIGN_MASTER_TIME_WINDOW - j - 1)];

		/* clear lowest */
		ppi->frgn_master[i].foreignMasterAnnounceMessages[0] = 0;

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

/* Find Erbest, 9.3.2.2 */
static void bmc_update_erbest(struct pp_globals *ppg)
{
	struct pp_instance *ppi= INST(ppg, 0);

	/* bmc_update_erbest is called several times, so report only at
	 * level 2 */
	pp_diag(ppi, bmc, 2, "%s\n", __func__);

	/* if link is down clear foreign master table */
	if ((!ppi->link_up) && (ppi->frgn_rec_num > 0))
		bmc_flush_frgn_master(ppi);

	ppi->frgn_rec_best = 0;
	if (ppi->frgn_rec_num > 0) {
		/* Only if port is not in the FAULTY or DISABLED
		 * state 9.2.6.8 */
		if ((ppi->state == PPS_FAULTY)
			|| (ppi->state == PPS_DISABLED)) {
			ppi->frgn_rec_num = 0;
			memset(&ppi->frgn_master, 0,
				   sizeof(ppi->frgn_master));
		}
	}
}

/* Find Ebest, 9.3.2.2 */
static void bmc_update_ebest(struct pp_globals *ppg)
{
	int best=0;
	struct pp_instance *ppi_best;
	PortIdentity *frgn_master_pid;

	/* bmc_update_ebest is called several times, so report only at
	 * level 2 */
	pp_diag(INST(ppg, 0), bmc, 2, "%s\n", __func__);

	/* check if best master is qualified */
	ppi_best = INST(ppg, 0);
	if (ppi_best->frgn_rec_num == 0) {
		pp_diag(ppi_best, bmc, 2, "No Ebest at port %i\n", (best+1));
		best = -1;
	} else {
		pp_diag(ppi_best, bmc, 1, "Best foreign master is at port "
			"%i\n", (best+1));
		frgn_master_pid = &ppi_best->frgn_master[ppi_best->frgn_rec_best].sourcePortIdentity;
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

static void bmc_update_clock_quality(struct pp_instance *ppi)
{
	struct pp_globals *ppg = GLBS(ppi);
	struct pp_runtime_opts *rt_opts = ppi->glbs->rt_opts;
	int state;
	int rt_opts_clock_quality_clockClass=rt_opts->clock_quality.clockClass;
	char *servo_state=NULL;

	if (rt_opts_clock_quality_clockClass < 128) {

		if ((rt_opts_clock_quality_clockClass == PP_PTP_CLASS_GM_LOCKED) ||
		    (rt_opts_clock_quality_clockClass == PP_ARB_CLASS_GM_LOCKED)) {
			pp_diag(ppi, bmc, 2,
				"GM locked class configured, checking servo state\n");

		} else if ((rt_opts_clock_quality_clockClass == PP_PTP_CLASS_GM_UNLOCKED) ||
			   (rt_opts_clock_quality_clockClass == PP_ARB_CLASS_GM_UNLOCKED)) {
			pp_diag(ppi, bmc, 2,
				"GM unlocked class configured, skipping checking servo state\n");
			return;
		} else {
			pp_diag(ppi, bmc, 2,
				"GM unknown clock class configured, skipping checking servo state\n");
			return;
		}

		if (ppi->t_ops->get_servo_state(ppi, &state)) {
			pp_diag(ppi, bmc, 1,
				"Could not get servo state, taking old clock class: %i\n",
				ppg->defaultDS->clockQuality.clockClass);
			return;
		}

		switch (state) {
		case PP_SERVO_LOCKED:
			if (rt_opts_clock_quality_clockClass == PP_PTP_CLASS_GM_LOCKED) {
				if (ppg->defaultDS->clockQuality.clockClass != PP_PTP_CLASS_GM_LOCKED) {
					ppg->defaultDS->clockQuality.clockClass = PP_PTP_CLASS_GM_LOCKED;
					ppg->defaultDS->clockQuality.clockAccuracy = PP_PTP_ACCURACY_GM_LOCKED;
					ppg->defaultDS->clockQuality.offsetScaledLogVariance = PP_PTP_VARIANCE_GM_LOCKED;
					servo_state="locked";
				}
			} else if (rt_opts_clock_quality_clockClass == PP_ARB_CLASS_GM_LOCKED) {
				if (ppg->defaultDS->clockQuality.clockClass != PP_ARB_CLASS_GM_LOCKED) {
					ppg->defaultDS->clockQuality.clockClass = PP_ARB_CLASS_GM_LOCKED;
					ppg->defaultDS->clockQuality.clockAccuracy = PP_ARB_ACCURACY_GM_LOCKED;
					ppg->defaultDS->clockQuality.offsetScaledLogVariance = PP_ARB_VARIANCE_GM_LOCKED;
					servo_state="locked";
				}
			}
			break;

		case PP_SERVO_HOLDOVER:
			if (rt_opts_clock_quality_clockClass == PP_PTP_CLASS_GM_LOCKED) {
				if (ppg->defaultDS->clockQuality.clockClass != PP_PTP_CLASS_GM_HOLDOVER) {
					ppg->defaultDS->clockQuality.clockClass = PP_PTP_CLASS_GM_HOLDOVER;
					ppg->defaultDS->clockQuality.clockAccuracy = PP_PTP_ACCURACY_GM_HOLDOVER;
					ppg->defaultDS->clockQuality.offsetScaledLogVariance = PP_PTP_VARIANCE_GM_HOLDOVER;
					servo_state="in holdover";
				}
			} else if (rt_opts_clock_quality_clockClass== PP_ARB_CLASS_GM_LOCKED) {
				if (ppg->defaultDS->clockQuality.clockClass != PP_ARB_CLASS_GM_HOLDOVER) {
					ppg->defaultDS->clockQuality.clockClass = PP_ARB_CLASS_GM_HOLDOVER;
					ppg->defaultDS->clockQuality.clockAccuracy = PP_ARB_ACCURACY_GM_HOLDOVER;
					ppg->defaultDS->clockQuality.offsetScaledLogVariance = PP_ARB_VARIANCE_GM_HOLDOVER;
					servo_state="in holdover";
				}
			}
			break;

		case PP_SERVO_UNLOCKED:
			if (rt_opts_clock_quality_clockClass == PP_PTP_CLASS_GM_LOCKED) {
				if (ppg->defaultDS->clockQuality.clockClass != PP_PTP_CLASS_GM_UNLOCKED) {
					ppg->defaultDS->clockQuality.clockClass = PP_PTP_CLASS_GM_UNLOCKED;
					ppg->defaultDS->clockQuality.clockAccuracy = PP_PTP_ACCURACY_GM_UNLOCKED;
					ppg->defaultDS->clockQuality.offsetScaledLogVariance = PP_PTP_VARIANCE_GM_UNLOCKED;
					servo_state="unlocked";
				}
			} else if (rt_opts_clock_quality_clockClass == PP_ARB_CLASS_GM_LOCKED) {
				if (ppg->defaultDS->clockQuality.clockClass != PP_ARB_CLASS_GM_UNLOCKED) {
					ppg->defaultDS->clockQuality.clockClass = PP_ARB_CLASS_GM_UNLOCKED;
					ppg->defaultDS->clockQuality.clockAccuracy = PP_ARB_ACCURACY_GM_UNLOCKED;
					ppg->defaultDS->clockQuality.offsetScaledLogVariance = PP_ARB_VARIANCE_GM_UNLOCKED;
					servo_state="unlocked";
				}
			}
			break;

		case PP_SERVO_UNKNOWN:
		default:
			pp_diag(ppi, bmc, 2,
				"Unknown servo state, taking old clock class: %i\n",
				ppg->defaultDS->clockQuality.clockClass);

			break;

		}

		if ( servo_state!=NULL ) {
			pp_diag(ppi, bmc, 1,
					"Servo %s, "
					"new clock class: %i,"
					"new clock accuracy: %i,"
					"new clock variance: %04x,",
					servo_state,
				    ppg->defaultDS->clockQuality.clockClass,
					ppg->defaultDS->clockQuality.clockAccuracy,
					ppg->defaultDS->clockQuality.offsetScaledLogVariance);

		}
	}
}

int bmc(struct pp_instance *ppi)
{
	struct pp_globals *ppg = GLBS(ppi);
	int next_state;
	// int ret = 0;

	/* bmc is called several times, so report only at level 2 */
	pp_diag(ppi, bmc, 2, "%s\n", __func__);

	/* check if we shall update the clock qualities */
	bmc_update_clock_quality(ppi);

	/* Age table only based on timeouts*/
	if (pp_timeout(ppi, PP_TO_BMC)) {
		bmc_age_frgn_master(ppi);
		/* restart timer, shall occur at
		   least once per annnounce interval 9.2.6.8
		 */
		pp_timeout_set(ppi, PP_TO_BMC);
	}

	// Remove as we have just one port
	// /* Only if port is not any port is in the INITIALIZING state 9.2.6.8 */
	//if (bmc_any_port_initializing(ppg)) {
	//	pp_diag(ppi, bmc, 2, "A Port is in intializing\n");
	//	return ppi->state;
	//}

	/* Calculate Erbest of all ports Figure 25 */
	bmc_update_erbest(ppg);

	// No longer needed as the number of ports is 1
	// if (DSDEF(ppi)->numberPorts > 1) {
	//	ret = bmc_check_frgn_master(ppi);
	//}

	/* Calulate Ebest Figure 25 */
	bmc_update_ebest(ppg);

	if (!ppi->link_up) {
		/* Set it back to initializing */
		next_state = PPS_INITIALIZING;
	// No longer needed as the number of ports is 1 : ret always equal to 0 in that case
	//} else if (ret == 1) {
	//	bmc_p1(ppi);
	//	next_state = PPS_PASSIVE;
	} else {
		/* Make state decision */
		next_state = bmc_state_decision(ppi);
	}

	/* Extra states handled here */
	if (pp_hooks.state_decision)
		next_state = pp_hooks.state_decision(ppi, next_state);

	return next_state;
}

#endif
