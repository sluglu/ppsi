/*
 * Copyright (C) 2013 CERN (www.cern.ch)
 * Author: Alessandro Rubini
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */
#include <ppsi/ppsi.h>

struct timeout_config {
	char *name;
	int isrand;
	int value;
};

/* most timeouts have a static configuration. Save it here */
static struct timeout_config to_configs[__PP_TO_ARRAY_SIZE] = {
	[PP_TO_REQUEST] =	{"REQUEST",	1,},
	[PP_TO_SYNC_SEND] =	{"SYNC_SEND",	1,},
	[PP_TO_ANN_RECEIPT] =	{"ANN_RECEIPT",	0,},
	[PP_TO_ANN_SEND] =	{"ANN_SEND",	1,},
	[PP_TO_FAULTY] =	{"FAULTY",	0, 4000},
	/* extension timeouts are explicitly set to a value */
};

/* Init fills the timeout values; they are not changed after program startup */
void pp_timeout_init(struct pp_instance *ppi)
{
	struct DSPort *port = ppi->portDS;

	to_configs[PP_TO_REQUEST].value =
		port->logMinDelayReqInterval;
	to_configs[PP_TO_SYNC_SEND].value =
		port->logSyncInterval;
	to_configs[PP_TO_ANN_RECEIPT].value = 1000 * (
		port->announceReceiptTimeout << port->logAnnounceInterval);
	to_configs[PP_TO_ANN_SEND].value = port->logAnnounceInterval;
}

static void pp_timeout_log(struct pp_instance *ppi, int index)
{
	pp_diag(ppi, time, 1, "timeout expired: %s\n",
		to_configs[index].name);
}


void __pp_timeout_set(struct pp_instance *ppi, int index, int millisec)
{
	ppi->timeouts[index] = ppi->t_ops->calc_timeout(ppi, millisec);
}

/*
 * Randomize a timeout. We are required to fit between 70% and 130%
 * of the value for 90% of the time, at least. But making it "almost
 * exact" is bad in a big network. So randomize between 80% and 120%:
 * constant part is 80% and variable is 40%.
 */

void pp_timeout_set(struct pp_instance *ppi, int index)
{
	static uint32_t seed;
	uint32_t rval;
	int millisec;
	int logval = to_configs[index].value;

	if (!to_configs[index].isrand){
		__pp_timeout_set(ppi, index, logval); /* not a logval */
		return;
	}

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

	millisec = (1 << logval) * 400; /* This is 40% of the nominal value */
	millisec = (millisec * 2) + rval % millisec;

	__pp_timeout_set(ppi, index, millisec);
}

/*
 * When we enter a new fsm state, we init all timeouts. Who cares if
 * some of them are not used (and even if some have no default timeout)
 */
void pp_timeout_setall(struct pp_instance *ppi)
{
	int i;
	for (i = 0; i < __PP_TO_ARRAY_SIZE; i++)
		pp_timeout_set(ppi, i);
	/* but announce_send must be send soon */
	__pp_timeout_set(ppi, PP_TO_ANN_SEND, 20);
}

void pp_timeout_clr(struct pp_instance *ppi, int index)
{
	ppi->timeouts[index] = 0;
}

int pp_timeout(struct pp_instance *ppi, int index)
{
	int ret = ppi->timeouts[index] &&
		time_after_eq(ppi->t_ops->calc_timeout(ppi, 0),
			      ppi->timeouts[index]);

	if (ret)
		pp_timeout_log(ppi, index);
	return ret;
}

int pp_timeout_z(struct pp_instance *ppi, int index)
{
	int ret = pp_timeout(ppi, index);

	if (ret)
		pp_timeout_clr(ppi, index);
	return ret;
}

/* how many ms to wait for the timeout to happen, for ppi->next_delay */
int pp_ms_to_timeout(struct pp_instance *ppi, int index)
{
	signed long ret;

	if (!ppi->timeouts[index]) /* not pending, nothing to wait for */
		return 0;

	ret = ppi->timeouts[index] - ppi->t_ops->calc_timeout(ppi, 0);
	return ret <= 0 ? 0 : ret;
}

/* called several times, only sets a timeout, so inline it here */
void pp_timeout_restart_annrec(struct pp_instance *ppi)
{
	pp_timeout_set(ppi, PP_TO_ANN_RECEIPT);
}
