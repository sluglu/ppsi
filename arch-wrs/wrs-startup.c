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

#include "../proto-ext-whiterabbit/wr-api.h"
#include "../proto-ext-l1sync/l1e-api.h"

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
	.read_corr_data = wrs_read_correction_data,
	.read_calib_data = wrs_read_calibration_data,
	.calib_disable = wrs_calibrating_disable,
	.calib_enable = wrs_calibrating_enable,
	.calib_poll = wrs_calibrating_poll,
	.calib_pattern_enable = wrs_calibration_pattern_enable,
	.calib_pattern_disable = wrs_calibration_pattern_disable,

	.enable_timing_output = wrs_enable_timing_output,
};

struct minipc_ch *hal_ch;
struct minipc_ch *ppsi_ch;
struct hal_port_state *hal_ports;
int hal_nports;
struct wrs_shm_head *ppsi_head;

extern struct pp_ext_hooks  pp_hooks;

/*
 * we need to call calloc, to reset all stuff that used to be static,
 * but we'd better have a simple prototype, compatilble with wrs_shm_alloc()
 */
static void *local_malloc(struct wrs_shm_head *headptr, size_t size)
{
	void *retval = malloc(size);

	if (retval)
		memset(retval, 0, size);
	return retval;
}

int main(int argc, char **argv)
{
	struct pp_globals *ppg;
	struct pp_instance *ppi;
	unsigned long seed;
	struct timex t;
	int i, hal_retries;
	struct wrs_shm_head *hal_head;
	struct hal_shmem_header *h;
	void *(*alloc_fn)(struct wrs_shm_head *headptr, size_t size);
	alloc_fn = local_malloc;

	setbuf(stdout, NULL);

	pp_printf("PPSi. Commit %s, built on " __DATE__ "\n",
		PPSI_VERSION);

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
	if ((ppsi_head->pid != 0) && (kill(ppsi_head->pid, 0) == 0)) {
		wrs_shm_put(ppsi_head);
		pp_printf("Fatal: There is another PPSi instance running. "
			  "Exit...\n\n");
		exit(1);
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
	alloc_fn = wrs_shm_alloc;
	ppsi_head->version = WRS_PPSI_SHMEM_VERSION;

	ppg = alloc_fn(ppsi_head, sizeof(*ppg));
	ppg->defaultDS = alloc_fn(ppsi_head, sizeof(*ppg->defaultDS));
	ppg->currentDS = alloc_fn(ppsi_head, sizeof(*ppg->currentDS));
	ppg->parentDS =  alloc_fn(ppsi_head, sizeof(*ppg->parentDS));
	ppg->timePropertiesDS = alloc_fn(ppsi_head,sizeof(*ppg->timePropertiesDS));
	ppg->rt_opts = &__pp_default_rt_opts;

	ppg->max_links = PP_MAX_LINKS;

	/* NOTE: arch_data is not in shmem */
	ppg->arch_data = malloc( sizeof(struct unix_arch_data));
	ppg->pp_instances = alloc_fn(ppsi_head,
				     ppg->max_links * sizeof(*ppi));

	if ((!ppg->arch_data) || (!ppg->pp_instances)) {
		fprintf(stderr, "ppsi: out of memory\n");
		exit(1);
	}
	/* Set default configuration value for all instances */
	for (i = 0; i < ppg->max_links; i++) {
		memcpy(INST(ppg, i), &__pp_default_instance_cfg,sizeof(__pp_default_instance_cfg));
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
		ppg->timePropertiesDS->currentUtcOffset = *p;
	}

	if (pp_parse_cmdline(ppg, argc, argv) != 0)
		return -1;

	/* If no item has been parsed, provide a default file or string */
	if (ppg->cfg.cfg_items == 0)
		pp_config_file(ppg, 0, PP_DEFAULT_CONFIGFILE);
	if (ppg->cfg.cfg_items == 0) {
		/* Default configuration for switch is all ports - Priority given to HA */
		char s[128];
		int i;

		for (i = 0; i < 18; i++) {
			Boolean configured=FALSE;
#if CONFIG_EXT_L1SYNC == 1
			sprintf(s, "port %i; iface wri%i; proto raw;"
				"profile ha; role auto", i + 1, i + 1);
			configured=TRUE;
#endif
#if CONFIG_EXT_WR == 1
			if ( ! configured )
				sprintf(s, "port %i; iface wri%i; proto raw;"
					"profile wr; role auto", i + 1, i + 1);
#endif
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
		ppi->portDS = alloc_fn(ppsi_head, sizeof(*ppi->portDS));
		ppi->ext_hooks=&pp_hooks; /* Default value. Can be overwritten by an extension */
		if (ppi->portDS) {
#if CONFIG_EXT_WR == 1
			if ( ppi->cfg.profile==PPSI_PROFILE_WR ) {
				ppi->protocol_extension=PPSI_EXT_WR;
				/* Add WR extension portDS */
				if ( !(ppi->portDS->ext_dsport =
						alloc_fn(ppsi_head, sizeof(struct wr_dsport))) ) {
						goto exit_out_of_memory;
				}

				/* Allocate WR data extension */
				if (! (ppi->ext_data = alloc_fn(ppsi_head,sizeof(struct wr_data))) ) {
					goto exit_out_of_memory;
				}
				/* Set WR extension hooks */
				ppi->ext_hooks=&wr_ext_hooks;
			}
#endif
#if CONFIG_EXT_L1SYNC == 1
			if ( ppi->cfg.profile==PPSI_PROFILE_HA ) {
				ppi->protocol_extension=PPSI_EXT_L1S;
				/* Add L1E extension portDS */
				if ( !(ppi->portDS->ext_dsport =alloc_fn(ppsi_head, sizeof(l1e_ext_portDS_t))) ) {
					goto exit_out_of_memory;
				}

				/* Allocate WR data extension */
				if (! (ppi->ext_data = alloc_fn(ppsi_head,sizeof(struct l1e_data))) ) {
					goto exit_out_of_memory;
				}
				/* Set ingress/egress latencies */
				ppi->timestampCorrectionPortDS.egressLatency=picos_to_interval(ppi->cfg.egressLatency_ps);
				ppi->timestampCorrectionPortDS.ingressLatency=picos_to_interval(ppi->cfg.ingressLatency_ps);
				ppi->timestampCorrectionPortDS.messageTimestampPointLatency=0;
				ppi->asymmetryCorrectionPortDS.constantAsymmetry=picos_to_interval(ppi->cfg.constantAsymmetry_ps);
				ppi->asymmetryCorrectionPortDS.scaledDelayCoefficient=
						(RelativeDifference)(ppi->cfg.delayCoefficient * (double)pow(2.0, REL_DIFF_FRACBITS_AS_FLOAT));
				/* Set L1SYNC extension hooks */
				ppi->ext_hooks=&l1e_ext_hooks;
				/* Set default profile parameters */
				ppg->defaultDS->externalPortConfigurationEnabled = 1;
				ppi->portDS->masterOnly = 0;
			}
#endif
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

	seed = time(NULL);
	if (getenv("PPSI_DROP_SEED"))
		seed = atoi(getenv("PPSI_DROP_SEED"));
	ppsi_drop_init(ppg, seed);

	/* release lock from wrs_shm_get */
	wrs_shm_write(ppsi_head, WRS_SHM_WRITE_END);

	wrs_main_loop(ppg);
	return 0; /* never reached */

	exit_out_of_memory:;
	fprintf(stderr, "ppsi: out of memory\n");
	exit(1);

}
