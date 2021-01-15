/*
 * Copyright (C) 2011 CERN (www.cern.ch)
 * Author: Alessandro Rubini
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Aurelio Colosimo
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */
#ifndef __WRPC_H
#define __WRPC_H

#include <ppsi/ppsi.h>
#include <libwr/hal_shmem.h>

/* This part is exactly wrpc-sw::wrc_ptp.h */
#define WRC_MODE_UNKNOWN 0
#define WRC_MODE_GM 1
#define WRC_MODE_MASTER 2
#define WRC_MODE_SLAVE 3
#define WRC_MODE_ABSCAL 4
extern int ptp_mode;

int wrc_ptp_init(void);
int wrc_ptp_set_mode(int mode);
int wrc_ptp_get_mode(void);
int wrc_ptp_sync_mech(int e2e_p2p_qry);
int wrc_ptp_start(void);
int wrc_ptp_stop(void);
int wrc_ptp_run(int start_stop_qry);
int wrc_ptp_update(void);
/* End of wrc-ptp.h */

extern struct pp_network_operations wrpc_net_ops;
extern struct pp_time_operations wrpc_time_ops;

/* other network stuff, bah.... */

struct wrpc_ethhdr {
	unsigned char	h_dest[6];
	unsigned char	h_source[6];
	uint16_t	h_proto;
} __attribute__((packed));

typedef struct  wrpc_arch_data_t {
	/* Keep a copy of timing mode for dump */
	wrh_timing_mode_t timingMode; /* Timing mode: Grand master, Free running,...*/
} wrpc_arch_data_t;

/* values mapped to pp_globals->pp_runtime_opts->forcePpsGen */
typedef enum {
	pps_force_off,
	pps_force_on,
	pps_force_check
} wrpc_pps_force_t;

/* wrpc-spll.c (some should move to time-wrpc/) */
int wrpc_spll_locking_enable(struct pp_instance *ppi);
int wrpc_spll_locking_poll(struct pp_instance *ppi);
int wrpc_spll_locking_disable(struct pp_instance *ppi);
int wrpc_spll_locking_reset(struct pp_instance *ppi);
int wrpc_spll_enable_ptracker(struct pp_instance *ppi);
int wrpc_adjust_in_progress(void);
int wrpc_adjust_counters(int64_t adjust_sec, int32_t adjust_nsec);
int wrpc_adjust_phase(int32_t phase_ps);
int wrpc_enable_timing_output(struct pp_globals *ppg, int enable);
uint8_t wrc_pps_force(wrpc_pps_force_t action);

/* wrpc-calibration.c */
int wrpc_read_calibration_data(
			       struct pp_instance *ppi,
			       int32_t *clock_period,
			       TimeInterval *scaledBitSlide,
			       RelativeDifference *scaledDelayCoefficient,
			       TimeInterval *scaledSfpDeltaTx,
			       TimeInterval *scaledSfpDeltaRx);
int wrpc_get_port_state(struct hal_port_state *port, const char *port_name);


static inline wrpc_arch_data_t *WRPC_ARCH_I(struct pp_instance *ppi)
{
	return (wrpc_arch_data_t *) GLBS(ppi)->arch_data;
}

static inline wrpc_arch_data_t *WRPC_ARCH_G(struct pp_globals *ppg)
{
	return (wrpc_arch_data_t *) ppg->arch_data;
}

#endif /* __WRPC_H */
