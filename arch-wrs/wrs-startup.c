/*
 * Copyright (C) 2011 CERN (www.cern.ch)
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
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/timex.h>
#include <signal.h>

#include <minipc.h>
#include <hal_exports.h>

#include <ppsi/ppsi.h>
#include <ppsi-wrs.h>
#include <libwr/shmem.h>

#  define WRSW_HAL_RETRIES 1000

#define WRSW_HAL_TIMEOUT 2000000 /* us */

struct wrh_operations wrh_oper = {
	.locking_enable = wrs_locking_enable,
	.locking_poll = wrs_locking_poll,
	.locking_disable = wrs_locking_disable,
	.locking_reset = wrs_locking_reset,
	.enable_ptracker = wrs_enable_ptracker,

	.adjust_in_progress = wrs_adjust_in_progress,
	.adjust_counters = wrs_adjust_counters,
	.adjust_phase = wrs_adjust_phase,
	.read_calib_data = wrs_read_calibration_data,

	.enable_timing_output = wrs_enable_timing_output,

	.set_timing_mode= wrs_set_timing_mode,
	.get_timing_mode= wrs_get_timing_mode,
	.get_timing_mode_state= wrs_get_timing_mode_state,
};

struct minipc_ch *hal_ch;
struct minipc_ch *ppsi_ch;
struct hal_port_state *hal_ports;
int hal_nports;
struct wrs_shm_head *ppsi_head;

extern struct pp_ext_hooks  pp_hooks;

#if CONFIG_HAS_EXT_L1SYNC
/**
 * Enable the l1sync extension for a given ppsi instance
 */
static  int enable_l1Sync(struct pp_instance *ppi, Boolean enable) {
	if ( enable ) {
		ppi->protocol_extension=PPSI_EXT_L1S;
		/* Add L1SYNC extension portDS */
		if ( !(ppi->portDS->ext_dsport =wrs_shm_alloc(ppsi_head, sizeof(l1e_ext_portDS_t))) ) {
			return 0;
		}

		/* Allocate L1SYNC data extension */
		if (! (ppi->ext_data = wrs_shm_alloc(ppsi_head,sizeof(struct l1e_data))) ) {
			return 0;
		}
		/* Set L1SYNC state. Must be done here because the init hook is called only in the initializing state. If
		 * the port is not connected, the initializing is then never called so the L1SYNC state is invalid (0)
		 */
		L1E_DSPOR_BS(ppi)->L1SyncState=L1SYNC_DISABLED;
		L1E_DSPOR_BS(ppi)->L1SyncEnabled=TRUE;
		/* Set L1SYNC extension hooks */
		ppi->ext_hooks=&l1e_ext_hooks;
	}
	return 1;
}
#endif

/* Calculate delay asymmetry coefficient :
 *    delayCoeff/(delayCoeff/2)
 */
static __inline__ double  calculateDelayAsymCoefficient(double  delayCoefficient) {
	return delayCoefficient/(delayCoefficient+2.0);
}

/**
 * Enable/disable asymmetry correction
 */
static void enable_asymmetryCorrection(struct pp_instance *ppi, Boolean enable ) {
	if ( (ppi->asymmetryCorrectionPortDS.enable=enable)==TRUE ) {
		/* Enabled: The delay asymmetry will be calculated */
		double delayCoefficient;

		if ( ppi->cfg.scaledDelayCoefficient != 0) {
			ppi->asymmetryCorrectionPortDS.scaledDelayCoefficient=ppi->cfg.scaledDelayCoefficient;
			delayCoefficient=ppi->cfg.scaledDelayCoefficient/REL_DIFF_TWO_POW_FRACBITS;
		} else {
			ppi->asymmetryCorrectionPortDS.scaledDelayCoefficient=(RelativeDifference)(ppi->cfg.delayCoefficient * REL_DIFF_TWO_POW_FRACBITS);
			delayCoefficient=ppi->cfg.delayCoefficient;
		}
		ppi->portDS->delayAsymCoeff=(RelativeDifference)(calculateDelayAsymCoefficient(delayCoefficient) * REL_DIFF_TWO_POW_FRACBITS);
	}
	ppi->asymmetryCorrectionPortDS.constantAsymmetry=picos_to_interval(ppi->cfg.constantAsymmetry_ps);
}

