/*
 * Copyright (C) 2011 CERN (www.cern.ch)
 * Author: Aurelio Colosimo
 * Based on PTPd project v. 2.1.0 (see AUTHORS for details)
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */

#include <ppsi/ppsi.h>

/*
 * Initialize parentDS
 */
static void init_parent_ds(struct pp_instance *ppi)
{	
	/* 8.2.3.2 */
	DSPAR(ppi)->parentPortIdentity.clockIdentity =
		DSDEF(ppi)->clockIdentity;
	DSPAR(ppi)->parentPortIdentity.portNumber = 0;
	/* 8.2.3.3 skipped (parentStats is not used) */
	/* 8.2.3.4 */
	DSPAR(ppi)->observedParentOffsetScaledLogVariance = 0xffff;
	/* 8.2.3.5 */
	DSPAR(ppi)->observedParentClockPhaseChangeRate = 0x7fffffffUL;
	/* 8.2.3.6 */
	DSPAR(ppi)->grandmasterIdentity = DSDEF(ppi)->clockIdentity;
	/* 8.2.3.7 */
	DSPAR(ppi)->grandmasterClockQuality = DSDEF(ppi)->clockQuality;
	/* 8.2.3.8 */
	DSPAR(ppi)->grandmasterPriority1 = DSDEF(ppi)->priority1;
	/* 8.2.3.9 */
	DSPAR(ppi)->grandmasterPriority2 = DSDEF(ppi)->priority2;
}

/*
 * Initializes network and other stuff
 */

int pp_initializing(struct pp_instance *ppi, void *buf, int len)
{
	unsigned char *id, *mac;
	struct DSPort *port = DSPOR(ppi);
	struct pp_runtime_opts *opt = OPTS(ppi);
	struct pp_globals *ppg = GLBS(ppi);
	int init_dspar = 1;
	int ret = 0;
	int i;

	if (ppi->n_ops->init(ppi) < 0) /* it must handle being called twice */
		goto failure;

	/* only fill in the parent data set when initializing */
	if (DSDEF(ppi)->numberPorts > 1) {
		for (i = 0; i < ppg->defaultDS->numberPorts; i++) {
			if (INST(ppg, i)->state != PPS_INITIALIZING)
				init_dspar = 0;
		}				
	}
	
	if (init_dspar)
		init_parent_ds(ppi);

	/* Clock identity comes from mac address with 0xff:0xfe intermixed */
	id = (unsigned char *)&DSDEF(ppi)->clockIdentity;
	/* we always take the one from the first port */
	mac = INST(ppg, 0)->ch[PP_NP_GEN].addr;
	id[0] = mac[0];
	id[1] = mac[1];
	id[2] = mac[2];
	id[3] = 0xff;
	id[4] = 0xfe;
	id[5] = mac[3];
	id[6] = mac[4];
	id[7] = mac[5];

	/*
	 * Initialize parent data set
	 */
	if (init_dspar)
		init_parent_ds(ppi);

	/*
	 * Initialize port data set
	 */
	memcpy(&port->portIdentity.clockIdentity,
		&DSDEF(ppi)->clockIdentity, PP_CLOCK_IDENTITY_LENGTH);
	/* 1-based port number =  index of this ppi in the global array */
	port->portIdentity.portNumber = 1 + ppi - ppi->glbs->pp_instances;
	port->logMinDelayReqInterval = PP_DEFAULT_DELAYREQ_INTERVAL;
	port->logAnnounceInterval = opt->announce_intvl;
	port->announceReceiptTimeout = PP_DEFAULT_ANNOUNCE_RECEIPT_TIMEOUT;
	port->logSyncInterval = opt->sync_intvl;
	port->versionNumber = PP_VERSION_PTP;
	pp_timeout_init(ppi);
	pp_timeout_setall(ppi);

	if (pp_hooks.init)
		ret = pp_hooks.init(ppi, buf, len);
	if (ret) {
		pp_diag(ppi, ext, 1, "%s: can't init extension\n", __func__);
		goto failure;
	}

	pp_diag(ppi, bmc, 1, "clock class = %d\n",
			DSDEF(ppi)->clockQuality.clockClass);
	pp_diag(ppi, bmc, 1, "clock accuracy = %d\n",
			DSDEF(ppi)->clockQuality.clockAccuracy);

	msg_init_header(ppi, ppi->tx_ptp); /* This is used for all tx */
	
	if (ppi->role != PPSI_ROLE_MASTER)
		ppi->next_state = PPS_LISTENING;
	else
		ppi->next_state = PPS_MASTER;

#ifdef CONFIG_ABSCAL
	/* absolute calibration only exists in arch-wrpc, so far */
	extern int ptp_mode;
	if (ptp_mode == 4 /* WRC_MODE_ABSCAL */)
		ppi->next_state = WRS_WR_LINK_ON;
#endif

	return 0;

failure:
	ppi->next_delay = 1000; /* wait 1s before retrying */
	return 0;
}
