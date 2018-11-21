/*
 * Copyright (C) 2013 CERN (www.cern.ch)
 * Author: Alessandro Rubini
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */
#include <ppsi/ppsi.h>

/* time-out counter names */
static const char *timeOutNames[__PP_TO_ARRAY_SIZE]={
		"REQUEST",
		"SYNC_SEND",
		"BMC",
		"ANN_RECEIPT",
		"ANN_SEND",
		"FAULT",
		"QUAL",
		"EXT_0",
		"EXT_1"
};

#define TIMEOUT_MAX_LOG_VALUE 21 /* 2^21 * 1000 =2097152000ms is the maximum value that can be stored in an integer */
#define TIMEOUT_MIN_LOG_VALUE -9 /* 2^-9 = 1ms is the minimum value that can be stored in an integer */
#define TIMEOUT_MAX_VALUE_MS  ((1<< TIMEOUT_MAX_LOG_VALUE)*1000)
#define TIMEOUT_MIN_VALUE_MS  (1000>>-(TIMEOUT_MIN_LOG_VALUE))

int pp_timeout_log_to_ms ( Integer8 logValue) {
	/* logValue can be in range -128 , +127
	 * However we restrict this range to TIMEOUT_MIN_LOG_VALUE, TIMEOUT_MAX_LOG_VALUE
	 * in order to optimize the calculation
	 */

	if ( logValue >= 0 ) {
		return ( logValue > TIMEOUT_MAX_LOG_VALUE ) ?
				TIMEOUT_MAX_VALUE_MS :
			    ((1<< logValue)*1000);
	}
	else {
		return (logValue<TIMEOUT_MIN_LOG_VALUE) ?
				TIMEOUT_MIN_VALUE_MS :
				(1000>>-logValue);
	}
}

/* Init fills the timeout values */
void pp_timeout_init(struct pp_instance *ppi)
{
	portDS_t *port = ppi->portDS;
	t_timeOutConfig *timeouts=ppi->timeouts;

	Boolean p2p=CONFIG_HAS_P2P && ppi->delayMechanism == P2P;
	Integer8 logDelayRequest=p2p ?
			port->logMinPdelayReqInterval : port->logMinDelayReqInterval;

	timeouts[PP_TO_REQUEST].which_rand = p2p ? TO_RAND_NONE : TO_RAND_0_200;
	timeouts[PP_TO_SYNC_SEND].which_rand=
			timeouts[PP_TO_ANN_SEND].which_rand=TO_RAND_70_130;
	timeouts[PP_TO_BMC].which_rand =
			timeouts[PP_TO_QUALIFICATION].which_rand =
			timeouts[PP_TO_ANN_RECEIPT].which_rand =
			timeouts[PP_TO_FAULT].which_rand =
			timeouts[PP_TO_EXT_0].which_rand =
			timeouts[PP_TO_EXT_1].which_rand = TO_RAND_NONE;

	timeouts[PP_TO_REQUEST].initValueMs= pp_timeout_log_to_ms(logDelayRequest);
	/* fault timeout is 4 avg request intervals, not randomized */
	timeouts[PP_TO_FAULT].initValueMs = pp_timeout_log_to_ms(logDelayRequest);
	if ( timeouts[PP_TO_FAULT].initValueMs < (TIMEOUT_MAX_VALUE_MS>>2))
		timeouts[PP_TO_FAULT].initValueMs<<=2; /* We can multiply by 4. No risk of overload */
	timeouts[PP_TO_SYNC_SEND].initValueMs =  pp_timeout_log_to_ms(port->logSyncInterval);
	timeouts[PP_TO_BMC].initValueMs = pp_timeout_log_to_ms(port->logAnnounceInterval);
	timeouts[PP_TO_ANN_RECEIPT].initValueMs = 1000 * (
		port->announceReceiptTimeout << port->logAnnounceInterval);
	timeouts[PP_TO_ANN_SEND].initValueMs =  pp_timeout_log_to_ms(port->logAnnounceInterval);
	timeouts[PP_TO_QUALIFICATION].initValueMs =
	    (1000 << port->logAnnounceInterval)*(DSCUR(ppi)->stepsRemoved + 1);
}

