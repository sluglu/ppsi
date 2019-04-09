/*
 * Copyright (C) 2011 CERN (www.cern.ch)
 * Author: Jean-Claude BAU
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
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
	struct pp_time t = {
			.secs=0,
			.scaled_nsecs=t2
	};
	normalize_pp_time(&t);
	pp_time_add(t1,&t);
}

/* Substract a TimeInterval to a pp_time */
void pp_time_sub_interval(struct pp_time *t1, TimeInterval t2)
{
	struct pp_time t = {
			.secs=0,
			.scaled_nsecs=t2
	};
	normalize_pp_time(&t);
	pp_time_sub(t1,&t);
}

#define IS_ADD_WILL_OVERFLOW(a,b) \
		( (((a > 0) && (b > INT64_MAX - a))) ||\
		  (((a < 0) && (b < INT64_MIN - a)))   \
		)

void pp_time_add(struct pp_time *t1, struct pp_time *t2)
{
	t1->secs += t2->secs;
	t1->scaled_nsecs += t2->scaled_nsecs;
	normalize_pp_time(t1);
}

#define IS_SUB_WILL_OVERFLOW(a,b) \
		( ((b < 0) && (a > INT64_MAX + b)) ||\
		  ((b > 0) && (a < INT64_MIN + b))   \
		)

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
	return ts->secs * PP_PSEC_PER_SEC
		+ ((ts->scaled_nsecs * 1000 + TIME_ROUNDING_VALUE) >> TIME_FRACBITS);
}

void fixedDelta_to_pp_time(struct FixedDelta fd, struct pp_time *t) {
	/* FixedDelta is expressed in ps*2^16 */
	uint64_t *v=(uint64_t*)&fd;
	t->scaled_nsecs=*v/1000L; /* We can do it because scaled_nsecs is also multiply by 2^16 */
	t->secs=0;
	normalize_pp_time(t);
}

void picos_to_pp_time(int64_t picos, struct pp_time *ts)
{
	uint64_t sec, nsec;
	int sign = (picos < 0 ? -1 : 1);

	picos *= sign;
	sec=picos/PP_PSEC_PER_SEC;
	picos-=sec*PP_PSEC_PER_SEC;
	nsec = picos/1000;
	picos-=nsec*1000;

	ts->scaled_nsecs = nsec << TIME_FRACBITS;
	ts->scaled_nsecs += (picos << TIME_FRACBITS) / 1000;
	ts->scaled_nsecs *= sign;
	ts->secs = sec * sign;
}

#if 0
/* "Hardwarizes" the timestamp - e.g. makes the nanosecond field a multiple
 * of 8/16ns cycles and puts the extra nanoseconds in the picos result */
