/*
 * Copyright (C) 2011 CERN (www.cern.ch)
 * Author: Aurelio Colosimo
 * Based on PTPd project v. 2.1.0 (see AUTHORS for details)
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */

#ifndef __PPSI_CONSTANTS_H__
#define __PPSI_CONSTANTS_H__

/* general purpose constants */
#define PP_NSEC_PER_SEC (1000*1000*1000)
#define PP_PSEC_PER_SEC ((int64_t)1000*(int64_t)PP_NSEC_PER_SEC)

/* implementation specific constants */
#define PP_MAX_LINKS				(CONFIG_NR_PORTS*CONFIG_NR_INSTANCES_PER_PORT)
#define PP_DEFAULT_CONFIGFILE			"/etc/ppsi.conf"

#define PP_DEFAULT_FLAGS			0
#define PP_DEFAULT_ROLE				PPSI_ROLE_AUTO
#define PP_DEFAULT_PROTO			PPSI_PROTO_UDP /* overridden by arch */
#define PP_DEFAULT_AP				10
#define PP_DEFAULT_AI				1000
#define PP_DEFAULT_DELAY_S			6

#define PP_MIN_DOMAIN_NUMBER		        0
#define PP_MAX_DOMAIN_NUMBER	 	        255
#define PP_DEFAULT_DOMAIN_NUMBER		    0

#define PP_DEFAULT_ANNOUNCE_INTERVAL		1	/* 0 in 802.1AS */
#define PP_MIN_ANNOUNCE_INTERVAL            0
#define PP_MAX_ANNOUNCE_INTERVAL            4
#define PP_DEFAULT_ANNOUNCE_RECEIPT_TIMEOUT	3	/* 3 by default */
#define PP_MIN_ANNOUNCE_RECEIPT_TIMEOUT     2
#define PP_MAX_ANNOUNCE_RECEIPT_TIMEOUT     255

#define PP_DEFAULT_MIN_DELAY_REQ_INTERVAL	0
#define PP_MIN_MIN_DELAY_REQ_INTERVAL		0
#define PP_MAX_MIN_DELAY_REQ_INTERVAL		5

#define PP_DEFAULT_MIN_PDELAY_REQ_INTERVAL	0
#define PP_MIN_MIN_PDELAY_REQ_INTERVAL		0
#define PP_MAX_MIN_PDELAY_REQ_INTERVAL		5

#define PP_DEFAULT_SYNC_INTERVAL		    0	/* -7 in 802.1AS */
#define PP_MIN_SYNC_INTERVAL		       -1
#define PP_MAX_SYNC_INTERVAL		        1

/* Min/max values for delay coefficient */
#define PP_MIN_DELAY_COEFFICIENT_AS_RELDIFF  (((int64_t)-1)<<REL_DIFF_FRACBITS)
#define PP_MAX_DELAY_COEFFICIENT_AS_RELDIFF  (((int64_t) 1)<<REL_DIFF_FRACBITS)
#define PP_MIN_DELAY_COEFFICIENT_AS_DOUBLE   (-1.0)
#define PP_MAX_DELAY_COEFFICIENT_AS_DOUBLE   (1.0)

#define PP_DEFAULT_UTC_OFFSET			37
#define PP_MIN_PRIORITY1			    0
#define PP_MAX_PRIORITY1			    255
#define PP_DEFAULT_PRIORITY1			64

#define PP_MIN_PRIORITY2			    0
#define PP_MAX_PRIORITY2			    255
#define PP_DEFAULT_PRIORITY2			128

#define PP_DEFAULT_EXT_PORT_CONFIG_ENABLE		0

#define PP_MIN_PTP_PPSGEN_THRESHOLD_MS        1
#define PP_MAX_PTP_PPSGEN_THRESHOLD_MS     1000
#define PP_DEFAULT_PTP_PPSGEN_THRESHOLD_MS  500

#define PP_MIN_GM_DELAY_TO_GEN_PPS_SEC        0
#define PP_MAX_GM_DELAY_TO_GEN_PPS_SEC     1000
#define PP_DEFAULT_GM_DELAY_TO_GEN_PPS_SEC    0 // No PPS

