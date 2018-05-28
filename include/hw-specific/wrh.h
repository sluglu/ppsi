/*
 * Copyright (C) 2011 CERN (www.cern.ch)
 * Author: Jean-Claude BAU
 * Declarations common to WR switches and nodes.
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */

#ifndef __WRH_H__
#define __WRH_H__

#include <stdint.h>
#include <ppsi/lib.h>

/* Please increment WRS_PPSI_SHMEM_VERSION if you change any exported data structure */
#define WRS_PPSI_SHMEM_VERSION 31 /* changed wrs_shm_head */

/* White Rabbit softpll status values */
#define WRH_SPLL_OK		0
#define WRH_SPLL_READY		1
#define WRH_SPLL_CALIB_NOT_READY	2
#define WRH_SPLL_ERROR		-1

/* White Rabbit calibration defines */
#define WRH_HW_CALIB_TX	1
#define WRH_HW_CALIB_RX	2
#define WRH_HW_CALIB_OK	0
#define WRH_HW_CALIB_READY	1
#define WRH_HW_CALIB_ERROR	-1
#define WRH_HW_CALIB_NOT_FOUND	-3


enum {
	WRH_SERVO_ENTER, WRH_SERVO_LEAVE
};


#define FIX_ALPHA_FRACBITS 40
#define FIX_ALPHA_FRACBITS_AS_FLOAT 40.0

#define WRH_SERVO_OFFSET_STABILITY_THRESHOLD 60 /* psec */

#ifdef CONFIG_WRPC_FAULTS
#define PROTO_EXT_HAS_FAULTS 1
#else
#define PROTO_EXT_HAS_FAULTS 0
#endif


/* Contains portDS common stuff which is manipulated outside of the protocol extension code */
/* Must be declared on top of the extension portDS structure */
typedef struct {
	Boolean extModeOn;
    Boolean ppsOutputOn;
} wrh_portds_head_t;

/* The head is expected at the beginning of the portDS structure */
static inline wrh_portds_head_t *WRH_DSPOR_HEAD(struct pp_instance *ppi)
{
	return (wrh_portds_head_t *) ppi->portDS->ext_dsport;
}

/* White Rabbit hw-dependent functions (code in arch-wrpc and arch-wrs) */
struct wrh_operations {
	int (*locking_enable)(struct pp_instance *ppi);
	int (*locking_poll)(struct pp_instance *ppi, int grandmaster);
	int (*locking_disable)(struct pp_instance *ppi);
	int (*locking_reset)(struct pp_instance *ppi);
	int (*enable_ptracker)(struct pp_instance *ppi);

	int (*adjust_in_progress)(void);
	int (*adjust_counters)(int64_t adjust_sec, int32_t adjust_nsec);
	int (*adjust_phase)(int32_t phase_ps);

	int (*read_calib_data)(struct pp_instance *ppi,
			      uint32_t *deltaTx, uint32_t *deltaRx,
			      int32_t *fix_alpha, int32_t *clock_period);
	int (*calib_disable)(struct pp_instance *ppi, int txrx);
	int (*calib_enable)(struct pp_instance *ppi, int txrx);
	int (*calib_poll)(struct pp_instance *ppi, int txrx, uint32_t *delta);
	int (*calib_pattern_enable)(struct pp_instance *ppi,
				    unsigned int calibrationPeriod,
				    unsigned int calibrationPattern,
				    unsigned int calibrationPatternLen);
	int (*calib_pattern_disable)(struct pp_instance *ppi);
	int (*enable_timing_output)(struct pp_instance *ppi, int enable);
	int (*servo_hook)(struct pp_instance *ppi, int action);
	int (*read_corr_data)(struct pp_instance *ppi, int64_t *delayCoeff,
			      int64_t *ingressLatency,   int64_t *egressLatency,
			      int64_t *msgTPointLatency, int64_t *delayAsymmetry,
			      int64_t *fixAlpha,         int32_t *clock_period );

};

extern struct wrh_operations wrh_oper;

static inline struct wrh_operations *WRH_OPER(void)
{
	return &wrh_oper;
}


#endif /* __WRH_H__ */

