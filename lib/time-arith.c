/*
 * Copyright (C) 2011 CERN (www.cern.ch)
 * Author: Jean-Claude BAU
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */

#include <limits.h>
#include <ppsi/ppsi.h>

void normalize_pp_time(struct pp_time *t)
{
	/* no 64b division please, we'll rather loop a few times */
	#define SNS_PER_S ((1000LL * 1000 * 1000) << 16)

	/* For whatever reason we perform a normalization on an incorrect
	 * timestamp, don't treat is as an negative value. */
	int64_t sign = ((t->secs < 0 && !is_incorrect(t))
			|| (t->secs == 0 && t->scaled_nsecs < 0))
		? -1 : 1;

	/* turn into positive, to make code shorter (don't replicate loops) */
	t->secs *= sign;
	t->scaled_nsecs *= sign;

	while (t->scaled_nsecs < 0) {
		t->secs--;
		t->scaled_nsecs += SNS_PER_S;
	}
	while (t->scaled_nsecs > SNS_PER_S) {
		t->secs++;
		t->scaled_nsecs -= SNS_PER_S;
	}
	t->secs *= sign;
	t->scaled_nsecs *= sign;
}

/* Add a TimeInterval to a pp_time */
void pp_time_add_interval(struct pp_time *t1, TimeInterval t2)
{
	t1->scaled_nsecs += t2;
	normalize_pp_time(t1);
}

/* Substract a TimeInterval to a pp_time */
void pp_time_sub_interval(struct pp_time *t1, TimeInterval t2)
{
	t1->scaled_nsecs -= t2;
	normalize_pp_time(t1);
}

void pp_time_add(struct pp_time *t1, struct pp_time *t2)
{
	t1->secs += t2->secs;
	t1->scaled_nsecs += t2->scaled_nsecs;
	normalize_pp_time(t1);
}

void pp_time_sub(struct pp_time *t1, struct pp_time *t2)
{
	t1->secs -= t2->secs;
	t1->scaled_nsecs -= t2->scaled_nsecs;
	normalize_pp_time(t1);
}

void pp_time_div2(struct pp_time *t)
{
	int sign = (t->secs < 0) ? -1 : 1;

	t->scaled_nsecs >>= 1;
	if (t->secs &1)
		t->scaled_nsecs += sign * SNS_PER_S / 2;
	t->secs >>= 1;
}


int64_t pp_time_to_picos(struct pp_time *ts)
{
	return ts->secs * PP_NSEC_PER_SEC
		+ ((ts->scaled_nsecs * 1000 + 0x8000) >> TIME_INTERVAL_FRACBITS);
}

void picos_to_pp_time(int64_t picos, struct pp_time *ts)
{
	uint64_t sec, nsec;
	int phase;
	int sign = (picos < 0 ? -1 : 1);

	picos *= sign;
	nsec = picos;
	phase = __div64_32(&nsec, 1000);
	sec = nsec;

	ts->scaled_nsecs = ((int64_t)__div64_32(&sec, PP_NSEC_PER_SEC)) << TIME_INTERVAL_FRACBITS;
	ts->scaled_nsecs += (phase << TIME_INTERVAL_FRACBITS) / 1000;
	ts->scaled_nsecs *= sign;
	ts->secs = sec * sign;
}


/* "Hardwarizes" the timestamp - e.g. makes the nanosecond field a multiple
 * of 8/16ns cycles and puts the extra nanoseconds in the picos result */
void pp_time_hardwarize(struct pp_time *time, int clock_period_ps,
			  int32_t *ticks, int32_t *picos)
{
	int32_t s, ns, ps, clock_ns;

	/* clock_period_ps *must* be a multiple of 1000 -- assert()? */
	clock_ns = clock_period_ps / 1000;

	/*
	 * In pp_time, both sec/nsec are positive, or both negative.
	 * Only 0 secs can have positive or negative nsecs.
	 *
	 * Here we need a positive count for both tick and picos. Or not.
	 * The code here replicates what found in original WR code.
	 */
	s = time->secs; /* a difference: known to fit 32 bits (really?) */
	ps = time->scaled_nsecs & 0xffff; /* fractional nano */
	ps = (ps * 1000) >> TIME_INTERVAL_FRACBITS; /* now picoseconds 0..999 -- positive*/
	ns = time->scaled_nsecs >> TIME_INTERVAL_FRACBITS;
	if (ns > 0 && clock_ns) {
		ps += (ns % clock_ns) * 1000;
		ns -= (ns % clock_ns);
	}
	if (ns < 0) {
		s--;
		ns += PP_NSEC_PER_SEC;
	}
	if (s == -1 && ns > 0) {
		s++;
		ns -= PP_NSEC_PER_SEC;
	}
	if (ns < 0 && s == 0 && ns >= -clock_ns) {
		/* originally, ns was a multiple of clock_ns, code differs */
		ps += ns * 1000;
		ns = 0;
	}
	*ticks = ns;
	*picos = ps;
}


TimeInterval pp_time_to_interval(struct pp_time *ts)
{
	return ((ts->secs * PP_NSEC_PER_SEC)<<TIME_INTERVAL_FRACBITS) + ts->scaled_nsecs ;
}

TimeInterval picos_to_interval(int64_t picos)
{
	return (picos << TIME_INTERVAL_FRACBITS)/1000;
}

int64_t interval_to_picos(TimeInterval interval)
{
	return (interval * 1000) >>  TIME_INTERVAL_FRACBITS;
}