/* Clock classes (pag 55, PTP-2008). See ppsi-manual for an explanation */
#define PP_MIN_CLOCK_CLASS              0
#define PP_MAX_CLOCK_CLASS              255
#define PP_CLASS_SLAVE_ONLY			    255
#define PP_CLASS_DEFAULT			    248
#define PP_PTP_CLASS_GM_LOCKED			6
#define PP_PTP_CLASS_GM_HOLDOVER		7
#define PP_PTP_CLASS_GM_UNLOCKED		52
#define PP_ARB_CLASS_GM_LOCKED			13
#define PP_ARB_CLASS_GM_HOLDOVER		14
#define PP_ARB_CLASS_GM_UNLOCKED		58

#define PP_MIN_CLOCK_ACCURACY           0
#define PP_MAX_CLOCK_ACCURACY           255
#define PP_ACCURACY_DEFAULT			    0xFE
#define PP_PTP_ACCURACY_GM_LOCKED		0x21
#define PP_PTP_ACCURACY_GM_HOLDOVER		0x21
#define PP_PTP_ACCURACY_GM_UNLOCKED		0xFE
#define PP_ARB_ACCURACY_GM_LOCKED		0x21
#define PP_ARB_ACCURACY_GM_HOLDOVER		0x21
#define PP_ARB_ACCURACY_GM_UNLOCKED		0xFE

#define PP_MIN_CLOCK_VARIANCE           0
#define PP_MAX_CLOCK_VARIANCE           65535
#define PP_VARIANCE_DEFAULT				0xC71D
#define PP_PTP_VARIANCE_GM_LOCKED		0xB900
#define PP_PTP_VARIANCE_GM_HOLDOVER		0xC71D
#define PP_PTP_VARIANCE_GM_UNLOCKED		0xC71D
#define PP_ARB_VARIANCE_GM_LOCKED		0xB900
#define PP_ARB_VARIANCE_GM_HOLDOVER		0xC71D
#define PP_ARB_VARIANCE_GM_UNLOCKED		0xC71D

#define PP_NR_FOREIGN_RECORDS           CONFIG_NR_FOREIGN_RECORDS	  /* Clause 9.3.2.4.5 */
#define PP_FOREIGN_MASTER_TIME_WINDOW	4
#define PP_FOREIGN_MASTER_THRESHOLD		2
#define PP_DEFAULT_TTL				    1

#define PP_ALTERNATE_MASTER_FLAG	1
#define PP_TWO_STEP_FLAG		2
#define PP_VERSION_PTP			2
#define PP_MINOR_VERSION_PTP	0

#define PP_HEADER_LENGTH		34
#define PP_ANNOUNCE_LENGTH		64
#define PP_SYNC_LENGTH			44
#define PP_FOLLOW_UP_LENGTH		44
#define PP_PDELAY_REQ_LENGTH		54
#define PP_DELAY_REQ_LENGTH		44
#define PP_DELAY_RESP_LENGTH		54
#define PP_PDELAY_RESP_LENGTH		54
#define PP_PDELAY_RESP_FOLLOW_UP_LENGTH	54
#define PP_MANAGEMENT_LENGTH		48

#define PP_MINIMUM_LENGTH	44
#define PP_MAX_FRAME_LENGTH	128 /* must fit extension and ethhdr */

#define PP_DEFAULT_NEXT_DELAY_MS	1000

/* UDP/IPv4 dependent */
#define PP_UUID_LENGTH			6
#define PP_FLAG_FIELD_LENGTH		2
#define PP_EVT_PORT			319
#define PP_GEN_PORT			320
#define PP_DEFAULT_DOMAIN_ADDRESS	"224.0.1.129"
#define PP_PDELAY_DOMAIN_ADDRESS	"224.0.0.107"

/* Raw ethernet dependent */
#ifndef ETH_P_1588
#define ETH_P_1588			0x88F7
#endif

#define PP_MCAST_MACADDRESS		"\x01\x1B\x19\x00\x00\x00"
#define PP_PDELAY_MACADDRESS		"\x01\x80\xC2\x00\x00\x0E"

#define PP_E2E_MECH     0
#define PP_P2P_MECH     1

#include <arch/constants.h> /* architectures may override the defaults */

#endif /* __PPSI_CONSTANTS_H__ */
