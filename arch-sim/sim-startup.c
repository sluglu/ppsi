/*
 * Copyright (C) 2013 CERN (www.cern.ch)
 * Author: Pietro Fezzardi (pietrofezzardi@gmail.com)
 *
 * Released to the public domain
 */

#include <stdio.h>

#include "generated/autoconf.h"
#include <ppsi/ppsi.h>
#include "ppsi-sim.h"

#if CONFIG_HAS_EXT_WR == 1
/* WR extension declaration */
#include "../proto-ext-whiterabbit/wr-api.h"
#include "../proto-ext-whiterabbit/wr-constants.h"
#endif

static struct pp_runtime_opts sim_master_rt_opts = {
/*	.clock_quality = {
			.clockClass = PP_PTP_CLASS_GM_LOCKED,
			.clockAccuracy = PP_ACCURACY_DEFAULT,
			.offsetScaledLogVariance = PP_VARIANCE_DEFAULT,
	},
*/
        .flags =		PP_DEFAULT_FLAGS,
	.ap =			PP_DEFAULT_AP,
	.ai =			PP_DEFAULT_AI,
	.s =			PP_DEFAULT_DELAY_S,
/*	.logAnnounceInterval =	PP_DEFAULT_ANNOUNCE_INTERVAL,
	.logSyncInterval =		PP_DEFAULT_SYNC_INTERVAL,
*/
	.priority1 =		PP_DEFAULT_PRIORITY1,
	.priority2 =		PP_DEFAULT_PRIORITY2,
	.domainNumber =	PP_DEFAULT_DOMAIN_NUMBER,
	.ttl =			PP_DEFAULT_TTL,
};

extern struct pp_ext_hooks const pp_hooks;

#if CONFIG_HAS_EXT_WR == 1

static int sim_read_calib_data(struct pp_instance *ppi,
			       int32_t *clock_period,
			       TimeInterval *scaledBitSlide,
			       RelativeDifference *scaledDelayCoefficient,
			       TimeInterval *scaledSfpDeltaTx,
			       TimeInterval *scaledSfpDeltaRx)
{
	if (scaledDelayCoefficient)
		abort();

	if (scaledBitSlide)
		abort();

	if (clock_period)
		*clock_period = 8000; /* in ps */

	if (scaledSfpDeltaTx)
		abort();

	if (scaledSfpDeltaRx)
		abort();

	return WRH_HW_CALIB_OK;
}

static int sim_adjust_phase(int32_t phase_ps)
{
	return WRH_SPLL_OK;
}

static int sim_locking_enable(struct pp_instance *ppi)
{
	return WRH_SPLL_OK;
}

static int sim_locking_poll(struct pp_instance *ppi)
{
	/* Perfect world */
	return WRH_SPLL_LOCKED;
}

static int sim_locking_reset(struct pp_instance *ppi)
{
	return WRH_SPLL_OK;
}

static int sim_enable_ptracker(struct pp_instance *ppi)
{
	return WRH_SPLL_OK;
}

static int sim_adjust_in_progress(void)
{
	return 0;
}

static int sim_adjust_counters(int64_t adjust_sec, int32_t adjust_nsec)
{
	return 0;
}

const struct wrh_operations wrh_oper = {
	.locking_enable = sim_locking_enable,
	.locking_poll = sim_locking_poll,
	.locking_disable = NULL,
	.locking_reset = sim_locking_reset,
	.enable_ptracker = sim_enable_ptracker,

	.adjust_in_progress = sim_adjust_in_progress,
	.adjust_counters = sim_adjust_counters,
	.adjust_phase = sim_adjust_phase,

	.read_calib_data = sim_read_calib_data,

	/* not used */
	.set_timing_mode = NULL,
	.get_timing_mode = NULL,
	.get_timing_mode_state = NULL,
};
#endif

/*
 * In arch-sim we use two pp_instances in the same pp_globals to represent
 * two different machines. This means *completely differnt* machines, with
 * their own Data Sets. Given we can't put more all the different Data Sets
 * in the same ppg, we stored them in the ppi->arch_data of every instance.
 * This function is used to set the inner Data Sets pointer of the ppg to
 * point to the Data Sets related to the pp_instange passed as argument
 */
int sim_set_global_DS(struct pp_instance *ppi)
{
	struct sim_ppi_arch_data *data = SIM_PPI_ARCH(ppi);

	ppi->glbs->defaultDS = data->defaultDS;
	ppi->glbs->currentDS = data->currentDS;
	ppi->glbs->parentDS = data->parentDS;
	ppi->glbs->timePropertiesDS = data->timePropertiesDS;
//	ppi->glbs->servo = data->servo;
	ppi->glbs->rt_opts = data->rt_opts;

	return 0;
}