static char *strCodeOpt="CODEOPT=("
#if CONFIG_HAS_CODEOPT_EPC_ENABLED
		" EPC"
#endif
#if CONFIG_HAS_CODEOPT_SO_ENABLED
		" SO"
#endif
#if CONFIG_HAS_CODEOPT_SINGLE_FMASTER
		" SFM"
#endif
#if CONFIG_HAS_CODEOPT_SINGLE_PORT
		" SP"
#endif
		")";

int main(int argc, char **argv)
{
	struct pp_globals *ppg;
	struct pp_instance *ppi;
	unsigned long seed;
	struct timex t;
	int i, hal_retries;
	struct wrs_shm_head *hal_head;
	struct hal_shmem_header *h;

	setbuf(stdout, NULL);

	pp_printf("PPSi. Commit %s, built on " __DATE__ ", %s\n",
		PPSI_VERSION,
		strCodeOpt);
	/* check if there is another instance of PPSi already running */
	ppsi_head = wrs_shm_get(wrs_shm_ptp, "", WRS_SHM_READ);
	if (!ppsi_head) {
		pp_printf("Unable to open shm for PPSi! Unable to check if "
			  "there is another PPSi instance running. Error: %s\n",
			  strerror(errno));
		exit(1);
	}

	/* check if pid is 0 (shm not filled) or process with provided
	 * pid does not exist (probably crashed) */
	{
		int nbTry=1;

		while ( nbTry >= 0 ) {
			if ((ppsi_head->pid != 0) && (kill(ppsi_head->pid, 0) == 0)) {
				nbTry--;
				sleep(1);
			}
			else
				break;
		}
		if ( nbTry<0 ) {
			wrs_shm_put(ppsi_head);
			pp_printf("Fatal: There is another PPSi instance running. "
				  "Exit...\n\n");
			exit(1);
		}
	}

	/* try connecting to HAL multiple times in case it's still not ready */
	hal_retries = WRSW_HAL_RETRIES;
	while (hal_retries) { /* may be never, if built without WR extension */
		hal_ch = minipc_client_create(WRSW_HAL_SERVER_ADDR,
					      MINIPC_FLAG_VERBOSE);
		if (hal_ch)
			break;
		hal_retries--;
		usleep(WRSW_HAL_TIMEOUT);
	}

	if (!hal_ch) {
		pp_printf("ppsi: could not connect to HAL RPC");
		exit(1);
	}

	/* If we connected, we also know "for sure" shmem is there */
	hal_head = wrs_shm_get(wrs_shm_hal,"", WRS_SHM_READ);
	if (!hal_head || !hal_head->data_off) {
		pp_printf("ppsi: Can't connect with HAL "
			  "shared memory\n");
		exit(1);
	}
	if (hal_head->version != HAL_SHMEM_VERSION) {
		pp_printf("ppsi: unknown HAL's shm version %i "
			  "(known is %i)\n", hal_head->version,
			  HAL_SHMEM_VERSION);
		exit(1);
	}

	h = (void *)hal_head + hal_head->data_off;
	hal_nports = h->nports;

	hal_ports = wrs_shm_follow(hal_head, h->ports);

	if (!hal_ports) {
		pp_printf("ppsi: unable to follow hal_ports pointer "
			  "in HAL's shmem\n");
		exit(1);
	}

	/* And create your own channel, until we move to shmem too */
	ppsi_ch = minipc_server_create("ptpd", 0);
	if (!ppsi_ch) { /* FIXME should we retry ? */
		pp_printf("ppsi: could not create minipc server");
		exit(1);
	}
	wrs_init_ipcserver(ppsi_ch);

	ppsi_head = wrs_shm_get(wrs_shm_ptp, "ppsi",
				WRS_SHM_WRITE | WRS_SHM_LOCKED);
	if (!ppsi_head) {
		fprintf(stderr, "Fatal: could not create shmem: %s\n",
			strerror(errno));
		exit(1);
	}
	ppsi_head->version = WRS_PPSI_SHMEM_VERSION;

	ppg = wrs_shm_alloc(ppsi_head, sizeof(*ppg));
	ppg->defaultDS = wrs_shm_alloc(ppsi_head, sizeof(*ppg->defaultDS));
	ppg->currentDS = wrs_shm_alloc(ppsi_head, sizeof(*ppg->currentDS));
	ppg->parentDS =  wrs_shm_alloc(ppsi_head, sizeof(*ppg->parentDS));
	ppg->timePropertiesDS = wrs_shm_alloc(ppsi_head,sizeof(*ppg->timePropertiesDS));
	ppg->rt_opts = &__pp_default_rt_opts;

	ppg->max_links = PP_MAX_LINKS;

	/* NOTE: arch_data is not in shmem */
	ppg->arch_data = malloc( sizeof(struct unix_arch_data));
	ppg->pp_instances = wrs_shm_alloc(ppsi_head,
				     ppg->max_links * sizeof(*ppi));

	if ((!ppg->arch_data) || (!ppg->pp_instances)) {
		fprintf(stderr, "ppsi: out of memory\n");
		exit(1);
	}
	/* Set default configuration value for all instances */
	for (i = 0; i < ppg->max_links; i++) {
		memcpy(&INST(ppg, i)->cfg, &__pp_default_instance_cfg,sizeof(__pp_default_instance_cfg));
	}

	/* Set offset here, so config parsing can override it */
	if (adjtimex(&t) >= 0) {
		int *p;
		/*
		 * Our WRS kernel has tai support, but our compiler does not.
		 * We are 32-bit only, and we know for sure that tai is
		 * exactly after stbcnt. It's a bad hack, but it works
		 */
		p = (int *)(&t.stbcnt) + 1;
		ppg->timePropertiesDS->currentUtcOffset = (Integer16)*p;
	}

	if (pp_parse_cmdline(ppg, argc, argv) != 0)
		return -1;

	/* If no item has been parsed, provide a default file or string */
	if (ppg->cfg.cfg_items == 0)
		pp_config_file(ppg, 0, PP_DEFAULT_CONFIGFILE);
	if (ppg->cfg.cfg_items == 0) {
		/* Default configuration for switch is all ports - Priority given to HA */
		char s[128];

		for (i = 0; i < WRS_NUMBER_PHYSICAL_PORTS; i++) {
			Boolean configured=FALSE;
			if ( CONFIG_HAS_PROFILE_HA )
				sprintf(s, "port %i; iface wri%i; proto raw; profile ha", i + 1, i + 1);
			if ( CONFIG_HAS_PROFILE_WR ) {
				configured=TRUE;
				if ( ! configured )
					sprintf(s, "port %i; iface wri%i; proto raw; profile wr", i + 1, i + 1);
			}
			pp_config_string(ppg, s);
		}
	}
	for (i = 0; i < ppg->nlinks; i++) {
		ppi = INST(ppg, i);
		ppi->ch[PP_NP_EVT].fd = -1;
		ppi->ch[PP_NP_GEN].fd = -1;

		ppi->glbs = ppg;
		ppi->vlans_array_len = CONFIG_VLAN_ARRAY_SIZE;
		ppi->iface_name = ppi->cfg.iface_name;
		ppi->port_name = ppi->cfg.port_name;
		ppi->delayMechanism = ppi->cfg.delayMechanism;
		ppi->portDS = wrs_shm_alloc(ppsi_head, sizeof(*ppi->portDS));
		ppi->servo = wrs_shm_alloc(ppsi_head, sizeof(*ppi->servo));
		ppi->ext_hooks=&pp_hooks; /* Default value. Can be overwritten by an extension */
		ppi->ptp_support=TRUE;
		if (ppi->portDS) {
			switch (ppi->cfg.profile) {
			case PPSI_PROFILE_WR :
				if ( CONFIG_HAS_PROFILE_WR ) {
					ppi->protocol_extension=PPSI_EXT_WR;
					/* Add WR extension portDS */
					if ( !(ppi->portDS->ext_dsport =
							wrs_shm_alloc(ppsi_head, sizeof(struct wr_dsport))) ) {
							goto exit_out_of_memory;
					}

					/* Allocate WR data extension */
					if (! (ppi->ext_data = wrs_shm_alloc(ppsi_head,sizeof(struct wr_data))) ) {
						goto exit_out_of_memory;
					}
					/* Set WR extension hooks */
					ppi->ext_hooks=&wr_ext_hooks;
					ppi->cfg.egressLatency_ps=ppi->cfg.ingressLatency_ps=0; /* Forced to 0: Already taken into account in WR calculation */
				} else {
					fprintf(stderr, "ppsi: Profile WR not supported");
					exit(1);
				}
				break;
			case PPSI_PROFILE_HA :
				if ( CONFIG_HAS_PROFILE_HA ) {
					if ( !enable_l1Sync(ppi,TRUE) )
						goto exit_out_of_memory;
					/* Force mandatory attributes - Do not take care of the configuration */
					L1E_DSPOR_BS(ppi)->rxCoherentIsRequired =
							L1E_DSPOR_BS(ppi)->txCoherentIsRequired =
									L1E_DSPOR_BS(ppi)->congruentIsRequired=
											L1E_DSPOR_BS(ppi)->L1SyncEnabled=TRUE;
					L1E_DSPOR_BS(ppi)->optParamsEnabled=FALSE;
					enable_asymmetryCorrection(ppi,TRUE);
				}
				else {
					fprintf(stderr, "ppsi: Profile HA not supported");
					exit(1);
				}
				break;
			case PPSI_PROFILE_PTP :
				/* Do not take care of L1SYNC */
				enable_asymmetryCorrection(ppi,ppi->cfg.asymmetryCorrectionEnable);
				ppi->protocol_extension=PPSI_EXT_NONE;
				break;
			case PPSI_PROFILE_CUSTOM :
				if ( CONFIG_HAS_PROFILE_CUSTOM ) {
				ppi->protocol_extension=PPSI_EXT_NONE; /* can be changed ...*/
				if ( CONFIG_HAS_EXT_L1SYNC ) {
					if (ppi->cfg.l1SyncEnabled ) {
						if ( !enable_l1Sync(ppi,TRUE) )
							goto exit_out_of_memory;
						/* Read L1SYNC parameters */
						L1E_DSPOR_BS(ppi)->rxCoherentIsRequired =ppi->cfg.l1SyncRxCoherencyIsRequired;
						L1E_DSPOR_BS(ppi)->txCoherentIsRequired =ppi->cfg.l1SyncTxCoherencyIsRequired;
						L1E_DSPOR_BS(ppi)->congruentIsRequired =ppi->cfg.l1SyncCongruencyIsRequired;
						L1E_DSPOR_BS(ppi)->optParamsEnabled=ppi->cfg.l1SyncOptParamsEnabled;
						if ( L1E_DSPOR_BS(ppi)->optParamsEnabled ) {
							L1E_DSPOR_OP(ppi)->timestampsCorrectedTx=ppi->cfg.l1SyncOptParamsTimestampsCorrectedTx;
						}
					}
				}
				enable_asymmetryCorrection(ppi,ppi->cfg.asymmetryCorrectionEnable);
				} else {
					fprintf(stderr, "ppsi: Profile CUSTOM not supported");
					exit(1);
				}
				break;
			}
			/* Parameters profile independent */
			ppi->timestampCorrectionPortDS.egressLatency=picos_to_interval(ppi->cfg.egressLatency_ps);
			ppi->timestampCorrectionPortDS.ingressLatency=picos_to_interval(ppi->cfg.ingressLatency_ps);
			ppi->timestampCorrectionPortDS.messageTimestampPointLatency=0;
			ppi->portDS->masterOnly= ppi->cfg.masterOnly; /* can be overridden in pp_init_globals() */
		} else {
			goto exit_out_of_memory;
		}

		/* The following default names depend on TIME= at build time */
		ppi->n_ops = &DEFAULT_NET_OPS;
		ppi->t_ops = &DEFAULT_TIME_OPS;

		ppi->__tx_buffer = malloc(PP_MAX_FRAME_LENGTH);
		ppi->__rx_buffer = malloc(PP_MAX_FRAME_LENGTH);

		if (!ppi->__tx_buffer || !ppi->__rx_buffer) {
			goto exit_out_of_memory;
		}
	}

	pp_init_globals(ppg, &__pp_default_rt_opts);

	{
		timing_mode_t prev_timing_mode=WRH_OPER()->get_timing_mode(ppg);
		int nbRetry;
		int enablePPS;

		if ( ppg->defaultDS->clockQuality.clockClass == PP_PTP_CLASS_GM_LOCKED ) {
			if (prev_timing_mode==-1) {
				fprintf(stderr, "ppsi: Cannot get current timing mode\n");
				exit(1);
			}
			/* If read timing mode was GM, then we do not reprogram the hardware because it
			 * may unlock the PLL.
			 */
			if ( prev_timing_mode != TM_GRAND_MASTER ){
				/* Timing mode was not GM before */
				WRH_OPER()->set_timing_mode(ppg,TM_GRAND_MASTER);
				ppg->waitGmLocking=1; /* We might wait PPL locking ... see below */
			}
		} else {
			/* Timing mode will be set to BC when a port will become slave */
			WRH_OPER()->set_timing_mode(ppg,TM_FREE_MASTER);
		}
		/* Waiting for PLL locking. We do not need a precise time-out here */
		/* We are waiting up to 3s for PLL locking.
		 * We do that to avoid to jump too quickly to a degraded clock class.
		 */
		nbRetry=2;
		while(nbRetry>0) {
			if ( WRH_OPER()->get_timing_mode_state(ppg)==PP_TIMING_MODE_STATE_LOCKED )
				break;
			sleep(1); // wait 1s
			nbRetry--;
		} // if nbRetry>0 it means that the PLL is locked

		if ( (ppg->waitGmLocking=ppg->waitGmLocking && nbRetry==0)==1 ) {
			/* we degrade the clockClass to be sure that all instances will stay in
			 * initializing state until the clock class goes to PP_PTP_CLASS_GM_LOCKED
			 */
			//GDSDEF(ppg)->clockQuality.clockClass = PP_PTP_CLASS_GM_UNLOCKED;
		}

		/* Enable the PPS generation only if
		 * - Grand master and PLL is locked
		 * OR
		 * - Free running master (no condition required)
		 * OOR
		 * - Timing output is forced (for testing only)
		 */
		enablePPS=(nbRetry>0 && ppg->defaultDS->clockQuality.clockClass == PP_PTP_CLASS_GM_LOCKED) ||
				( ppg->defaultDS->clockQuality.clockClass == PP_PTP_CLASS_GM_UNLOCKED ||
						GOPTS(ppg)->forcePpsGen);
		WRH_OPER()->enable_timing_output(ppg,enablePPS);
	}

	seed = (unsigned long) time(NULL);
	if (getenv("PPSI_DROP_SEED"))
		seed = (unsigned long) atoi(getenv("PPSI_DROP_SEED"));
	ppsi_drop_init(ppg, seed);

	/* release lock from wrs_shm_get */
	wrs_shm_write(ppsi_head, WRS_SHM_WRITE_END);

	wrs_main_loop(ppg);
	return 0; /* never reached */

	exit_out_of_memory:;
	fprintf(stderr, "ppsi: out of memory\n");
	exit(1);
}
