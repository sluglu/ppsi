/*
 * Copyright (C) 2018 CERN (www.cern.ch)
 * Author: Jean-Claude BAU & Maciej Lipinski
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */

#include <inttypes.h>
#include <ppsi/ppsi.h>
#include <common-fun.h>
#include "l1e-constants.h"
#include <math.h>

char *l1e_state_name[] = {
	[L1SYNC_DISABLED]	= "L1SYNC_DISABLED",
	[L1SYNC_IDLE]		= "L1SYNC_IDLE",
	[L1SYNC_LINK_ALIVE]	= "L1SYNC_LINK_ALIVE",
	[L1SYNC_CONFIG_MATCH]	= "L1SYNC_CONFIG_MATCH",
	[L1SYNC_UP]		= "L1SYNC_UP",
};


void l1e_print_L1Sync_basic_bitmaps(struct pp_instance *ppi, uint8_t configed,
					uint8_t active, char* text)
{

	pp_diag(ppi, ext, 2, "ML: L1Sync %s\n", text);
	pp_diag(ppi, ext, 2, "ML: \tConfig: TxC=%d RxC=%d Cong=%d Param=%d\n",
		  ((configed & L1E_TX_COHERENT) == L1E_TX_COHERENT),
		  ((configed & L1E_RX_COHERENT) == L1E_RX_COHERENT),
		  ((configed & L1E_CONGRUENT)   == L1E_CONGRUENT),
		  ((configed & L1E_OPT_PARAMS)  == L1E_OPT_PARAMS));
	pp_diag(ppi, ext, 2, "ML: \tActive: TxC=%d RxC=%d Cong=%d\n",
		  ((active & L1E_TX_COHERENT)   == L1E_TX_COHERENT),
		  ((active & L1E_RX_COHERENT)   == L1E_RX_COHERENT),
		  ((active & L1E_CONGRUENT)     == L1E_CONGRUENT));
}

/* update DS values of latencies and delay coefficient
 * - these values are provided by HW (i.e. HAL) depending on SFPs, wavelenghts, etc
 * - these values are stored in configurable data sets
 * - the values from data sets are used in calculations
 */
int l1e_update_correction_values(struct pp_instance *ppi)
{

	struct l1e_servo_state *s = L1E_SRV(ppi);
	pp_diag(ppi, ext, 2, "hook: %s -- ext %i\n", __func__, ppi->protocol_extension);


	/* read the interesting values from HW (i.e. HAL)*/
	if ( WRH_OPER()->read_corr_data(ppi,
		NULL,
		&s->clock_period_ps,
		NULL) != WRH_HW_CALIB_OK){
		      pp_diag(ppi, ext, 2, "hook: %s -- cannot read calib values\n",
			__func__);
		return -1;
	}
	pp_diag(ppi, ext, 2, "ML- Updated correction values: Clock period=%d [ps]",
		s->clock_period_ps);

	return 0;
}

/* open is global; called from "pp_init_globals" */
static int l1e_open(struct pp_instance *ppi, struct pp_runtime_opts *rt_opts)
{
	pp_diag(NULL, ext, 2, "hook: %s -- ext %i\n", __func__,
		ppi->protocol_extension);
	return 0;
}

/* initialize one specific port */
static int l1e_init(struct pp_instance *ppi, void *buf, int len)
{
	L1SyncBasicPortDS_t * bds=L1E_DSPOR_BS(ppi);

	pp_diag(ppi, ext, 2, "hook: %s -- ext %i\n", __func__,
		ppi->protocol_extension);
	// init configurable data set members with proper config values
	bds->L1SyncEnabled            = TRUE;
	bds->txCoherentIsRequired     = TRUE;
	bds->rxCoherentIsRequired     = TRUE;
	bds->congruentIsRequired      = TRUE;
	bds->optParamsEnabled         = FALSE;
	// init dynamic data set members with zeros/defaults
	bds->L1SyncLinkAlive          = FALSE;
	bds->isTxCoherent             = FALSE;
	bds->isRxCoherent             = FALSE;
	bds->isCongruent              = FALSE;
	bds->L1SyncState              = L1SYNC_DISABLED;
	bds->peerTxCoherentIsRequired = FALSE;
	bds->peerRxCoherentIsRequired = FALSE;
	bds->peerCongruentIsRequired  = FALSE;
	bds->peerIsTxCoherent     	  = FALSE;
	bds->peerIsRxCoherent     	  = FALSE;
	bds->peerIsCongruent          = FALSE;
	/* Init  configurable parameters */
	bds->logL1SyncInterval=ppi->cfg.l1sync_interval;
	bds->L1SyncReceiptTimeout=ppi->cfg.l1sync_receipt_timeout;

	/* Init other specific members */
	bds->next_state=bds->L1SyncState;

	/* Init configuration members of L1SyncOptParamsPortDS */
	L1E_DSPOR_OP(ppi)->timestampsCorrectedTx=FALSE;

	return 0;
}

/* This hook is called whenever a signaling message is received */
static int l1e_handle_signaling(struct pp_instance * ppi, void *buf, int len)
{
	L1SyncBasicPortDS_t * bds=L1E_DSPOR_BS(ppi);
	pp_diag(ppi, ext, 2, "hook: %s (%i:%s) -- plen %i\n", __func__,
		ppi->state, l1e_state_name[bds->L1SyncState], len);

	if ( l1e_unpack_signal(ppi, buf, len)==0 ) {
		/* Valid Sync message */
		int timeout_tx_sync;

		/* Reset reception timeout */
		timeout_tx_sync = (4 << (bds->logL1SyncInterval + 8)) * bds->L1SyncReceiptTimeout;
		__pp_timeout_set(ppi, L1E_TIMEOUT_RX_SYNC, timeout_tx_sync);

		bds->L1SyncLinkAlive = TRUE;
	}
	return 0;
}

