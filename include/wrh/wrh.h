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

enum {
	WRH_SERVO_ENTER, WRH_SERVO_LEAVE
};


struct wrh_servo_head {
	int  extension; /* Used to identify the servo extension. Useful for the monitoring tool */
};


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
	int (*servo_hook)(struct wrh_servo_head *s, int action);
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

