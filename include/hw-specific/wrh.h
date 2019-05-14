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

	int (*read_calib_data)(struct pp_instance *ppi,int32_t *clock_period, TimeInterval *scaledBitSlide,
			RelativeDifference *scaledDelayCoefficient,
			TimeInterval *scaledSfpDeltaTx, TimeInterval *scaledSfpDeltaRx);
	int (*enable_timing_output)(struct pp_globals *,int enable);
	timing_mode_t (*get_timing_mode)(struct pp_globals *);
	timing_mode_state_t (*get_timing_mode_state)(struct pp_globals *);
	int (*set_timing_mode)(struct pp_globals *, timing_mode_t tm);
};

extern struct wrh_operations wrh_oper;

static inline struct wrh_operations *WRH_OPER(void)
{
	return &wrh_oper;
}


/****************************************************************************************/
/* wrh_servo interface */

/* Servo state */
typedef enum {
	WRH_UNINITIALIZED = 0,
	WRH_SYNC_TAI,
	WRH_SYNC_NSEC,
	WRH_SYNC_PHASE,
	WRH_TRACK_PHASE,
	WRH_WAIT_OFFSET_STABLE,
}wrh_servo_state_t;


#define WRH_SERVO_RESET_DATA_SIZE        (sizeof(wrh_servo_t)-offsetof(wrh_servo_t,reset_address))
#define WRH_SERVO_RESET_DATA(servo)      memset(&servo->reset_address,0,WRH_SERVO_RESET_DATA_SIZE);


typedef struct wrh_servo_t {
	/* Values used by snmp. Values are increased at servo update when
	 * erroneous condition occurs. */
	uint32_t n_err_state;
	uint32_t n_err_offset;
	uint32_t n_err_delta_rtt;

	int32_t cur_setpoint_ps;

	/* ----- All data after this line will cleared during a servo reset */
	int reset_address;

	/* These fields are used by servo code, after setting at init time */
	int32_t clock_period_ps;

	/* Following fields are for monitoring/diagnostics (use w/ shmem) */
	int64_t delayMM_ps;
	int64_t delayMS_ps;
	int tracking_enabled;
	int64_t skew_ps;
	int64_t offsetMS_ps;

	/* These fields are used by servo code, across iterations */
	int64_t prev_delayMS_ps;
	int missed_iters;

	Boolean readyForSync; /* Ready for synchronization */
	Boolean doRestart; /* PLL is unlocked: A restart of the calibration is needed */
} wrh_servo_t;


static inline  wrh_servo_t *WRH_SRV(struct pp_instance *ppi)
{
	return (wrh_servo_t *)ppi->ext_data;
}


/* Prototypes */
extern void    wrh_servo_enable_tracking(int enable);
extern int     wrh_servo_init(struct pp_instance *ppi);
extern void    wrh_servo_reset(struct pp_instance *ppi);
extern void    wrh_servo_enable_tracking(int enable);
extern int     wrh_servo_got_sync(struct pp_instance *ppi);
extern int     wrh_servo_got_resp(struct pp_instance *ppi);
extern int     wrh_servo_got_presp(struct pp_instance *ppi);
extern int     wrh_update_correction_values(struct pp_instance *ppi);

#endif /* __WRH_H__ */

