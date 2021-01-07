/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Aurelio Colosimo <aurelio@aureliocolosimo.it>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <stdint.h>
#include <errno.h>
#include <ppsi/ppsi.h>
#include "wrpc.h"
#include <common-fun.h>

/* All of these live in wrpc-sw/include */
#include "dev/minic.h"
#include "dev/syscon.h"
#include "dev/endpoint.h"
#include "dev/pps_gen.h"
#include "dev/console.h"
#include "dev/rxts_calibrator.h"

#include "softpll_ng.h"

extern int32_t cal_phase_transition;
int wrc_set_timing_mode(struct pp_globals * ppg,wrh_timing_mode_t tm);
int wrc_get_timing_mode_state(struct pp_globals *ppg, wrh_timing_mode_pll_state_t *state);
int wrc_get_timing_mode(struct pp_globals *ppg,wrh_timing_mode_t *tm);

/* TODO: get rid of ptp_mode, use WRPC_ARCH_G(ppg)->timingMode instead */
int ptp_mode = WRC_MODE_UNKNOWN;
static int ptp_enabled = 0;

struct wrh_operations wrh_oper = {
	.locking_enable = wrpc_spll_locking_enable,
	.locking_poll = wrpc_spll_locking_poll,
	.locking_disable = wrpc_spll_locking_disable,
	.locking_reset = wrpc_spll_locking_reset,
	.enable_ptracker = wrpc_spll_enable_ptracker,

	.adjust_in_progress = wrpc_adjust_in_progress,
	.adjust_counters = wrpc_adjust_counters,
	.adjust_phase = wrpc_adjust_phase,

	.read_calib_data = wrpc_read_calibration_data,
	
	/* from wrs */
	.set_timing_mode = wrc_set_timing_mode,
	.get_timing_mode = wrc_get_timing_mode,
	.get_timing_mode_state = wrc_get_timing_mode_state,
};

/*ppi fields*/
static defaultDS_t  defaultDS;
static currentDS_t  currentDS;
static parentDS_t   parentDS;
static timePropertiesDS_t timePropertiesDS;
static struct pp_servo servo;


#if CONFIG_HAS_EXT_WR == 1
/* WR extension declaration */
#include "../proto-ext-whiterabbit/wr-api.h"
#include "../proto-ext-whiterabbit/wr-constants.h"

static struct wr_data wr_ext_data; /* WR extension data */

static struct wr_dsport wr_dsport;
#endif

static portDS_t     portDS ;

static int delay_ms = PP_DEFAULT_NEXT_DELAY_MS;
static int start_tics = 0;

struct pp_globals ppg_static; /* forward declaration */
struct pp_globals *ppg = &ppg_static;
static unsigned char __tx_buffer[PP_MAX_FRAME_LENGTH];
static unsigned char __rx_buffer[PP_MAX_FRAME_LENGTH];

/* despite the name, ppi_static is not static: tests/measure_t24p.c uses it */
struct pp_instance ppi_static = {
	.glbs			= &ppg_static,
	.portDS			= &portDS,
	.n_ops			= &wrpc_net_ops,
	.t_ops			= &wrpc_time_ops,
	.vlans_array_len	= CONFIG_VLAN_ARRAY_SIZE,
	.proto			= PP_DEFAULT_PROTO,
	.delayMechanism		= E2E, /* until changed by cfg */
	.iface_name		= "wr0",
	.port_name		= "wr0",
	.__tx_buffer		= __tx_buffer,
	.__rx_buffer		= __rx_buffer,
	.servo			= &servo,
	.ptp_support		= TRUE,
	.asymmetryCorrectionPortDS.enable = 1,
};

struct wrpc_arch_data_t wrpc_arch_data = {
	.timingMode = WRC_MODE_UNKNOWN
};

/* We now have a structure with all globals, and multiple ppi inside */
struct pp_globals ppg_static = {
	.pp_instances		= &ppi_static,
	.defaultDS		= &defaultDS,
	.currentDS		= &currentDS,
	.parentDS		= &parentDS,
	.timePropertiesDS	= &timePropertiesDS,
	.max_links		= PP_MAX_LINKS,
	.nlinks			= 1,
	/* rt_opts set by pp_init_globals */
	.arch_data = &wrpc_arch_data,
};


extern struct pp_ext_hooks  pp_hooks;

