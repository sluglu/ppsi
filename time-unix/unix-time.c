/*
 * Copyright (C) 2011 CERN (www.cern.ch)
 * Author: Alessandro Rubini
 *
 * Released to the public domain
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/timex.h>
#include <ppsi/ppsi.h>

#ifndef MOD_TAI
#define MOD_TAI 0x80
#endif

static void clock_fatal_error(char *context)
{
	pp_error("failure in \"%s\": %s\n.Exiting.\n", context,
		  strerror(errno));
	exit(1);
}

static int unix_time_get_utc_offset(struct pp_instance *ppi, int *offset)
{
	struct timex t;
	/*
	 * Get the UTC/TAI difference
	 */
	memset(&t, 0, sizeof(t));
	if (adjtimex(&t) >= 0) {
		/*
		 * Our WRS kernel has tai support, but our compiler does not.
		 * We are 32-bit only, and we know for sure that tai is
		 * exactly after stbcnt. It's a bad hack, but it works
		 */
		*offset = *((int *)(&t.stbcnt) + 1);
		return 0;
	} else {
		*offset = 0;
		return -1;
	}		
}

static int unix_time_get(struct pp_instance *ppi, struct pp_time *t)
{
	struct timespec tp;
	if (clock_gettime(CLOCK_REALTIME, &tp) < 0)
		clock_fatal_error("clock_gettime");
	/* TAI = UTC + 35 */
	t->secs = tp.tv_sec + DSPRO(ppi)->currentUtcOffset;
	t->scaled_nsecs = ((int64_t)tp.tv_nsec) << 16;
	if (!(pp_global_d_flags & PP_FLAG_NOTIMELOG))
		pp_diag(ppi, time, 2, "%s: %9li.%09li\n", __func__,
			tp.tv_sec, tp.tv_nsec);
	return 0;
}

static int unix_time_set(struct pp_instance *ppi, const struct pp_time *t)
{
	struct timespec tp;

	if (!t) { /* Change the network notion of the utc/tai offset */
		struct timex t;

		t.modes = MOD_TAI;
		t.constant = DSPRO(ppi)->currentUtcOffset;
		if (adjtimex(&t) < 0)
			clock_fatal_error("change TAI offset");
		pp_diag(ppi, time, 1, "New TAI offset: %i\n",
			DSPRO(ppi)->currentUtcOffset);
		return 0;
	}

	/* UTC = TAI - 35 */
	tp.tv_sec = t->secs - DSPRO(ppi)->currentUtcOffset;
	tp.tv_nsec = t->scaled_nsecs >> 16;
	if (clock_settime(CLOCK_REALTIME, &tp) < 0)
		clock_fatal_error("clock_settime");
	pp_diag(ppi, time, 1, "%s: %9li.%09li\n", __func__,
		tp.tv_sec, tp.tv_nsec);
	return 0;
}

static int unix_time_init_servo(struct pp_instance *ppi)
{
	struct timex t;

	/* We must set MOD_PLL and recover the current frequency value */
	t.modes = MOD_STATUS;
	t.status = STA_PLL;
	if (adjtimex(&t) < 0)
		return -1;
	return (t.freq >> 16) * 1000; /* positive or negative, not -1 */
}

static int unix_time_adjust(struct pp_instance *ppi, long offset_ns, long freq_ppb)
{
	struct timex t;
	int ret;

	t.modes = 0;

	if (freq_ppb) {
		if (freq_ppb > PP_ADJ_FREQ_MAX)
			freq_ppb = PP_ADJ_FREQ_MAX;
		if (freq_ppb < -PP_ADJ_FREQ_MAX)
			freq_ppb = -PP_ADJ_FREQ_MAX;

		t.freq = freq_ppb * ((1 << 16) / 1000);
		t.modes = MOD_FREQUENCY;
	}

	if (offset_ns) {
		t.offset = offset_ns / 1000;
		t.modes |= MOD_OFFSET;
	}

	ret = adjtimex(&t);
	pp_diag(ppi, time, 1, "%s: %li %li\n", __func__, offset_ns, freq_ppb);
	return ret;
}

static int unix_time_adjust_offset(struct pp_instance *ppi, long offset_ns)
{
	return unix_time_adjust(ppi, offset_ns, 0);
}

static int unix_time_adjust_freq(struct pp_instance *ppi, long freq_ppb)
{
	return unix_time_adjust(ppi, 0, freq_ppb);
}

static unsigned long unix_calc_timeout(struct pp_instance *ppi, int millisec)
{
	struct timespec now;
	uint64_t now_ms;

	clock_gettime(CLOCK_MONOTONIC, &now);
	now_ms = 1000LL * now.tv_sec + now.tv_nsec / 1000 / 1000;

	return now_ms + millisec;
}

struct pp_time_operations unix_time_ops = {
	.get_utc_offset = unix_time_get_utc_offset,
	.get = unix_time_get,
	.set = unix_time_set,
	.adjust = unix_time_adjust,
	.adjust_offset = unix_time_adjust_offset,
	.adjust_freq = unix_time_adjust_freq,
	.init_servo = unix_time_init_servo,
	.calc_timeout = unix_calc_timeout,
};