void __pp_timeout_set(struct pp_instance *ppi, int index, int millisec)
{
	ppi->timeouts[index].tmo = ppi->t_ops->calc_timeout(ppi, millisec);
	pp_diag(ppi, time, 3, "Set timeout for %s : %i / %lu\n",
			timeOutNames[index], millisec, ppi->timeouts[index].tmo);
}

void pp_timeout_clear(struct pp_instance *ppi, int index)
{
	__pp_timeout_set(ppi, index, 0);
}

void pp_timeout_set(struct pp_instance *ppi, int index)
{
	static uint32_t seed;
	int millisec;
	t_timeOutConfig *timeouts=&ppi->timeouts[index];

	millisec = timeouts->initValueMs;
	if (timeouts->which_rand!=TO_RAND_NONE ) {
		uint32_t rval;

		if (!seed) {
			uint32_t *p;
			/* use the least 32 bits of the mac address as seed */
			p = (void *)(&DSDEF(ppi)->clockIdentity)
				+ sizeof(ClockIdentity) - 4;
			seed = *p;
		}
		/* From uclibc: they make 11 + 10 + 10 bits, we stop at 21 */
		seed *= 1103515245;
		seed += 12345;
		rval = (unsigned int) (seed / 65536) % 2048;

		seed *= 1103515245;
		seed += 12345;
		rval <<= 10;
		rval ^= (unsigned int) (seed / 65536) % 1024;

		millisec=(millisec<<1)/5; /* keep 40% of the reference value */
		switch(timeouts->which_rand) {
		case TO_RAND_70_130:
			/*
			 * We are required to fit between 70% and 130%
			 * of the value for 90% of the time, at least.
			 * So randomize between 80% and 120%: constant
			 * part is 80% and variable is 40%.
			 */
			millisec = (millisec * 2) + rval % millisec;
			break;
		case TO_RAND_0_200:
			millisec = rval % (millisec * 5);
			break;
		default:
		/* RAND_NONE already treated */
			break;
		}
	}
	__pp_timeout_set(ppi, index, millisec);
}

/*
 * When we enter a new fsm state, we init all timeouts. Who cares if
 * some of them are not used (and even if some have no default timeout)
 */
void pp_timeout_setall(struct pp_instance *ppi)
{
	int i;
	for (i = 0; i < __PP_TO_ARRAY_SIZE; i++) {
		/* keep BMC timeout */
		if (i!=PP_TO_BMC) {
			pp_timeout_set(ppi, i);
		}
	}
	/* but announce_send must be send soon */
	__pp_timeout_set(ppi, PP_TO_ANN_SEND, 20);
}

int pp_timeout(struct pp_instance *ppi, int index)
{
	unsigned long now=ppi->t_ops->calc_timeout(ppi, 0);
	int ret = time_after_eq(now,ppi->timeouts[index].tmo);

	if (ret)
		pp_diag(ppi, time, 1, "timeout expired: %s / %lu\n",
				timeOutNames[index],now);
	return ret;
}

/*
 * How many ms to wait for the timeout to happen, for ppi->next_delay.
 * It is not allowed for a timeout to not be pending
 */
int pp_next_delay_1(struct pp_instance *ppi, int i1)
{
	unsigned long now = ppi->t_ops->calc_timeout(ppi, 0);
	signed long r1;

	r1 = ppi->timeouts[i1].tmo - now;
	return r1 < 0 ? 0 : r1;
}

int pp_next_delay_2(struct pp_instance *ppi, int i1, int i2)
{
	unsigned long now = ppi->t_ops->calc_timeout(ppi, 0);
	signed long r1, r2;

	r1 = ppi->timeouts[i1].tmo - now;
	r2 = ppi->timeouts[i2].tmo - now;
	if (r2 < r1)
		r1 = r2;
	return r1 < 0 ? 0 : r1;
}

int pp_next_delay_3(struct pp_instance *ppi, int i1, int i2, int i3)
{
	unsigned long now = ppi->t_ops->calc_timeout(ppi, 0);
	signed long r1, r2, r3;

	r1 = ppi->timeouts[i1].tmo - now;
	r2 = ppi->timeouts[i2].tmo - now;
	r3 = ppi->timeouts[i3].tmo - now;
	if (r2 < r1)
		r1 = r2;
	if (r3 < r1)
		r1 = r3;
	return r1 < 0 ? 0 : r1;
}