int wrc_ptp_init()
{
	struct pp_instance *ppi = &ppi_static;

	pp_printf("PPSi for WRPC. Commit %s, built on " __DATE__ "\n",
		PPSI_VERSION);
	pp_printf("ppg at %p\n", ppg);
	ppg->timePropertiesDS->currentUtcOffset = CONFIG_LEAP_SECONDS_VAL;

	/* copy default ppi config */
	memcpy(&ppi->cfg, &__pp_default_instance_cfg, sizeof(__pp_default_instance_cfg));
	ppi->ext_hooks =&pp_hooks; /* default value */
	if ( CONFIG_HAS_EXT_WR == 1 ) {
		ppi->protocol_extension = PPSI_EXT_WR;
		ppi->ext_hooks = &wr_ext_hooks;
		ppi->ext_data = &wr_ext_data;

		ppi->portDS->ext_dsport = &wr_dsport;
// FIXME:		enable_asymmetryCorrection(ppi,TRUE);
	}
	/* egressLatency and ingressLatency are overwritten on ptp_start */
// 	ppi->timestampCorrectionPortDS.egressLatency=picos_to_interval(ppi->cfg.egressLatency_ps);
// 	ppi->timestampCorrectionPortDS.ingressLatency=picos_to_interval(ppi->cfg.ingressLatency_ps);

	ppi->timestampCorrectionPortDS.messageTimestampPointLatency=0;
	ppi->portDS->masterOnly= ppi->cfg.masterOnly; /* can be overridden in pp_init_globals() */

	pp_init_globals(&ppg_static, &__pp_default_rt_opts);	

	pp_printf("Init done\n");

	return 0;
}

int wrc_set_timing_mode(struct pp_globals * ppg,wrh_timing_mode_t tm)
{
	/* pewnie wrc_ptp_set_mode */
	pp_printf("FIXME: %s not implemented yet\n", __func__);
}

int wrc_get_timing_mode_state(struct pp_globals *ppg, wrh_timing_mode_pll_state_t *state)
{
	pp_printf("FIXME: %s not implemented yet\n", __func__);
}

int wrc_get_timing_mode(struct pp_globals *ppg,wrh_timing_mode_t *tm)
{
	pp_printf("FIXME: %s not implemented yet\n", __func__);
	*tm = WRH_TM_BOUNDARY_CLOCK;
	return 0;
}

#define LOCK_TIMEOUT_FM (4 * TICS_PER_SECOND)
#define LOCK_TIMEOUT_GM (60 * TICS_PER_SECOND)

