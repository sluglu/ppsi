/*
 * Copyright (C) 2011 CERN (www.cern.ch)
 * Author: Aurelio Colosimo
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */

#include <ppsi/ppsi.h>

/*
 * This is a global structure, because commandline and other config places
 * need to change some values in there.
 */
struct pp_runtime_opts __pp_default_rt_opts = {
	.clock_quality_clockClass = PP_CLASS_DEFAULT,
	.clock_quality_clockAccuracy = PP_ACCURACY_DEFAULT,
	.clock_quality_offsetScaledLogVariance = PP_VARIANCE_DEFAULT,
	.flags =		PP_DEFAULT_FLAGS,
	.ap =			PP_DEFAULT_AP,
	.ai =			PP_DEFAULT_AI,
	.s =			PP_DEFAULT_DELAY_S,
	.priority1 =		PP_DEFAULT_PRIORITY1,
	.priority2 =		PP_DEFAULT_PRIORITY2,
	.domainNumber =	PP_DEFAULT_DOMAIN_NUMBER,
	.ttl =			PP_DEFAULT_TTL,
	.externalPortConfigurationEnabled = PP_DEFAULT_EXT_PORT_CONFIG_ENABLE,
	.ptpPpsThresholdMs=PP_DEFAULT_PTP_PPSGEN_THRESHOLD_MS,
	.gmDelayToGenPpsSec=PP_DEFAULT_GM_DELAY_TO_GEN_PPS_SEC,
	.forcePpsGen=    FALSE,
	.ptpFallbackPpsGen=FALSE,

};

/* Default values used to fill configurable parameters associated to each instance */
/* These parameters can be then overwritten with the config file ppsi.conf */
struct pp_instance_cfg __pp_default_instance_cfg = {
		.profile=PPSI_PROFILE_PTP,
		.delayMechanism=E2E,
		.announce_interval=PP_DEFAULT_ANNOUNCE_INTERVAL,
		.announce_receipt_timeout=PP_DEFAULT_ANNOUNCE_RECEIPT_TIMEOUT,
		.sync_interval=PP_DEFAULT_SYNC_INTERVAL,
		.min_delay_req_interval=PP_DEFAULT_MIN_DELAY_REQ_INTERVAL,
		.min_pdelay_req_interval=PP_DEFAULT_MIN_PDELAY_REQ_INTERVAL,
#if CONFIG_HAS_EXT_L1SYNC
		.l1SyncEnabled=FALSE,
		.l1SyncRxCoherencyIsRequired=FALSE,
		.l1SyncTxCoherencyIsRequired=FALSE,
		.l1SyncCongruencyIsRequired=FALSE,
		.l1SyncOptParamsEnabled=FALSE,
		.l1SyncOptParamsTimestampsCorrectedTx=FALSE,
		.l1syncInterval=L1E_DEFAULT_L1SYNC_INTERVAL,
		.l1syncReceiptTimeout=L1E_DEFAULT_L1SYNC_RECEIPT_TIMEOUT,
#endif
		.asymmetryCorrectionEnable=FALSE,
		.egressLatency_ps=0,
		.ingressLatency_ps=0,
		.constantAsymmetry_ps=0,
		.scaledDelayCoefficient=0,
		.delayCoefficient=0,
		.desiredState=PPS_PASSIVE, /* Clause 17.3.6.2 ; The default value should be PASSIVE unless otherwise specified */
		.masterOnly=FALSE
};

/*
 * This file deals with opening and closing an instance. The channel
 * must already have been created. In practice, this initializes the
 * state machine to the first state.
 */