uint8_t l1e_creat_L1Sync_bitmask(int tx_coh, int rx_coh, int congru)
{
	uint8_t outputMask=0;
	if(tx_coh) outputMask |= L1E_TX_COHERENT;
	if(rx_coh) outputMask |= L1E_RX_COHERENT;
	if(congru) outputMask |= L1E_CONGRUENT;
	return outputMask;
}


static int l1e_handle_resp(struct pp_instance *ppi)
{
	struct pp_time *ofm = &SRV(ppi)->offsetFromMaster;
	l1e_ext_portDS_t *l1epds = L1E_DSPOR(ppi);

	pp_diag(ppi, ext, 2, "hook: %s\n", __func__);

	if ( l1epds->basic.L1SyncState != L1SYNC_UP )
		return 0;

	/* This correction_field we received is already part of t4 */

	/*
	 * If no WR mode is on, run normal code, if T2/T3 are valid.
	 * After we adjusted the pps counter, stamps are invalid, so
	 * we'll have the Unix time instead, marked by "correct"
	 */
	if (!l1epds->head.extModeOn) {
		if (is_incorrect(&ppi->t2) || is_incorrect(&ppi->t3)) {
			pp_diag(ppi, servo, 1,
				"T2 or T3 incorrect, discarding tuple\n");
			return 0;
		}
		pp_servo_got_resp(ppi);
		/*
		 * pps always on if offset less than 1 second,
		 * until we have a configurable threshold */
		WRH_OPER()->enable_timing_output(ppi, ofm->secs==0);

	}
	l1e_servo_got_delay(ppi);
	l1e_servo_update(ppi);
	return 0;
}

/* Hmm... "execute_slave" should look for errors; but it's off in WR too */
static int l1e_handle_followup(struct pp_instance *ppi,
			      struct pp_time *t1)
{
	l1e_ext_portDS_t *pds=L1E_DSPOR(ppi);

	pp_diag(ppi, ext, 2, "hook: %s\n", __func__);
	if ((pds->basic.L1SyncState  != L1SYNC_UP) || !pds->head.extModeOn )
		return 0;

	l1e_servo_got_sync(ppi, t1, &ppi->t2);

	if (CONFIG_HAS_P2P && ppi->delayMechanism == P2P)
		l1e_servo_update(ppi);

	return 1; /* the caller returns too */
}

static __attribute__((used)) int l1e_handle_presp(struct pp_instance *ppi)
{
	struct pp_time *ofm = &SRV(ppi)->offsetFromMaster;
	l1e_ext_portDS_t *pds=L1E_DSPOR(ppi);


	/* Servo is active only when the state is UP */
	if ( pds->basic.L1SyncState != L1SYNC_UP )
		return 0;

	/*
	 * If no WR mode is on, run normal code, if T2/T3 are valid.
	 * After we adjusted the pps counter, stamps are invalid, so
	 * we'll have the Unix time instead, marked by "correct"
	 */

	if (!WRH_DSPOR_HEAD(ppi)->extModeOn) {
		if (is_incorrect(&ppi->t3) || is_incorrect(&ppi->t6)) {
			pp_diag(ppi, servo, 1,
				"T3 or T6 incorrect, discarding tuple\n");
			return 0;
		}
		pp_servo_got_presp(ppi);
		/*
		 * pps always on if offset less than 1 second,
		 * until ve have a configurable threshold */
		WRH_OPER()->enable_timing_output(ppi, ofm->secs==0);

		return 0;
	}

	/* FIXME: verify that last-received cField is already accounted for */
	l1e_servo_got_delay(ppi);
	return 0;
}

/* Check if ready to go to SLAVE state */
static int l1e_ready_for_slave(struct pp_instance *ppi)
{
	pp_diag(ppi, ext, 2, "hook: %s\n", __func__);

	/* Current state is UNCALIBRATED. Can we go to SLAVE state ? */
	/* YES if L1SYNC state is UP. It means that the PPL is locked*/
	if ( L1E_DSPOR_BS(ppi)->L1SyncState!=L1SYNC_UP &&
		 L1E_DSPOR_BS(ppi)->L1SyncEnabled == TRUE) {
		return 0; /* Not ready */
	}
	return 1; /* Ready for slave */
}


static 	void l1e_state_change(struct pp_instance *ppi) {
	switch (ppi->next_state) {
		case PPS_DISABLED :
			/* In PPSI we go to DISABLE state when the link is down */
			/* For the time being, it should be done like this because fsm is not called when the link is down */
			l1e_run_state_machine(ppi); /* First call to choose the next state */
			l1e_run_state_machine(ppi); /* Second call to apply next state */
			break;
		case PPS_INITIALIZING :
			L1E_DSPOR(ppi)->basic.L1SyncState=L1E_DSPOR(ppi)->basic.next_state=L1SYNC_DISABLED;
			break;
	}
}

/* The global structure used by ppsi */
struct pp_ext_hooks l1e_ext_hooks = {
	.open = l1e_open,
	.init = l1e_init,
	.handle_signaling = l1e_handle_signaling,
	.run_ext_state_machine = l1e_run_state_machine,
	.ready_for_slave = l1e_ready_for_slave,
	.handle_resp = l1e_handle_resp,
	.handle_followup = l1e_handle_followup,
#if CONFIG_HAS_P2P
	.handle_presp = l1e_handle_presp,
#endif
	.state_change = l1e_state_change,
};

