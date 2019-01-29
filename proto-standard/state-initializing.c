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
	unsigned char *mac;
	unsigned char mac_port1[6];
	portDS_t *port = DSPOR(ppi);
	struct pp_globals *ppg = GLBS(ppi);
	int ret = 0;
	int i;
	unsigned int portidx;
	unsigned int remainder;
	int initds = 1;

	if (ppi->n_ops->init(ppi) < 0) /* it must handle being called twice */
		goto failure;

	/* only fill in the data set when initializing */
	if (!CODEOPT_ONE_PORT() && DSDEF(ppi)->numberPorts > 1) {
		for (i = 0; i < ppg->defaultDS->numberPorts; i++) {
			if ((INST(ppg, i)->state != PPS_INITIALIZING) && (INST(ppg, i)->link_up == TRUE)) 
				initds = 0;
		}			
	}
					
	/*
	 * Initialize default and parent data set
	 */
	if (initds) 
	{
		if (!CODEOPT_ONE_PORT() && DSDEF(ppi)->numberPorts > 1) {
			/* Clock identity comes from mac address with 0xff:0xfe intermixed */
			mac = ppi->ch[PP_NP_GEN].addr;
			/* calculate MAC of Port 0 */
			portidx = ppi - ppi->glbs->pp_instances;
			remainder = portidx;
			for (i = 5; i >= 0; i--) {
				mac_port1[i] = mac[i] - remainder;
				if (mac[i] >= remainder)
					remainder = 0;
				else
					remainder = 1;
			}
		} else {
			/* Clock identity comes from mac address with 0xff:0xfe intermixed */
			for (i = 5; i >= 0; i--)
				mac_port1[i] = ((unsigned char*)ppi->ch[PP_NP_GEN].addr)[i];
		}
			
		memcpy( DSDEF(ppi)->clockIdentity.id, mac_port1, PP_CLOCK_IDENTITY_LENGTH);
		DSDEF(ppi)->clockIdentity.id[3] = 0xff;
		DSDEF(ppi)->clockIdentity.id[4] = 0xfe;

		init_parent_ds(ppi);	
	}
		
	/*
	 * Initialize port data set
	 */
	memcpy(&port->portIdentity.clockIdentity,
		&DSDEF(ppi)->clockIdentity, PP_CLOCK_IDENTITY_LENGTH);
	/* 1-based port number =  index of this ppi in the global array */
	port->portIdentity.portNumber = 1 + ppi - ppi->glbs->pp_instances;
	port->versionNumber = PP_VERSION_PTP;
	port->minorVersionNumber = PP_MINOR_VERSION_PTP;
	pp_timeout_init(ppi);
	pp_timeout_setall(ppi);/* PP_TO_BMC is not set by default */
	pp_timeout_set(ppi, PP_TO_BMC);

	ppi->link_state=PP_LSTATE_PROTOCOL_DETECTION;
	ppi->ptp_msg_received=FALSE;
	ppi->ext_enabled=(ppi->protocol_extension!=PPSI_EXT_NONE);

	if (is_ext_hook_available(ppi,init))
		ret = ppi->ext_hooks->init(ppi, buf, len);
	if (ret) {
		pp_diag(ppi, ext, 1, "%s: can't init extension\n", __func__);
		goto failure;
	}

	pp_diag(ppi, bmc, 1, "clock class = %d\n",
			DSDEF(ppi)->clockQuality.clockClass);
	pp_diag(ppi, bmc, 1, "clock accuracy = %d\n",
			DSDEF(ppi)->clockQuality.clockAccuracy);

	msg_init_header(ppi, ppi->tx_ptp); /* This is used for all tx */
	
	if (DSDEF(ppi)->externalPortConfigurationEnabled) {
		/* Clause 17.6.5.2 : the member portDS.portState shall be set to
		 * the value of the member externalPortConfigurationPortDS.desiredState
		 */
		if ( ppi->externalPortConfigurationPortDS.desiredState==PPS_SLAVE)
			ppi->next_state=PPS_UNCALIBRATED;
		else
			ppi->next_state = ppi->externalPortConfigurationPortDS.desiredState;
	}
	else
		ppi->next_state = PPS_LISTENING;

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
