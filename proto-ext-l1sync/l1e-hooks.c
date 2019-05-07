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

int l1eTmoTxSync=0;
int l1eTmoRxSync=0;

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

	if ( l1eTmoTxSync==0) {
		l1eTmoTxSync=pp_timeout_get_timer(ppi,"L1E_TX_SYNC",TO_RAND_NONE, TMO_CF_INSTANCE_DEPENDENT);
	}

	if ( l1eTmoRxSync==0)
		l1eTmoRxSync=pp_timeout_get_timer(ppi,"L1E_RX_SYNC",TO_RAND_NONE, TMO_CF_INSTANCE_DEPENDENT);

	pp_timeout_set(ppi, L1E_TIMEOUT_TX_SYNC, 100); /* Will be set later to the appropriate value */
	pp_timeout_set(ppi, L1E_TIMEOUT_RX_SYNC, 100); /* Will be set later to the appropriate value */

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
	bds->logL1SyncInterval=ppi->cfg.l1syncInterval;
	bds->L1SyncReceiptTimeout=ppi->cfg.l1syncReceiptTimeout;

	/* Init other specific members */
	bds->next_state=bds->L1SyncState;

	/* Init configuration members of L1SyncOptParamsPortDS */
	L1E_DSPOR_OP(ppi)->timestampsCorrectedTx=TRUE;

	ppi->ext_enabled=TRUE;
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

		/* Reset reception timeout */
		pp_timeout_set(ppi, L1E_TIMEOUT_RX_SYNC, l1e_get_rx_tmo_ms(bds));

		bds->L1SyncLinkAlive = TRUE;
		lstate_enable_extension(ppi);
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
	pp_diag(ppi, ext, 2, "hook: %s\n", __func__);

	/* This correction_field we received is already part of t4 */
	if ( ppi->ext_enabled ) {
		l1e_servo_got_resp(ppi);
	}
	else {
		pp_servo_got_resp(ppi,OPTS(ppi)->ptpFallbackPpsGen);
	}
	return 0;
}

static int  l1e_sync_followup(struct pp_instance *ppi) {
	if ( ppi->ext_enabled ) {
		l1e_servo_got_sync(ppi);
	}
	else {
		pp_servo_got_sync(ppi,OPTS(ppi)->ptpFallbackPpsGen);
	}
	return 1; /* the caller returns too */
}

/* Hmm... "execute_slave" should look for errors; but it's off in WR too */
static int l1e_handle_followup(struct pp_instance *ppi)
{
	/* This handle is called in case of two step clock */
	pp_diag(ppi, ext, 2, "hook: %s\n", __func__);
	return l1e_sync_followup(ppi);
}

static int l1e_handle_sync(struct pp_instance *ppi)
{
	/* This handle is called in case of one step clock */
	pp_diag(ppi, ext, 2, "hook: %s\n", __func__);
	return l1e_sync_followup(ppi);
}

static int l1e_handle_presp(struct pp_instance *ppi)
{
	/* FIXME: verify that last-received cField is already accounted for */
	if ( ppi->ext_enabled )
		l1e_servo_got_presp(ppi);
	else
		pp_servo_got_presp(ppi);
	return 0;
}

/* Check if ready to go to SLAVE state */
static int l1e_ready_for_slave(struct pp_instance *ppi)
{
	pp_diag(ppi, ext, 2, "hook: %s\n", __func__);

	/* Current state is UNCALIBRATED. Can we go to SLAVE state ? */
	/* We do not wait L1SYNC_UP do go to slave state */
	return 1; /* Ready for slave */
}

static 	void l1e_state_change(struct pp_instance *ppi) {

	pp_diag(ppi, ext, 2, "hook: %s\n", __func__);

	if ( !ppi->ext_enabled)
		return;
	switch (ppi->next_state) {
		case PPS_DISABLED :
			/* In PPSI we go to DISABLE state when the link is down */
			/* For the time being, it should be done like this because fsm is not called when the link is down */
			L1E_DSPOR(ppi)->basic.next_state=L1SYNC_DISABLED; /* Force L1Sync DISABLE state */
			l1e_run_state_machine(ppi,NULL,0);
			break;
		case PPS_INITIALIZING :
			L1E_DSPOR(ppi)->basic.L1SyncState=L1E_DSPOR(ppi)->basic.next_state=L1SYNC_DISABLED;
			break;
	}
	if ( ppi->state==PPS_SLAVE && ppi->next_state!=PPS_UNCALIBRATED ) {
		/* Leave SLAVE state : We must stop the PPS generation */
		WRH_OPER()->enable_timing_output(GLBS(ppi),0);
		WRH_OPER()->locking_reset(ppi);
	}
}

static int l1e_new_slave (struct pp_instance *ppi, void *buf, int len) {
	if ( ppi->ext_enabled )
		l1e_servo_init(ppi);
	return 0;
}

static int l1e_require_precise_timestamp(struct pp_instance *ppi) {
	return ppi->ext_enabled  ? L1E_DSPOR_BS(ppi)->L1SyncState==L1SYNC_UP : 0;
}

static int l1e_get_tmo_lstate_detection(struct pp_instance *ppi) {
	return is_externalPortConfigurationEnabled(DSDEF(ppi)) ?
			10000 : /* 10s: externalPortConfiguration enable means no ANN_RECEIPT timeout */
			l1e_get_rx_tmo_ms(L1E_DSPOR_BS(ppi));
}

/* The global structure used by ppsi */
struct pp_ext_hooks l1e_ext_hooks = {
	.open = l1e_open,
	.init = l1e_init,
	.handle_signaling = l1e_handle_signaling,
	.run_ext_state_machine = l1e_run_state_machine,
	.ready_for_slave = l1e_ready_for_slave,
	.handle_resp = l1e_handle_resp,
	.handle_sync = l1e_handle_sync,
	.handle_followup = l1e_handle_followup,
	.new_slave = l1e_new_slave,
#if CONFIG_HAS_P2P
	.handle_presp = l1e_handle_presp,
#endif
	.state_change = l1e_state_change,
	.servo_reset= l1e_servo_reset,
	.require_precise_timestamp=l1e_require_precise_timestamp,
	.get_tmo_lstate_detection=l1e_get_tmo_lstate_detection
};

