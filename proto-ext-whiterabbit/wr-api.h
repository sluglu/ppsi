/*
 * Copyright (C) 2011 CERN (www.cern.ch)
 * Author: Aurelio Colosimo
 * Based on ptp-noposix project (see AUTHORS for details)
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */

#ifndef __WREXT_WR_API_H__
#define __WREXT_WR_API_H__

#if CONFIG_HAS_EXT_WR

/* Don't include the Following when this file is included in assembler. */
#ifndef __ASSEMBLY__
#include <ppsi/lib.h>
#include "../include/hw-specific/wrh.h"
#include "wr-constants.h"

/*
 * This structure is used as extension-specific data in the DSPort
 * (see wrspec.v2-06-07-2011, page 17)
 */
struct wr_dsport {
	Boolean    wrModeOn; /* True when extension is running */
	Boolean    parentWrModeOn;
	FixedDelta deltaTx;
	FixedDelta deltaRx;
	FixedDelta otherNodeDeltaTx;
	FixedDelta otherNodeDeltaRx;
	Boolean doRestart;

	Enumeration8 wrConfig;
	Enumeration8 wrMode;
	Enumeration8  wrPortState; /* used for sub-states during calibration */
	/* FIXME check doc: knownDeltaTx, knownDeltaRx, deltasKnown?) */
	Boolean calibrated;
	UInteger32 wrStateTimeout;
	UInteger8 wrStateRetry;
	UInteger32 calPeriod;		/* microseconsds, never changed */
	UInteger8 calRetry;
	Enumeration8 parentWrConfig;
	Boolean parentIsWRnode; /* FIXME Not in the doc */
	/* FIXME check doc: (parentWrMode?) */
	Enumeration16 msgTmpWrMessageID; /* FIXME Not in the doc */
	Boolean parentCalibrated;

	/* FIXME: are they in the doc? */
	UInteger16 otherNodeCalSendPattern;
	UInteger32 otherNodeCalPeriod;/* microseconsds, never changed */
	UInteger8 otherNodeCalRetry;
};

/* This uppercase name matches "DSPOR(ppi)" used by standard protocol */
static inline struct wr_dsport *WR_DSPOR(struct pp_instance *ppi)
{
	return ppi->portDS->ext_dsport;
}

static inline Integer32 phase_to_cf_units(Integer32 phase)
{
	uint64_t ph64;
	if (phase >= 0) {
		ph64 = phase * 65536LL;
		__div64_32(&ph64, 1000);
		return ph64;
	} else {
		ph64 = -phase * 65536LL;
		__div64_32(&ph64, 1000);
		return -ph64;
	}
}

/* Pack/Unkpack White rabbit message in the suffix of PTP announce message */
void msg_pack_announce_wr_tlv(struct pp_instance *ppi);
void msg_unpack_announce_wr_tlv(void *buf, MsgAnnounce *ann);

/* Pack/Unkpack/Issue White rabbit message signaling msg */
int msg_pack_wrsig(struct pp_instance *ppi, Enumeration16 wr_msg_id);
void msg_unpack_wrsig(struct pp_instance *ppi, void *buf,
		      MsgSignaling *wrsig_msg, Enumeration16 *wr_msg_id);
int msg_issue_wrsig(struct pp_instance *ppi, Enumeration16 wr_msg_id);

/* White rabbit state functions */
int wr_present(struct pp_instance *ppi, void *buf, int len);
int wr_m_lock(struct pp_instance *ppi, void *buf, int len);
int wr_s_lock(struct pp_instance *ppi, void *buf, int len);
int wr_locked(struct pp_instance *ppi, void *buf, int len);
int wr_calibration(struct pp_instance *ppi, void *buf, int len);
int wr_calibrated(struct pp_instance *ppi, void *buf, int len);
int wr_resp_calib_req(struct pp_instance *ppi, void *buf, int len);
int wr_link_on(struct pp_instance *ppi, void *buf, int len);
int wr_abscal(struct pp_instance *ppi, void *buf, int plen);

/* Common functions, used by various states and hooks */
void wr_handshake_init(struct pp_instance *ppi, int mode);
void wr_handshake_fail(struct pp_instance *ppi); /* goto non-wr */
int wr_handshake_retry(struct pp_instance *ppi); /* 1 == retry; 0 == failed */
int wr_execute_slave(struct pp_instance *ppi);


/* wr_servo interface */
int wr_servo_init(struct pp_instance *ppi);
void wr_servo_reset(struct pp_instance *ppi);
void wr_servo_enable_tracking(int enable);
int wr_servo_got_sync(struct pp_instance *ppi);
int wr_servo_got_delay(struct pp_instance *ppi);
int wr_servo_update(struct pp_instance *ppi);

struct wr_servo_state {
	/* These fields are used by servo code, after asetting at init time */
	int32_t delta_txm_ps;
	int32_t delta_rxm_ps;
	int32_t delta_txs_ps;
	int32_t delta_rxs_ps;
	int32_t fiber_fix_alpha;
	int32_t clock_period_ps;

	/* Following fields are for monitoring/diagnostics (use w/ shmem) */
	struct pp_time delayMM;
	int64_t        delayMM_ps;
	int32_t        cur_setpoint;
	int64_t        delayMS_ps;
	int            tracking_enabled;
	int64_t        skew;

	/* Values used by snmp. Values are increased at servo update when
	 * erroneous condition occurs. */
	uint32_t n_err_state;
	uint32_t n_err_offset;
	uint32_t n_err_delta_rtt;
	struct pp_time update_time;

	/* These fields are used by servo code, across iterations */
	struct pp_time t1, t2, t3, t4, t5, t6;
	int64_t        prev_delayMS_ps;
	int missed_iters;
};


/* All data used as extension ppsi-wr must be put here */
struct wr_data {
	struct wr_servo_state servo_state;
};

extern struct pp_ext_hooks wr_ext_hooks;
extern int wrTmoIdx;

#endif /* __ASSEMBLY__ */
#endif  /* CONFIG_EXT_WR == 1*/

#endif /* __WREXT_WR_API_H__ */
