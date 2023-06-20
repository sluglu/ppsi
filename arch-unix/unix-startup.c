/*
 * Copyright (C) 2011-2022 CERN (www.cern.ch)
 * Author: Alessandro Rubini
 *
 * Released to the public domain
 */

/*
 * This is the startup thing for hosted environments. It
 * defines main and then calls the main loop.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/timex.h>

#include <ppsi/ppsi.h>
#include "ppsi-unix.h"

char *format_hex(char *s, const unsigned char *mac, int cnt);
char *format_hex8(char *s, const unsigned char *mac);

/* ppg and fields */
static struct pp_globals ppg_static;
static defaultDS_t defaultDS;
static currentDS_t currentDS;
static parentDS_t parentDS;
static timePropertiesDS_t timePropertiesDS;

extern struct pp_ext_hooks pp_hooks;

/**
 * Enable/disable asymmetry correction
 */
static void enable_asymmetryCorrection(struct pp_instance *ppi, Boolean enable ) {
	if ((ppi->asymmetryCorrectionPortDS.enable = enable) == TRUE ) {
		/* Enabled: The delay asymmetry will be calculated */

		ppi->asymmetryCorrectionPortDS.scaledDelayCoefficient =
			(ppi->cfg.scaledDelayCoefficient != 0) ?
				ppi->cfg.scaledDelayCoefficient :
				(RelativeDifference)(ppi->cfg.delayCoefficient * REL_DIFF_TWO_POW_FRACBITS);
		ppi->portDS->delayAsymCoeff =
			pp_servo_calculateDelayAsymCoefficient(ppi->asymmetryCorrectionPortDS.scaledDelayCoefficient);
	}
	ppi->asymmetryCorrectionPortDS.constantAsymmetry =
		picos_to_interval(ppi->cfg.constantAsymmetry_ps);
}