int wrc_ptp_set_mode(int mode)
{
	uint32_t start_tics, lock_timeout = 0;
	struct pp_instance *ppi = &ppi_static;
	struct pp_globals *ppg = ppi->glbs;
	struct wr_dsport *wrp = WR_DSPOR(ppi);
// 	typeof(ppg->rt_opts->clock_quality.clockClass) *class_ptr;
// 	typeof(ppg->rt_opts->clock_quality.clockAccuracy) *accuracy_ptr;
	int error = 0;
	/*
	 * We need to change the class in the default options.
	 * Unfortunately, ppg->rt_opts may be yet unassigned when this runs
	 */
// 	class_ptr = &__pp_default_rt_opts.clock_quality.clockClass;
// 	accuracy_ptr = &__pp_default_rt_opts.clock_quality.clockAccuracy;

	ptp_mode = 0;

	wrc_ptp_stop();

	switch (mode) {
	case WRC_MODE_GM:
	case WRC_MODE_ABSCAL: /* absolute calibration, gm-lookalike */
		wrp->wrConfig = WR_M_ONLY;
// 		ppi->role = PPSI_ROLE_MASTER;
// 		*class_ptr = PP_PTP_CLASS_GM_LOCKED;
// 		*accuracy_ptr = PP_PTP_ACCURACY_GM_LOCKED;
		spll_init(SPLL_MODE_GRAND_MASTER, 0, SPLL_FLAG_ALIGN_PPS);
		shw_pps_gen_unmask_output(1);
		lock_timeout = LOCK_TIMEOUT_GM;
// 		DSDEF(ppi)->clockQuality.clockClass = PP_PTP_CLASS_GM_LOCKED;
// 		DSDEF(ppi)->clockQuality.clockAccuracy = PP_PTP_ACCURACY_GM_LOCKED;
		bmc_m1(ppi);
		break;

	case WRC_MODE_MASTER:
		wrp->wrConfig = WR_M_ONLY;
// 		ppi->role = PPSI_ROLE_MASTER;
// 		*class_ptr = PP_PTP_CLASS_GM_UNLOCKED;
// 		*accuracy_ptr = PP_PTP_ACCURACY_GM_UNLOCKED;
		spll_init(SPLL_MODE_FREE_RUNNING_MASTER, 0, SPLL_FLAG_ALIGN_PPS);
		shw_pps_gen_unmask_output(1);
		lock_timeout = LOCK_TIMEOUT_FM;
// 		DSDEF(ppi)->clockQuality.clockClass = PP_PTP_CLASS_GM_UNLOCKED;
// 		DSDEF(ppi)->clockQuality.clockAccuracy = PP_PTP_ACCURACY_GM_UNLOCKED;
		bmc_m1(ppi);
		break;

	case WRC_MODE_SLAVE:
		wrp->wrConfig = WR_S_ONLY;
		// cfg needed?
		ppi->externalPortConfigurationPortDS.desiredState = ppi->cfg.desiredState = PPS_SLAVE;
		ppg->defaultDS->externalPortConfigurationEnabled = 1;
// 		ppi->role = PPSI_ROLE_SLAVE;
// // 		*class_ptr = PP_CLASS_SLAVE_ONLY;
// 		*accuracy_ptr = PP_ACCURACY_DEFAULT;
		/* skip initialization of spll for slave mode */
// 		pp_printf("ppsON\n");
		shw_pps_gen_unmask_output(0);
// 		shw_pps_gen_unmask_output(1);
// 		DSDEF(ppi)->clockQuality.clockClass = PP_CLASS_SLAVE_ONLY;
// 		DSDEF(ppi)->clockQuality.clockAccuracy = PP_ACCURACY_DEFAULT;
		break;
	}

	start_tics = timer_get_tics();

	pp_printf("Locking PLL");
// 	TOPS(ppi)->enable_timing_output(ppg,0); /* later, wr_init chooses */

	while (!spll_check_lock(0) && lock_timeout) {
		spll_update();
		timer_delay(TICS_PER_SECOND);
		if (timer_get_tics() - start_tics > lock_timeout) {
			pp_printf("\nLock timeout.");
			error = -ETIMEDOUT;
			break;
		}
		pp_printf(".");
	}
	pp_printf("\n");

	/* If we can't lock to the atomic/gps, we say it in the class */
	if (error && mode == WRC_MODE_GM) {
// 		*class_ptr = PP_PTP_CLASS_GM_UNLOCKED;
// 		*accuracy_ptr = PP_PTP_ACCURACY_GM_UNLOCKED;
	}

	ptp_mode = mode;
	/* Keep a copy of mode for dump */
	WRPC_ARCH_G(ppg)->timingMode = mode;
	return error;
}

int wrc_ptp_get_mode()
{
	return ptp_mode;
}

int wrc_ptp_sync_mech(int e2e_p2p_qry)
{
	struct pp_instance *ppi = &ppi_static;
	int running;
	pp_printf("%s set to %d ppi->delayMechanism %d\n", __func__, e2e_p2p_qry, ppi->delayMechanism);
	if (!CONFIG_HAS_P2P)
		return ppi->delayMechanism;

	switch(e2e_p2p_qry) {
	case E2E:
	case P2P:
		running = wrc_ptp_run(-1);
		wrc_ptp_run(0);
		ppi->delayMechanism = e2e_p2p_qry;
		wrc_ptp_run(running);
		return 0;
	default:
		return ppi->delayMechanism;
	}
}

