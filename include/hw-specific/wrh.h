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
#include <hal_exports.h>
#include <ppsi/lib.h>

/* Please increment WRS_PPSI_SHMEM_VERSION if you change any exported data structure */
#define WRS_PPSI_SHMEM_VERSION 32 /* added HAL_PORT_STATE_RESET to hal */

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

#define FIX_ALPHA_FRACBITS 40
#define FIX_ALPHA_FRACBITS_AS_FLOAT 40.0

#define WRH_SERVO_OFFSET_STABILITY_THRESHOLD 60 /* psec */

/* Parameter of wrs_set_timing_mode */
typedef enum {
	TM_GRAND_MASTER=HAL_TIMING_MODE_GRAND_MASTER,
	TM_FREE_MASTER= HAL_TIMING_MODE_FREE_MASTER,
	TM_BOUNDARY_CLOCK=HAL_TIMING_MODE_BC,
	TM_DISABLED=HAL_TIMING_MODE_DISABLED
}timing_mode_t;


/* White Rabbit hw-dependent functions (code in arch-wrpc and arch-wrs) */
struct wrh_operations {
	int (*locking_enable)(struct pp_instance *ppi);
	int (*locking_poll)(struct pp_instance *ppi);
	int (*locking_disable)(struct pp_instance *ppi);
	int (*locking_reset)(struct pp_instance *ppi);
	int (*enable_ptracker)(struct pp_instance *ppi);

	int (*adjust_in_progress)(void);
	int (*adjust_counters)(int64_t adjust_sec, int32_t adjust_nsec);
	int (*adjust_phase)(int32_t phase_ps);

	int (*read_calib_data)(struct pp_instance *ppi,
			      uint32_t *deltaTx, uint32_t *deltaRx,
			      int32_t *fix_alpha, int32_t *clock_period, uint32_t *bit_slide_ps);
	int (*calib_disable)(struct pp_instance *ppi, int txrx);
	int (*calib_enable)(struct pp_instance *ppi, int txrx);
	int (*calib_poll)(struct pp_instance *ppi, int txrx, uint32_t *delta);
	int (*calib_pattern_enable)(struct pp_instance *ppi,
				    unsigned int calibrationPeriod,
				    unsigned int calibrationPattern,
				    unsigned int calibrationPatternLen);
	int (*calib_pattern_disable)(struct pp_instance *ppi);
	int (*enable_timing_output)(struct pp_globals *,int enable);
	int (*read_corr_data)(struct pp_instance *ppi, int64_t *fixAlpha,
			int32_t *clock_period, uint32_t *bit_slide_ps);
	timing_mode_t (*get_timing_mode)(struct pp_globals *);
	timing_mode_state_t (*get_timing_mode_state)(struct pp_globals *);
	int (*set_timing_mode)(struct pp_globals *, timing_mode_t tm);
};

extern struct wrh_operations wrh_oper;

static inline struct wrh_operations *WRH_OPER(void)
{
	return &wrh_oper;
}


#endif /* __WRH_H__ */