int pp_init_globals(struct pp_globals *ppg, struct pp_runtime_opts *pp_rt_opts)
{
	/*
	 * Initialize default data set
	 */
	int i,ret=0;
	defaultDS_t *def = ppg->defaultDS;
	struct pp_runtime_opts *rt_opts;

	def->twoStepFlag = TRUE;

	/* if ppg->nlinks == 0, let's assume that the 'pp_links style'
	 * configuration was not used, so we have 1 port */
	def->numberPorts = ppg->nlinks > 0 ? ppg->nlinks : 1;

	if (!ppg->rt_opts)
		ppg->rt_opts = pp_rt_opts;

	rt_opts = ppg->rt_opts;

	// Copy clock quality
	def->clockQuality.clockClass=rt_opts->clock_quality_clockClass;
	def->clockQuality.clockAccuracy=rt_opts->clock_quality_clockAccuracy;
	def->clockQuality.offsetScaledLogVariance=rt_opts->clock_quality_offsetScaledLogVariance;

	/* Clause 17.6.5.3 : Clause 9.2.2 shall not be in effect. If implemented, defaultDS.slaveOnly should be FALSE, and
	 * portDS.masterOnly should be FALSE on all PTP Ports of the PTP Instance.
	 */
	def->externalPortConfigurationEnabled=pp_rt_opts->externalPortConfigurationEnabled;
	def->slaveOnly=rt_opts->slaveOnly;
	if ( is_externalPortConfigurationEnabled(def) ) {
		if ( def->slaveOnly ) {
			pp_printf("ppsi: Incompatible configuration: SlaveOnly  and externalPortConfigurationEnabled\n");
			def->slaveOnly=FALSE;
		}
	}


	if ( is_slaveOnly(def) ) {
		if ( get_numberPorts(def) > 1 ) {
			/* Check if slaveOnly is allowed
			 * Only one ppsi instance must exist however n instances on the same physical port
			 * and  using the same protocol must be considered as one. We do this because these
			 * instances are exclusive and will be never enabled at the same time
			 */
			struct pp_instance *ppi = INST(ppg, 0);
			for (i = 1; i < get_numberPorts(def); i++) {
				struct pp_instance *ppi_cmp = INST(ppg, i);
				if ( ppi->proto != ppi_cmp->proto /* different protocol used */
						|| strcmp(ppi->cfg.iface_name,ppi_cmp->cfg.iface_name)!=0 /* Not the same interface */
					) {
					/* remove slaveOnly */
					def->slaveOnly = 0;
					pp_printf("ppsi: SlaveOnly cannot be applied. It is not a ordinary clock\n");
					break; /* No reason to continue */
				}
			}
		}
		if ( is_slaveOnly(def) ) {
			/* Configured clockClass must be also changed to avoid to be set by BMCA bmc_update_clock_quality() */
			rt_opts->clock_quality_clockClass =
					def->clockQuality.clockClass = PP_CLASS_SLAVE_ONLY;
			pp_printf("Slave Only, clock class set to %d\n", def->clockQuality.clockClass);
		}

	}

	def->priority1 = rt_opts->priority1;
	def->priority2 = rt_opts->priority2;
	def->domainNumber = rt_opts->domainNumber;

	for (i = 0; i < get_numberPorts(def); i++) {
		struct pp_instance *ppi = INST(ppg, i);

		ppi->state = PPS_DISABLED;
		ppi->pdstate = PP_PDSTATE_NONE;
		ppi->current_state_item = NULL;
		ppi->port_idx = i;
		ppi->frgn_rec_best = -1;
		pp_timeout_disable_all(ppi); /* By default, disable all timers */
	}

	if ( is_externalPortConfigurationEnabled(GDSDEF(ppg)) ) {
		Boolean isSlavePresent=FALSE;

		for (i = 0; i < get_numberPorts(def); i++) {
			struct pp_instance *ppi = INST(ppg, i);
			Enumeration8 desiradedState=ppi->cfg.desiredState;

			/* Clause 17.6.5.3 : - Clause 9.2.2 shall not be in effect */
			if ( ppi->portDS->masterOnly ) {
				/* priority given to externalPortConfigurationEnabled */
				ppi->portDS->masterOnly=FALSE;
				pp_printf("ppsi: Wrong configuration: externalPortConfigurationEnabled=materOnly=TRUE. materOnly set to FALSE\n");
			}
			ppi->externalPortConfigurationPortDS.desiredState =desiradedState ;
			isSlavePresent|=desiradedState==PPS_SLAVE || desiradedState==PPS_UNCALIBRATED;
		}
		if ( def->clockQuality.clockClass < 128 && isSlavePresent) {
			/* clockClass cannot be < 128 if a port is configured as slave */
			/* Configured clockClass must be also changed to avoid to be set by BMCA bmc_update_clock_quality() */
			rt_opts->clock_quality_clockClass =
					def->clockQuality.clockClass=PP_CLASS_DEFAULT;
			pp_printf("PPSi: GM clock set but slave present. Clock class set to %d\n", def->clockQuality.clockClass);
		}
	}

	for (i = 0; i < get_numberPorts(def); i++) {
		struct pp_instance *ppi = INST(ppg, i);
		int r;

		if (is_ext_hook_available(ppi,open)) {
		   ret=(r=ppi->ext_hooks->open(ppi, rt_opts))==0 ? ret : r;
		}
	}
	return ret;
}

int pp_close_globals(struct pp_globals *ppg)
{
	int i,ret=0;;
	defaultDS_t *def = ppg->defaultDS;

	for (i = 0; i < get_numberPorts(def); i++) {
		struct pp_instance *ppi = INST(ppg, i);
		int r;

		if (is_ext_hook_available(ppi,close) ){
		   ret=(r=ppi->ext_hooks->close(ppi))==0 ? ret : r;
		}
	}
	return ret;

}