int wrc_ptp_start()
{
	struct pp_instance *ppi = &ppi_static;
	TimeInterval scaledBitSlide = 0;
	RelativeDifference scaledDelayCoefficient = 0;
	TimeInterval scaledSfpDeltaTx = 0;
	TimeInterval scaledSfpDeltaRx = 0;

	pp_printf("PTP Start!\n");

	/* sfp match was done before so read calibration data */

	if ( wrpc_read_calibration_data(ppi,NULL,
			&scaledBitSlide,
			&scaledDelayCoefficient,
			&scaledSfpDeltaTx,
			&scaledSfpDeltaRx)!= WRH_HW_CALIB_OK ) {
		pp_diag(ppi, fsm, 1, "Cannot get calibration values (bitslide, alpha, TX/Rx delays\n");
	}
	ppi->timestampCorrectionPortDS.semistaticLatency = scaledBitSlide;
	if (scaledDelayCoefficient>=PP_MIN_DELAY_COEFFICIENT_AS_RELDIFF
	    && scaledDelayCoefficient<=PP_MAX_DELAY_COEFFICIENT_AS_RELDIFF ) {
		/* Scaled delay coefficient is valid then delta tx and rx also */
		if ( ppi->asymmetryCorrectionPortDS.enable ) {
			pp_printf("ppi->asymmetryCorrectionPortDS.enable\n");
			ppi->asymmetryCorrectionPortDS.scaledDelayCoefficient = ppi->cfg.scaledDelayCoefficient = scaledDelayCoefficient;
			/* taken from: enable_asymmetryCorrection(ppi,TRUE); */
			ppi->portDS->delayAsymCoeff=pp_servo_calculateDelayAsymCoefficient(ppi->asymmetryCorrectionPortDS.scaledDelayCoefficient);
			ppi->asymmetryCorrectionPortDS.constantAsymmetry = picos_to_interval(ppi->cfg.constantAsymmetry_ps); /* needed? */

		}
		ppi->timestampCorrectionPortDS.egressLatency =
			picos_to_interval(ppi->cfg.egressLatency_ps) + scaledSfpDeltaTx;
		ppi->timestampCorrectionPortDS.ingressLatency =
			picos_to_interval(ppi->cfg.ingressLatency_ps) + scaledSfpDeltaRx;
		
		pp_printf("scaledDelayCoefficient ok\n");
	}
	else
	    pp_printf("scaledDelayCoefficient NOK\n");
	pp_printf("scaledDelayCoefficient %s\n", print64(scaledDelayCoefficient,0));
	/* Call the state machine. Being it in "Initializing" state, make
	 * ppsi initialize what is necessary */
	delay_ms = pp_state_machine(ppi, NULL, 0);
	start_tics = timer_get_tics();

	/* just tell that the link is up, if not it will anyhow not receive anything */
	ppi->link_up = TRUE;
	ppi->state = PPS_INITIALIZING;
	wr_servo_reset(ppi);
	ptp_enabled = 1;
	return 0;
}

int wrc_ptp_stop()
{
	pp_printf("PTP Stop!\n");
	struct pp_instance *ppi = &ppi_static;
	struct wr_dsport *wrp = WR_DSPOR(ppi);

// 	TOPS(ppi)->enable_timing_output(ppg,0);
	/* Moving fiber: forget about this parent (FIXME: shouldn't be here) */
	wrp->parentWrConfig = wrp->parentWrModeOn = 0;
	memset(ppi->frgn_master, 0, sizeof(ppi->frgn_master));
	ppi->frgn_rec_num = 0;          /* no known master */
	
	ppi->frgn_rec_best = -1;

	/* just tell that the link is down now */
	ppi->link_up = FALSE;
	ptp_enabled = 0;
	ppi->next_state = PPS_DISABLED;
	pp_leave_current_state(ppi);
	ppi->n_ops->exit(ppi);
	
	if( ppi->ext_hooks->servo_reset)
		(*ppi->ext_hooks->servo_reset)(ppi);
	/* FIXME: this should be done in a different place and in a nicer way.
	    This dirty hack was introduce to force re-doing of WR Link Setup
	    when a link goes down and then up. */
	if (ppi->ext_data)
		WRH_SRV(ppi)->doRestart = TRUE;
	pp_close_globals(&ppg_static);

	return 0;
}

int wrc_ptp_run(int start_stop_query)
{
	switch(start_stop_query) {
	case 0:
		return wrc_ptp_stop();
	case 1:
		return wrc_ptp_start();
	default:
		return ptp_enabled;
	}
}

/* this returns whether or not the function did any work */
int wrc_ptp_update()
{
	int i;
	struct pp_instance *ppi = &ppi_static;

	if (!ptp_enabled)
		return 0;

	i = __recv_and_count(ppi, ppi->rx_frame, PP_MAX_FRAME_LENGTH - 4,
			     &ppi->last_rcv_time);

	if ((!i) && (timer_get_tics() - start_tics < delay_ms))
		return 0;

	if (!i) {
		/* Nothing received, but timeout elapsed */
		start_tics = timer_get_tics();
		delay_ms = pp_state_machine(ppi, NULL, 0);
		return 1;
	}
	delay_ms = pp_state_machine(ppi, ppi->rx_ptp, i);
	return 1;
}