void pp_time_hardwarize(struct pp_time *time, int clock_period_ps,
			  int32_t *ticks, int32_t *picos)
{
	int32_t s, ns, ps, clock_ns;

	if ( clock_period_ps <= 0 ) {
		pp_error("%s : Invalid clock period %d\n",__func__, clock_period_ps);
		exit(-1);
	}
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
#else
/* "Hardwarizes" the timestamp - e.g. makes the nanosecond field a multiple
 * of 8/16ns cycles and puts the extra nanoseconds in the picos result */
void pp_time_hardwarize(struct pp_time *time, int clock_period_ps,
			  int32_t *ticks, int32_t *picos)
{
	int64_t ps, adj_ps;
	int32_t sign=(time->scaled_nsecs<0) ? -1 : 1;
	int64_t scaled_nsecs=time->scaled_nsecs*sign;

	if ( clock_period_ps <= 0 ) {
		pp_error("%s : Invalid clock period %d\n",__func__, clock_period_ps);
		return;
	}
	ps = (scaled_nsecs * 1000L+TIME_INTERVAL_ROUNDING_VALUE) >> TIME_INTERVAL_FRACBITS; /* now picoseconds 0..999 -- positive*/
	adj_ps=ps - ps%clock_period_ps;
	*picos=(int32_t)(ps-adj_ps)*sign;
	*ticks = (int32_t)(adj_ps/1000)*sign;
}
#endif

TimeInterval pp_time_to_interval(struct pp_time *ts)
{
	return ((ts->secs * PP_NSEC_PER_SEC)<<TIME_INTERVAL_FRACBITS) + ts->scaled_nsecs ;
}

TimeInterval picos_to_interval(int64_t picos)
{
	if ( picos <= TIME_INTERVAL_MIN_PICOS_VALUE_AS_INT64) {
		/* special case : Return the minimum value */
		/* The evaluation is made at compilation time */
		return (TimeInterval) 1<<(64-1); /* Maximum negative value */
	}
	if (  picos >= TIME_INTERVAL_MAX_PICOS_VALUE_AS_INT64 ) {
		/* special case : Return the maximum */
		/* The evaluation is made at compilation time */
		return (TimeInterval) (((TIME_INTERVAL_MAX_PICOS_VALUE_AS_INT64/1000) <<  TIME_INTERVAL_FRACBITS)|
				(((TIME_INTERVAL_MAX_PICOS_VALUE_AS_INT64%1000) << TIME_INTERVAL_FRACBITS)/1000));
	} else {

		int64_t scaled_ns;

		int sign = (picos < 0 ? -1 : 1);
		picos *= sign;
		scaled_ns=(picos/1000) << TIME_INTERVAL_FRACBITS; /* Calculate nanos */
		scaled_ns+=((picos%1000) << TIME_INTERVAL_FRACBITS)/1000; /* Add picos */

		return scaled_ns*sign;
	}
}

int64_t interval_to_picos(TimeInterval interval)
{
	return (interval * 1000) >>  TIME_INTERVAL_FRACBITS;
}

/*
 * Check the timestamps (t1 to t6).
 * err_count is a counter to avoid printing the first time the error messages
 * ts_mask is the mask of the timestamps to check.
 * Returns 1 if a timestamp is incorrect otherwise 0.
 */
int is_timestamps_incorrect(struct pp_instance *ppsi, int *err_count, int ts_mask) {
	static int ts_ppsi_offset[6]={
			offsetof(struct pp_instance,t1),
			offsetof(struct pp_instance,t2),
			offsetof(struct pp_instance,t3),
			offsetof(struct pp_instance,t4),
			offsetof(struct pp_instance,t5),
			offsetof(struct pp_instance,t6)
	};
	int mask=1,i,local_err_count;

	if ( err_count==NULL ) {
		/* Optimize the execution of the next code avoiding checking err_count at each step */
		err_count=&local_err_count;
		local_err_count=5; /* Force printing immediately if a timestamp is incorrect */
	}

	for ( i=0; i<6 && ts_mask; i++ ) { // Check up to 6 timestamps
		if ( mask & ts_mask ) {
			struct pp_time *ts=(void *) ppsi + ts_ppsi_offset[i] ;
			if ( is_incorrect(ts) ) {
				if ( (*err_count)++ > 5 ) {
					pp_error("%s: t%d is incorrect\n",__func__,i+1);
				}
				return 1;
			}
		}
		ts_mask &=~mask;
		mask<<=1;
	}
	return *err_count=0;
}

static char time_as_string[32];

/* Convert pp_time to string */
char *time_to_string(struct pp_time *t)
{
	struct pp_time time=*t;
	int picos;
	char *sign;

	if (t->secs < 0 || (t->secs == 0 && t->scaled_nsecs < 0) ) {
		sign="-";
		time.scaled_nsecs=-t->scaled_nsecs;
		time.secs=-t->secs;
	} else {
		sign="";
		time=*t;
	}
	picos=((int)(time.scaled_nsecs&TIME_FRACMASK)*1000+TIME_ROUNDING_VALUE)>>TIME_FRACBITS;
	pp_sprintf(time_as_string, "%s%d.%09d%03d",
		   sign,
		   (int)time.secs,
		   (int)(time.scaled_nsecs >> TIME_FRACBITS),
		   picos);
	return time_as_string;
}

/* Convert TimeInterval to string */
char *interval_to_string(TimeInterval time)
{
	int64_t sign,nanos,picos;

	if ( time<0 && time !=INT64_MIN) {
		sign=-1;
		time=-time;
	} else {
		sign=1;
	}
	nanos = time >> TIME_INTERVAL_FRACBITS;
	picos = (((time & TIME_INTERVAL_FRACMASK) * 1000) + TIME_INTERVAL_ROUNDING_VALUE ) >> TIME_INTERVAL_FRACBITS;
	sprintf(time_as_string,"%" PRId64 ".%03" PRId64, sign*nanos,picos);
	return time_as_string;
}

/* Convert RelativeInterval to string */
char *relative_interval_to_string(TimeInterval time) {
    int32_t nsecs=time >> REL_DIFF_FRACBITS;
	uint64_t sub_yocto=0;
    int64_t fraction;
	uint64_t bitWeight=500000000000000000;
	uint64_t mask;


    fraction=time & REL_DIFF_FRACMASK;
	for (mask=(uint64_t) 1<< (REL_DIFF_FRACBITS-1);mask!=0; mask>>=1 ) {
		if ( mask & fraction )
			sub_yocto+=bitWeight;
		bitWeight/=2;
	}
	sprintf(time_as_string,"%"PRId32".%018"PRIu64, nsecs, sub_yocto);
	return time_as_string;
}
