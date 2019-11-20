/*
 * Copyright (C) 2019 CERN (www.cern.ch)
 * Author: Jean-Claude BAU
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */

#ifndef USERSPACE_PPSI_INCLUDE_PPSI_BMC_H_
#define USERSPACE_PPSI_INCLUDE_PPSI_BMC_H_

typedef enum {
	PP_TIMING_MODE_STATE_UNLOCKED=0,
	PP_TIMING_MODE_STATE_HOLDOVER,
	PP_TIMING_MODE_STATE_LOCKED,
}pp_timing_mode_state_t;

/**
 * These structure is used to describe the behavior to degrade a clock class
 * or its attributes.
 */
typedef struct {
	ClockQuality clockQuality;
	UInteger8 timeSource;
	Boolean ptpTimeScale;
	Boolean frequencyTraceable;
	Boolean timeTraceable;
	char *msg;
}deviceAttributesDegradation_t;

typedef struct {
	int clockClass;
	pp_timing_mode_state_t  lastTimingModeState;
	int enable_timing_output;
	deviceAttributesDegradation_t holdover;
	deviceAttributesDegradation_t unlocked;
}clockDegradation_t;


/* Used to define all default PTP device attributes for a given clock class */
typedef struct {
	int clockClass;
	UInteger8 clock_quality_clockAccuracy;
	UInteger8 timeSource;
	UInteger16 clock_quality_offsetScaledLogVariance;
	Boolean ptpTimeScale;
	Boolean frequencyTraceable;
	Boolean timeTraceable;
}defaultDeviceAttributes_t;


/* These structures can be patched to change the default behavior */
extern deviceAttributesDegradation_t deviceAttributesDegradation[];
extern defaultDeviceAttributes_t defaultDeviceAttributes[];

extern void bmc_set_default_device_attributes (struct pp_globals *ppg);
extern void bmc_update_clock_quality(struct pp_globals *ppg);
extern void bmc_apply_configured_device_attributes(struct pp_globals *ppg);
extern int bmc_is_erbest(struct pp_instance *ppi, struct PortIdentity *srcPortIdentity);





#endif /* USERSPACE_PPSI_INCLUDE_PPSI_BMC_H_ */
