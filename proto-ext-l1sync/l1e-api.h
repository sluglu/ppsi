/*
 * Copyright (C) 2018 CERN (www.cern.ch)
 * Author: Jean-Claude BAU & Maciej Lipinski
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */

#ifndef __L1SYNC_EXT_API_H__
#define __L1SYNC_EXT_API_H__

#if CONFIG_EXT_L1SYNC == 1
#define PROTO_EXT_L1SYNC (1)
#else
#define PROTO_EXT_L1SYNC (0)
#endif

#if CONFIG_EXT_L1SYNC == 1

#include <ppsi/lib.h>
#include "../include/hw-specific/wrh.h"
#include "l1e-constants.h"

#include <math.h>

/* Rename the timeouts, for readability */
#define L1E_TIMEOUT_TX_SYNC	PP_TO_EXT_0
#define L1E_TIMEOUT_RX_SYNC	PP_TO_EXT_1

#define L1E_DEFAULT_L1SYNCRECEIPTTIMEOUT 5 /* was 3: need more for pll lock */

/*
 * We don't have ha_dsport, but rather rely on wr_dsport with our fields added.
 *
 *
 * Fortunately (and strangely (thanks Maciej), here the spec *is* consistent.
 * These 3 bits are valid 4 times, in 4 different bytes (conf/active, me/peer)
 */
#define L1E_TX_COHERENT	0x01
#define L1E_RX_COHERENT	0x02
#define L1E_CONGRUENT	0x04
#define L1E_OPT_PARAMS	0x08

enum l1_sync_states { /*draft P1588_v_29: page 334 */
	__L1SYNC_MISSING = 0, /* my addition... */ //TODO: std-error->report in ballout
	L1SYNC_DISABLED = 1,
	L1SYNC_IDLE,
	L1SYNC_LINK_ALIVE,
	L1SYNC_CONFIG_MATCH,
	L1SYNC_UP,
};

/* Pack/Unkpack L1SYNC messages (all of them are signalling messages) */
int l1e_pack_signal(struct pp_instance *ppi);
int l1e_unpack_signal(struct pp_instance *ppi, void *pkt, int plen);

/*
 * These structures are used as extension-specific data in the DSPort
 * ()
 */
typedef struct  { /*draft P1588_v_29: page 100 and 333-335 */
	/* configurable members */
	Boolean		L1SyncEnabled;
	Boolean		txCoherentIsRequired;
	Boolean		rxCoherentIsRequired;
	Boolean		congruentIsRequired;
	Boolean		optParamsEnabled;
	Integer8	logL1SyncInterval;
	Integer8	L1SyncReceiptTimeout;
	/* dynamic members */
	Boolean		L1SyncLinkAlive;
	Boolean		isTxCoherent;
	Boolean		isRxCoherent;
	Boolean		isCongruent;
	Enumeration8	L1SyncState;
	Boolean		peerTxCoherentIsRequired;
	Boolean		peerRxCoherentIsRequired;
	Boolean		peerCongruentIsRequired;
	Boolean		peerIsTxCoherent;
	Boolean		peerIsRxCoherent;
	Boolean		peerIsCongruent;
	/* None compliant members with IEEE1558-2018 */
	Enumeration8	next_state;
} L1SyncBasicPortDS_t;

typedef struct { /*draft P1588_v_29: page 101 and 340-341  */
	/* configurable members */
	Boolean		timestampsCorrectedTx;
	/* dynamic members */
	Boolean		phaseOffsetTxValid;
	Boolean		frequencyOffsetTxValid;
	struct pp_time 	phaseOffsetTx;
	Timestamp	phaseOffsetTxTimesatmp;
	struct pp_time 	frequencyOffsetTx;
	Timestamp	frequencyOffsetTxTimesatmp;
} L1SyncOptParamsPortDS_t;

/* Add all extension port DS related structure must be store here */
typedef struct  {
	wrh_portds_head_t  head; /* Must be always at the first place */
	L1SyncBasicPortDS_t      basic;
	L1SyncOptParamsPortDS_t  opt_params;

	Boolean    parentExtModeOn;
}l1e_ext_portDS_t;

static inline  l1e_ext_portDS_t *L1E_DSPOR(struct pp_instance *ppi)
{
	return (l1e_ext_portDS_t *) ppi->portDS->ext_dsport;
}

static inline L1SyncBasicPortDS_t *L1E_DSPOR_BS(struct pp_instance *ppi)
{
	return &L1E_DSPOR(ppi)->basic;
}

static inline  L1SyncOptParamsPortDS_t *L1E_DSPOR_OP(struct pp_instance *ppi)
{
	return &L1E_DSPOR(ppi)->opt_params;
}


/****************************************************************************************/
/* l1e_servo interface */

struct l1e_servo_state {

	int state;

	/* These fields are used by servo code, after setting at init time */
	int64_t fiber_fix_alpha;
	int32_t clock_period_ps;

	/* Following fields are for monitoring/diagnostics (use w/ shmem) */
	struct pp_time delayMM;
	int64_t delayMM_ps;
	int32_t cur_setpoint_ps;
	int64_t delayMS_ps;
	int tracking_enabled;
	int64_t skew_ps;
	int64_t offsetMS_ps;

	/* Values used by snmp. Values are increased at servo update when
	 * erroneous condition occurs. */
	uint32_t n_err_state;
	uint32_t n_err_offset;
	uint32_t n_err_delta_rtt;
	struct pp_time update_time;

	/* These fields are used by servo code, across iterations */
	struct pp_time t1, t2, t3, t4, t5, t6;
	int64_t prev_delayMS_ps;
	int missed_iters;
};

/* Prototypes */
int     l1e_servo_init(struct pp_instance *ppi);
void    l1e_servo_reset(struct pp_instance *ppi);
void    l1e_servo_enable_tracking(int enable);
int     l1e_servo_got_sync(struct pp_instance *ppi, struct pp_time *t1,
		      struct pp_time *t2);
int     l1e_servo_got_delay(struct pp_instance *ppi);
int     l1e_servo_update(struct pp_instance *ppi);
uint8_t l1e_creat_L1Sync_bitmask(int tx_coh, int rx_coh, int congru);
void    l1e_print_L1Sync_basic_bitmaps(struct pp_instance *ppi,
			uint8_t configed, uint8_t active, char* text);
void    l1e_servo_enable_tracking(int enable);
int l1e_update_correction_values(struct pp_instance *ppi);
int l1e_run_state_machine(struct pp_instance *ppi);

/* All data used as extension ppsi l1sync must be put here */
struct l1e_data {
	struct l1e_servo_state servo_state;
};

static inline  struct l1e_servo_state *L1E_SRV(struct pp_instance *ppi)
{
	return &((struct l1e_data *)ppi->ext_data)->servo_state;
}



extern struct pp_ext_hooks l1e_ext_hooks;

#endif /* CONFIG_EXT_L1SYNC == 1 */
#endif /* __L1SYNC_EXT_API_H__ */
