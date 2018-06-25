/*
 * Copyright (C) 2018 CERN (www.cern.ch)
 * Author: Jean-Claude BAU & Maciej Lipinski
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */


#ifndef __L1SYNC_EXT_CONSTANTS_H__
#define __L1SYNC_EXT_CONSTANTS_H__

/* Define threshold values for SNMP */
#define SNMP_MAX_OFFSET_PS 500
#define SNMP_MAX_DELTA_RTT_PS 1000


/* L1SYNC Servo */
enum {
	L1E_UNINITIALIZED = 0,
	L1E_SYNC_NSEC,
	L1E_SYNC_TAI,
	L1E_SYNC_PHASE,
	L1E_TRACK_PHASE,
	L1E_WAIT_OFFSET_STABLE,
};


#endif /* __L1SYNC_EXT_CONSTANTS_H__ */