int main(int argc, char **argv)
{
	struct pp_globals *ppg;
	struct pp_instance *ppi;
	unsigned long seed;
	struct timex t;
	int i;

	setbuf(stdout, NULL);

	pp_printf("PPSi. Commit %s, built on " __DATE__ "\n", PPSI_VERSION);

	/* So far allow more than one instance of PPSi running on the same
	 * machine. 
	 TODO: to be considered to allow only one instance of PPSi to run
	 * at the same time. 
	 * Potential problems my be in:
	 * shmem (not used in arch-unix)
	 * race of setting of time if more than one instance run as slave
	 */

	ppg = &ppg_static;
	ppg->defaultDS = &defaultDS;
	ppg->currentDS = &currentDS;
	ppg->parentDS = &parentDS;
	ppg->timePropertiesDS = &timePropertiesDS;
	ppg->rt_opts = &__pp_default_rt_opts;

	/* We are hosted, so we can allocate */
	ppg->max_links = PP_MAX_LINKS;
	ppg->arch_glbl_data = calloc(1, sizeof(struct unix_arch_data));
	ppg->pp_instances = calloc(ppg->max_links, sizeof(struct pp_instance));

	if ((!ppg->arch_glbl_data) || (!ppg->pp_instances)) {
		fprintf(stderr, "ppsi: out of memory\n");
		exit(1);
	}

	/* Set default configuration value for all instances */
	for (i = 0; i < ppg->max_links; i++) {
		memcpy(&INST(ppg, i)->cfg, &__pp_default_instance_cfg,
		       sizeof(__pp_default_instance_cfg));
	}

	/* Set offset here, so config parsing can override it */
	memset(&t, 0, sizeof(t));
	if (adjtimex(&t) >= 0) {
		ppg->timePropertiesDS->currentUtcOffset = (Integer16)t.tai;
	}

	if (pp_parse_cmdline(ppg, argc, argv) != 0)
		return -1;

	/* If no item has been parsed, provide a default file or string */
	if (ppg->cfg.cfg_items == 0)
		pp_config_file(ppg, 0, PP_DEFAULT_CONFIGFILE);

	/* No config found, add default */
	if (ppg->cfg.cfg_items == 0)
		pp_config_string(ppg, strdup("link 0; iface eth0; proto udp"));

	for (i = 0; i < ppg->nlinks; i++) {

		ppi = INST(ppg, i);
		ppi->ch[PP_NP_EVT].fd = -1;
		ppi->ch[PP_NP_GEN].fd = -1;
		ppi->glbs = ppg;
		ppi->vlans_array_len = CONFIG_VLAN_ARRAY_SIZE,
		ppi->iface_name = ppi->cfg.iface_name;
		ppi->port_name = ppi->cfg.port_name;
		ppi->delayMechanism = ppi->cfg.delayMechanism;
		ppi->portDS = calloc(1, sizeof(*ppi->portDS));
		ppi->servo = calloc(1, sizeof(*ppi->servo));
		ppi->ext_hooks = &pp_hooks;
		ppi->ptp_support = TRUE;

		if (ppi->portDS) {
			switch (ppi->cfg.profile) {
			case PPSI_PROFILE_WR:
#if CONFIG_HAS_PROFILE_WR
				ppi->protocol_extension = PPSI_EXT_WR;
				/* Add WR extension portDS */
				if ( !(ppi->portDS->ext_dsport =
					wrs_shm_alloc(ppsi_head,
						      sizeof(struct wr_dsport))
				      )
				    ) {
					goto exit_out_of_memory;
				}

				/* Allocate WR data extension */
				if (! (ppi->ext_data =
						wrs_shm_alloc(ppsi_head,
							      sizeof(struct wr_data))
				      )
				   ) {
					goto exit_out_of_memory;
				}
				/* Set WR extension hooks */
				ppi->ext_hooks = &wr_ext_hooks;
				enable_asymmetryCorrection(ppi, TRUE);
#else
				fprintf(stderr, "ppsi: Profile WR not supported");
				exit(1);
#endif
				break;

			case PPSI_PROFILE_HA:
#if CONFIG_HAS_PROFILE_HA
				if (!enable_l1Sync(ppi, TRUE))
					goto exit_out_of_memory;
				/* Force mandatory attributes - Do not take care of the configuration */
				L1E_DSPOR_BS(ppi)->rxCoherentIsRequired = TRUE;
				L1E_DSPOR_BS(ppi)->txCoherentIsRequired = TRUE;
				L1E_DSPOR_BS(ppi)->congruentIsRequired = TRUE;
				L1E_DSPOR_BS(ppi)->L1SyncEnabled = TRUE;
				L1E_DSPOR_BS(ppi)->optParamsEnabled = FALSE;
				enable_asymmetryCorrection(ppi, TRUE);
#else
				fprintf(stderr, "ppsi: Profile HA not supported");
				exit(1);
#endif
				break;

			case PPSI_PROFILE_PTP :
				/* Do not take care of L1SYNC */
				enable_asymmetryCorrection(ppi,
					    ppi->cfg.asymmetryCorrectionEnable);
				ppi->protocol_extension = PPSI_EXT_NONE;

				break;

			case PPSI_PROFILE_CUSTOM :
#if CONFIG_HAS_PROFILE_CUSTOM
				ppi->protocol_extension = PPSI_EXT_NONE; /* can be changed ...*/
#if CONFIG_HAS_EXT_L1SYNC
				if (ppi->cfg.l1SyncEnabled) {
					if (!enable_l1Sync(ppi, TRUE))
						goto exit_out_of_memory;
					/* Read L1SYNC parameters */
					L1E_DSPOR_BS(ppi)->rxCoherentIsRequired = ppi->cfg.l1SyncRxCoherencyIsRequired;
					L1E_DSPOR_BS(ppi)->txCoherentIsRequired = ppi->cfg.l1SyncTxCoherencyIsRequired;
					L1E_DSPOR_BS(ppi)->congruentIsRequired = ppi->cfg.l1SyncCongruencyIsRequired;
					L1E_DSPOR_BS(ppi)->optParamsEnabled = ppi->cfg.l1SyncOptParamsEnabled;
					if (L1E_DSPOR_BS(ppi)->optParamsEnabled) {
						L1E_DSPOR_OP(ppi)->timestampsCorrectedTx = ppi->cfg.l1SyncOptParamsTimestampsCorrectedTx;
					}
				}
				enable_asymmetryCorrection(ppi, ppi->cfg.asymmetryCorrectionEnable);
#endif
#else
					fprintf(stderr, "ppsi: Profile CUSTOM not supported");
					exit(1);
#endif
				break;
			}
			/* Parameters profile independent */
			ppi->timestampCorrectionPortDS.egressLatency = picos_to_interval(ppi->cfg.egressLatency_ps);
			ppi->timestampCorrectionPortDS.ingressLatency = picos_to_interval(ppi->cfg.ingressLatency_ps);
			ppi->timestampCorrectionPortDS.messageTimestampPointLatency = 0;
			ppi->portDS->masterOnly = ppi->cfg.masterOnly; /* can be overridden in pp_init_globals() */
		} else {
			goto exit_out_of_memory;
		}

		/* The following default names depend on TIME= at build time */
		ppi->n_ops = &DEFAULT_NET_OPS;
		ppi->t_ops = &DEFAULT_TIME_OPS;

		ppi->__tx_buffer = malloc(PP_MAX_FRAME_LENGTH);
		ppi->__rx_buffer = malloc(PP_MAX_FRAME_LENGTH);

		if (!ppi->portDS || !ppi->__tx_buffer || !ppi->__rx_buffer) {
			goto exit_out_of_memory;
		}
		
	}
	
	pp_init_globals(ppg, &__pp_default_rt_opts);

	seed = time(NULL);
	if (getenv("PPSI_DROP_SEED"))
		seed = atoi(getenv("PPSI_DROP_SEED"));
	ppsi_drop_init(ppg, seed);

	unix_main_loop(ppg);
	return 0; /* never reached */

exit_out_of_memory:
	fprintf(stderr, "ppsi: out of memory\n");
	exit(1);
}

char *format_hex(char *s, const unsigned char *mac, int cnt)
{
	int i;
	*s = '\0';
	for (i = 0; i < cnt; i++) {
		pp_sprintf(s, "%s%02x:", s, mac[i]);
	}

	/* remove last colon */
	s[cnt * 3 - 1] = '\0'; /* cnt * strlen("FF:") - 1 */
	return s;
}