static int sim_ppi_init(struct pp_instance *ppi, int which_ppi)
{
	struct sim_ppi_arch_data *data;

	memcpy(&ppi->cfg, &__pp_default_instance_cfg, sizeof(__pp_default_instance_cfg));

	ppi->proto = PP_DEFAULT_PROTO;
	ppi->__tx_buffer = malloc(PP_MAX_FRAME_LENGTH);
	ppi->__rx_buffer = malloc(PP_MAX_FRAME_LENGTH);
	ppi->arch_data = calloc(1, sizeof(struct sim_ppi_arch_data));
	ppi->portDS = calloc(1, sizeof(*ppi->portDS));
	if ((!ppi->arch_data) || (!ppi->portDS))
		return -1;

	ppi->ext_hooks=&pp_hooks;

#if CONFIG_HAS_EXT_WR
	ppi->protocol_extension = PPSI_EXT_WR;
	ppi->ext_hooks = &wr_ext_hooks;

	ppi->ext_data = calloc(1, sizeof (struct wr_data));
	ppi->portDS->ext_dsport = calloc(1, sizeof(struct wr_dsport));
	if (!ppi->ext_data || !ppi->portDS->ext_dsport)
		return -1;
#endif

	data = SIM_PPI_ARCH(ppi);
	data->defaultDS = calloc(1, sizeof(*data->defaultDS));
	data->currentDS = calloc(1, sizeof(*data->currentDS));
	data->parentDS = calloc(1, sizeof(*data->parentDS));
	data->timePropertiesDS = calloc(1, sizeof(*data->timePropertiesDS));
	data->servo = calloc(1, sizeof(*data->servo));
	if ((!data->defaultDS) ||
			(!data->currentDS) ||
			(!data->parentDS) ||
			(!data->timePropertiesDS) ||
			(!data->servo))
		return -1;
	if (which_ppi == SIM_MASTER)
		data->rt_opts = &sim_master_rt_opts;
	else
		data->rt_opts = &__pp_default_rt_opts;
	data->other_ppi = INST(ppi->glbs, -(which_ppi - 1));
	return 0;
}

int main(int argc, char **argv)
{
	struct pp_globals *ppg;
	struct pp_instance *ppi;
	int i;

	setbuf(stdout, NULL);
	pp_printf("PPSi. Commit %s, built on " __DATE__ "\n", PPSI_VERSION);

	ppg = calloc(1, sizeof(struct pp_globals));
	ppg->max_links = 2; // master and slave, nothing else
	ppg->arch_data = calloc(1, sizeof(struct sim_ppg_arch_data));
	ppg->pp_instances = calloc(ppg->max_links, sizeof(struct pp_instance));

	if ((!ppg->arch_data) || (!ppg->pp_instances))
		return -1;

	/* Alloc data stuctures inside the pp_instances */
	for (i = 0; i < ppg->max_links; i++) {
		ppi = INST(ppg, i);
		ppi->glbs = ppg; // must be done before using sim_set_global_DS
		ppi->vlans_array_len = CONFIG_VLAN_ARRAY_SIZE;
		ppi->servo = calloc(1, sizeof (struct pp_servo));
		if (ppi->servo == NULL)
			return -1;
		if (sim_ppi_init(ppi, i))
			return -1;
	}

	/*
	 * Configure the master with standard configuration, only from default
	 * string. The master is not configurable, but there's no need to do
	 * it cause we are ok with a standard one. We just want to see the
	 * behaviour of the slave.
	 * NOTE: the master instance is initialized before parsing the command
	 * line, so the diagnostics cannot be enabled here. We cannot put the
	 * master config later because the initial time for the master is needed
	 * to set the initial offset for the slave
	 */
	sim_set_global_DS(pp_sim_get_master(ppg));

	pp_config_string(ppg, strdup("port SIM_MASTER; iface MASTER;"
					"proto udp;"
					"sim_iter_max 10000;"
					"sim_init_master_time .9;"));

	/* parse commandline for configuration options */
	sim_set_global_DS(pp_sim_get_slave(ppg));
	if (pp_parse_cmdline(ppg, argc, argv) != 0)
		return -1;
	/* If no item has been parsed, provide default file or string */
	if (ppg->cfg.cfg_items == 0)
		pp_config_file(ppg, 0, PP_DEFAULT_CONFIGFILE);
	if (ppg->cfg.cfg_items == 0)
		pp_config_string(ppg, strdup("port SIM_SLAVE; iface SLAVE;"
						"proto udp;"));

	for (i = 0; i < ppg->nlinks; i++) {
		ppi = INST(ppg, i);
		sim_set_global_DS(ppi);
		ppi->iface_name = ppi->cfg.iface_name;
		ppi->port_name = ppi->cfg.port_name;
		ppi->delayMechanism = ppi->cfg.delayMechanism;
		if (ppi->proto == PPSI_PROTO_RAW)
			pp_printf("Warning: simulator doesn't support raw "
					"ethernet. Using UDP\n");
		ppi->ch[PP_NP_GEN].fd = -1;
		ppi->ch[PP_NP_EVT].fd = -1;
		ppi->t_ops = &DEFAULT_TIME_OPS;
		ppi->n_ops = &DEFAULT_NET_OPS;

		ppi->portDS->logAnnounceInterval = PP_DEFAULT_ANNOUNCE_INTERVAL;

		if (pp_sim_is_master(ppi))
			pp_init_globals(ppg, &sim_master_rt_opts);
		else
			pp_init_globals(ppg, &__pp_default_rt_opts);
	}

	sim_main_loop(ppg);
	return 0;
}
