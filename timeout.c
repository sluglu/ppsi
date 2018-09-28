/*
 * Copyright (C) 2013 CERN (www.cern.ch)
 * Author: Alessandro Rubini
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */
#include <ppsi/ppsi.h>

enum rand_type {
	RAND_NONE,	/* Not randomized */
	RAND_70_130,	/* Should be 70% to 130% of 1 << value */
	RAND_0_200,	/* Should be 0% to 200% of 1 << value */
};

struct timeout_config {
	char *name;
	int which_rand;
	int value;
};

/* most timeouts have a static configuration. Save it here */
static struct timeout_config to_configs[__PP_TO_ARRAY_SIZE] = {
	[PP_TO_REQUEST] =	{"REQUEST", /* RAND defined at run-time */},
	[PP_TO_SYNC_SEND] =	{"SYNC_SEND",	RAND_70_130,},
	[PP_TO_BMC] =		{"BMC",		RAND_NONE,},
	[PP_TO_ANN_RECEIPT] =	{"ANN_RECEIPT",	RAND_NONE,},
	[PP_TO_ANN_SEND] =	{"ANN_SEND",	RAND_70_130,},
	[PP_TO_FAULT] =		{"FAULT",	RAND_NONE, 4000},
	[PP_TO_QUALIFICATION] = {"QUAL",	RAND_NONE,},
	/* extension timeouts are explicitly set to a value */
	[PP_TO_EXT_0]={"EXT_0", RAND_NONE,},
	[PP_TO_EXT_1]={"EXT_1", RAND_NONE,}
};

#define TIMEOUT_MAX_LOG_VALUE 21 /* 2^21 * 1000 =2097152000ms is the maximum value that can be stored in an integer */
#define TIMEOUT_MIN_LOG_VALUE -9 /* 2^-9 = 1ms is the minimum value that can be stored in an integer */

int pp_timeout_log_to_ms ( Integer8 logValue) {
	/* logValue can be in range -128 , +127
	 * However we restrict this range to TIMEOUT_MIN_LOG_VALUE, TIMEOUT_MAX_LOG_VALUE
	 * in order to optimize the calculation
	 */

	if ( logValue >= 0 ) {
		if ( logValue > TIMEOUT_MAX_LOG_VALUE )
			logValue=TIMEOUT_MAX_LOG_VALUE;
		return (1<< logValue)*1000;
	}
	else {
		if (logValue<TIMEOUT_MIN_LOG_VALUE)
			logValue=TIMEOUT_MIN_LOG_VALUE;
		return 1000>>-logValue;
	}
}

/* Init fills the timeout values */
void pp_timeout_init(struct pp_instance *ppi)
{
	portDS_t *port = ppi->portDS;
	Boolean p2p=CONFIG_HAS_P2P && ppi->delayMechanism == P2P;
	Integer8 logDelayRequest=p2p ?
			port->logMinPdelayReqInterval : port->logMinDelayReqInterval;

	to_configs[PP_TO_REQUEST].which_rand = p2p ? RAND_NONE : RAND_0_200;
	to_configs[PP_TO_REQUEST].value= pp_timeout_log_to_ms(logDelayRequest);
	/* fault timeout is 4 avg request intervals, not randomized */
	to_configs[PP_TO_FAULT].value =
			pp_timeout_log_to_ms(logDelayRequest + 12); /* 0 -> 4096ms */
	to_configs[PP_TO_SYNC_SEND].value =  pp_timeout_log_to_ms(port->logSyncInterval);
	to_configs[PP_TO_BMC].value = pp_timeout_log_to_ms(port->logAnnounceInterval);
	to_configs[PP_TO_ANN_RECEIPT].value = 1000 * (
		port->announceReceiptTimeout << port->logAnnounceInterval);
	to_configs[PP_TO_ANN_SEND].value =  pp_timeout_log_to_ms(port->logAnnounceInterval);
	to_configs[PP_TO_QUALIFICATION].value =
	    (1000 << port->logAnnounceInterval)*(DSCUR(ppi)->stepsRemoved + 1);
}

void __pp_timeout_set(struct pp_instance *ppi, int index, int millisec)
{
	ppi->timeouts[index] = ppi->t_ops->calc_timeout(ppi, millisec);
	pp_diag(ppi, time, 3, "Set timeout for %s : %i / %lu\n",
		to_configs[index].name, millisec, ppi->timeouts[index]);
}

void pp_timeout_clear(struct pp_instance *ppi, int index)
{
	__pp_timeout_set(ppi, index, 0);
}

void pp_timeout_set(struct pp_instance *ppi, int index)
{
	static uint32_t seed;
	int millisec;
	struct timeout_config * to_config=&to_configs[index];

	millisec = to_config->value;
	if (to_config->which_rand!=RAND_NONE ) {
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

		switch(to_config->which_rand) {
		case RAND_70_130:
			/*
			 * We are required to fit between 70% and 130%
			 * of the value for 90% of the time, at least.
			 * So randomize between 80% and 120%: constant
			 * part is 80% and variable is 40%.
			 */
			millisec = (millisec * 2) + rval % millisec;
			break;
		case RAND_0_200:
			millisec = rval % (millisec * 5);
			break;
		/* RAND_NONE already treated */
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
	int ret = time_after_eq(now,ppi->timeouts[index]);

	if (ret)
		pp_diag(ppi, time, 1, "timeout expired: %s / %lu\n",
			to_configs[index].name,now);
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

	r1 = ppi->timeouts[i1] - now;
	return r1 < 0 ? 0 : r1;
}

int pp_next_delay_2(struct pp_instance *ppi, int i1, int i2)
{
	unsigned long now = ppi->t_ops->calc_timeout(ppi, 0);
	signed long r1, r2;

	r1 = ppi->timeouts[i1] - now;
	r2 = ppi->timeouts[i2] - now;
	if (r2 < r1)
		r1 = r2;
	return r1 < 0 ? 0 : r1;
}

int pp_next_delay_3(struct pp_instance *ppi, int i1, int i2, int i3)
{
	unsigned long now = ppi->t_ops->calc_timeout(ppi, 0);
	signed long r1, r2, r3;

	r1 = ppi->timeouts[i1] - now;
	r2 = ppi->timeouts[i2] - now;
	r3 = ppi->timeouts[i3] - now;
	if (r2 < r1)
		r1 = r2;
	if (r3 < r1)
		r1 = r3;
	return r1 < 0 ? 0 : r1;
}
