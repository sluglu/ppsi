/*
 * Copyright (C) 2018 CERN (www.cern.ch)
 * Author: Jean-Claude BAU & Maciej Lipinski
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */

#include <ppsi/ppsi.h>
#include "l1e-api.h"
#include "l1e-constants.h"
#include <libwr/shmem.h>

static const char *l1e_servo_state_name[] = {
	[L1E_UNINITIALIZED] = "Uninitialized",
	[L1E_SYNC_NSEC] = "SYNC_NSEC",
	[L1E_SYNC_TAI] = "SYNC_SEC",
	[L1E_SYNC_PHASE] = "SYNC_PHASE",
	[L1E_TRACK_PHASE] = "TRACK_PHASE",
	[L1E_WAIT_OFFSET_STABLE] = "WAIT_OFFSET_STABLE",
};

/* Enable tracking by default. Disabling the tracking is used for demos. */
static int l1e_tracking_enabled = 1;
static struct pp_time l1e_faulty_stamps[6]; /* if unused, dropped at link time */
static int l1e_got_sync = 0;


/* prototypes */
static int l1e_p2p_delay(struct pp_instance *ppi, struct l1e_servo_state *s);
static void l1e_apply_faulty_stamp(struct l1e_servo_state *s, int index);
static void l1e_dump_timestamp(struct pp_instance *ppi, char *what,	struct pp_time ts);

/* External data */
extern struct wrs_shm_head *ppsi_head;


static void l1e_apply_faulty_stamp(struct l1e_servo_state *s, int index)
{
	if (PROTO_EXT_HAS_FAULTS) {
		assert(index >= 1 && index <= 6, "Wrong T index %i\n", index);
		pp_time_add(&s->t1 + index - 1, l1e_faulty_stamps + index - 1);
	}
}

static void l1e_dump_timestamp(struct pp_instance *ppi, char *what,struct pp_time ts)
{
	pp_diag(ppi, servo, 2, "%s = %ld:%09ld:%03ld\n", what, (long)ts.secs,
		(long)(ts.scaled_nsecs >> 16),
		/* unlikely what we had earlier, third field is not phase */
		((long)(ts.scaled_nsecs & 0xffff) * 1000 + 0x8000) >> 16);
}

void l1e_servo_enable_tracking(int enable)
{
	l1e_tracking_enabled = enable;
}

int l1e_servo_init(struct pp_instance *ppi)
{
	struct l1e_servo_state *s=L1E_SRV(ppi);

	/* shmem lock */
	wrs_shm_write(ppsi_head, WRS_SHM_WRITE_BEGIN);

	/* Update correction data in data sets*/
	if (l1e_update_correction_values(ppi) < 0)
		return -1;

	WRH_OPER()->enable_timing_output(ppi, 0);

	/*
	 * Do not reset cur_setpoint, but trim it to be less than one tick.
	 * The softpll code uses the module anyways, but if we unplug-replug
	 * the fiber it will always increase, so don't scare the user
	 */
	if (s->cur_setpoint_ps > s->clock_period_ps)
		s->cur_setpoint_ps %= s->clock_period_ps;

	pp_diag(ppi, servo, 3, "%s.%d: Adjust_phase: %d\n",__func__,__LINE__,s->cur_setpoint_ps);

	WRH_OPER()->adjust_phase(s->cur_setpoint_ps);
	s->missed_iters = 0;
	SRV(ppi)->state = L1E_SYNC_TAI;

	strcpy(SRV(ppi)->servo_state_name, l1e_servo_state_name[SRV(ppi)->state]);

	SRV(ppi)->flags |= PP_SERVO_FLAG_VALID;
	SRV(ppi)->update_count = 0;
	ppi->t_ops->get(ppi, &s->update_time);
	s->tracking_enabled = l1e_tracking_enabled;

	l1e_got_sync = 0;

	/* shmem unlock */
	wrs_shm_write(ppsi_head, WRS_SHM_WRITE_END);
	return 0;
}


void l1e_servo_reset(struct pp_instance *ppi)
{
	/* values from servo_state to be preserved */
	uint32_t n_err_state;
	uint32_t n_err_offset;
	uint32_t n_err_delta_rtt;

	struct l1e_servo_state *s=L1E_SRV(ppi);

	if (!s) {
		/* Don't clean servo state when is not available */
		return;
	}
	/* shmem lock */
	wrs_shm_write(ppsi_head, WRS_SHM_WRITE_BEGIN);
	ppi->flags = 0;

	/* preserve some values from servo_state */
	n_err_state = s->n_err_state;
	n_err_offset = s->n_err_offset;
	n_err_delta_rtt = s->n_err_delta_rtt;
	/* clear servo_state to display empty fields in wr_mon and SNMP */
	memset(s, 0, sizeof(struct l1e_servo_state));
	/* restore values from servo_state */
	s->n_err_state = n_err_state;
	s->n_err_offset = n_err_offset;
	s->n_err_delta_rtt = n_err_delta_rtt;

	/* shmem unlock */
	wrs_shm_write(ppsi_head, WRS_SHM_WRITE_END);
}

int l1e_servo_got_sync(struct pp_instance *ppi, struct pp_time *t1,
		      struct pp_time *t2)
{
	struct l1e_servo_state *s=L1E_SRV(ppi);

	s->t1 = *t1; l1e_apply_faulty_stamp(s, 1);
	s->t2 = *t2; l1e_apply_faulty_stamp(s, 2);
	l1e_got_sync = 1;

	return 0;
}

int l1e_servo_got_delay(struct pp_instance *ppi)
{
	struct l1e_servo_state *s=L1E_SRV(ppi);

	wrs_shm_write(ppsi_head, WRS_SHM_WRITE_BEGIN);

	s->t3 = ppi->t3; l1e_apply_faulty_stamp(s, 3);
	s->t4 = ppi->t4; l1e_apply_faulty_stamp(s, 4);

	if (CONFIG_HAS_P2P && ppi->delayMechanism == P2P) {
		s->t5 = ppi->t5; l1e_apply_faulty_stamp(s, 5);
		s->t6 = ppi->t6; l1e_apply_faulty_stamp(s, 6);

		l1e_p2p_delay(ppi, s);
	}

	wrs_shm_write(ppsi_head, WRS_SHM_WRITE_END);
	return 0;
}

/* update currentDS.meanDelay */
static void l1e_update_meanDelay(struct pp_instance *ppi,struct l1e_servo_state *s) {
	struct pp_time mtime;
	mtime=s->delayMM;
	pp_time_div2(&mtime);
	DSCUR(ppi)->meanDelay=pp_time_to_interval(&mtime);

}

/* update currentDS.delayAsymmetry */
static void l1e_update_delayAsymmetry(struct pp_instance *ppi,struct l1e_servo_state *s) {
	DSPOR(ppi)->delayAsymmetry=picos_to_interval(s->delayMM_ps - 2LL * s->delayMS_ps);
}

static void l1e_update_offsetFromMaster (struct pp_instance *ppi,struct pp_time *offsetMS) {
	DSCUR(ppi)->offsetFromMaster = pp_time_to_interval(offsetMS);
}

static int l1e_p2p_delay(struct pp_instance *ppi, struct l1e_servo_state *s)
{
	static int errcount;

	if (is_incorrect(&s->t3) || is_incorrect(&s->t4)
	    || is_incorrect(&s->t5) || is_incorrect(&s->t6)) {
		errcount++;
		if (errcount > 5)	/* a 2-3 in a row are expected */
			pp_error("%s: TimestampsIncorrect: %d %d %d %d\n",
				 __func__, !is_incorrect(&s->t3),
				 !is_incorrect(&s->t4), !is_incorrect(&s->t5),
				 !is_incorrect(&s->t6));
		return 0;
	}
	errcount = 0;

	SRV(ppi)->update_count++;
	ppi->t_ops->get(ppi, &s->update_time);

	{ /* avoid modifying stamps in place */
		struct pp_time mtime, stime;

		stime = s->t6; pp_time_sub(&stime, &s->t3);
		mtime = s->t5; pp_time_sub(&mtime, &s->t4);
		s->delayMM = stime; pp_time_sub(&s->delayMM, &mtime);
	}


	if (__PP_DIAG_ALLOW(ppi, pp_dt_servo, 1)) {
		l1e_dump_timestamp(ppi, "servo:t1", s->t1);
		l1e_dump_timestamp(ppi, "servo:t2", s->t2);
		l1e_dump_timestamp(ppi, "servo:t3", s->t3);
		l1e_dump_timestamp(ppi, "servo:t4", s->t4);
		l1e_dump_timestamp(ppi, "servo:t5", s->t5);
		l1e_dump_timestamp(ppi, "servo:t6", s->t6);
		l1e_dump_timestamp(ppi, "->delayMM", s->delayMM);
	}

	s->delayMM_ps = pp_time_to_picos(&s->delayMM);
	/* delayMS[ps] = (fix_alpha*delayMM[ps])/2e40 +delayMM[ps]/2 */
	s->delayMS_ps =
	    ((s->delayMM_ps * ppi->asymmetryCorrectionPortDS.scaledDelayCoefficient) >> REL_DIFF_FRACBITS)
	    + (s->delayMM_ps >> 1);

	return 1;
}

static int l1e_p2p_offset(struct pp_instance *ppi,
		  struct l1e_servo_state *s, struct pp_time *offsetMS)
{
	static int errcount;

	if (is_incorrect(&s->t1) || is_incorrect(&s->t2)) {
		errcount++;
		if (errcount > 5)	/* a 2-3 in a row are expected */
			pp_error("%s: TimestampsIncorrect: %d %d \n",
				 __func__, !is_incorrect(&s->t1),
				 !is_incorrect(&s->t2));
		return 0;
	}
	errcount = 0;
	l1e_got_sync = 0;

	SRV(ppi)->update_count++;
	ppi->t_ops->get(ppi, &s->update_time);

	*offsetMS = s->t1;
	pp_time_sub(offsetMS, &s->t2);
	pp_time_add(offsetMS, &SRV(ppi)->delayMS);

	/* is it possible to calculate it in client,
	 * but then t1 and t2 require shmem locks */

	s->tracking_enabled =  l1e_tracking_enabled;
	return 1;
}

static int l1e_e2e_offset(struct pp_instance *ppi,
		  struct l1e_servo_state *s, struct pp_time *offsetMS)
{
	static int errcount;

	if (is_incorrect(&s->t1) || is_incorrect(&s->t2)
	    || is_incorrect(&s->t3) || is_incorrect(&s->t4)) {
		errcount++;
		if (errcount > 5) /* a 2-3 in a row are expected */
			pp_error("%s: TimestampsIncorrect: %d %d %d %d\n",
				 __func__, !is_incorrect(&s->t1),
				 !is_incorrect(&s->t2), !is_incorrect(&s->t3),
				 !is_incorrect(&s->t4));
		return 0;
	}

	if (WRH_OPER()->servo_hook)
		WRH_OPER()->servo_hook(ppi, WRH_SERVO_ENTER);

	errcount = 0;

	SRV(ppi)->update_count++;
	ppi->t_ops->get(ppi, &s->update_time);

	l1e_got_sync = 0;

	{ /* avoid modifying stamps in place */
		struct pp_time mtime, stime;

		mtime = s->t4; pp_time_sub(&mtime, &s->t1);
		stime = s->t3; pp_time_sub(&stime, &s->t2);
		s->delayMM = mtime; pp_time_sub(&s->delayMM, &stime);
	}

	s->delayMM_ps = pp_time_to_picos(&s->delayMM);

	/* avoid negatives in calculations */
	if ( s->delayMM_ps < 0 ) {
		 s->delayMM_ps=0;
		 picos_to_pp_time(s->delayMM_ps,&s->delayMM);
	}

	if (__PP_DIAG_ALLOW(ppi, pp_dt_servo, 1)) {
		l1e_dump_timestamp(ppi, "t1", s->t1);
		l1e_dump_timestamp(ppi, "t2", s->t2);
		l1e_dump_timestamp(ppi, "t3", s->t3);
		l1e_dump_timestamp(ppi, "t4", s->t4);
		l1e_dump_timestamp(ppi, "delayMM", s->delayMM);
	}

	/* delayMS = (delayMM * fix_alpha) + delayMM/2*/
	s->delayMS_ps = ((s->delayMM_ps * ppi->asymmetryCorrectionPortDS.scaledDelayCoefficient) >> REL_DIFF_FRACBITS) + (s->delayMM_ps >> 1);

	{ /* offsetMS = t1-t2 + delayMS */
		struct pp_time tmp1;

		*offsetMS = s->t1; pp_time_sub(offsetMS, &s->t2);
	    picos_to_pp_time(s->delayMS_ps,&tmp1);
	    pp_time_add(offsetMS, &tmp1);
	}

	s->tracking_enabled =  l1e_tracking_enabled;

	picos_to_pp_time(s->delayMS_ps,&SRV(ppi)->delayMS);

	l1e_update_meanDelay(ppi,s); /* update currentDS.meanDelay */
	l1e_update_delayAsymmetry(ppi,s); /* update currentDS.delayAsymmetry */

	return 1;
}

int l1e_servo_update(struct pp_instance *ppi)
{
	struct l1e_servo_state *s=L1E_SRV(ppi);
	int remaining_offset;
	int32_t  offset_ticks;
	int64_t prev_delayMM_ps = 0;
	int locking_poll_ret;

	struct pp_time offsetMS ;
	int32_t  offset_ps;

	if (!l1e_got_sync)
		return 0;

	/* shmem lock */
	wrs_shm_write(ppsi_head, WRS_SHM_WRITE_BEGIN);

	if ( SRV(ppi)->state==L1E_UNINITIALIZED )
		goto out;

	prev_delayMM_ps = s->delayMM_ps;
	if (CONFIG_HAS_P2P && ppi->delayMechanism == P2P) {
		if (!l1e_p2p_offset(ppi, s, &offsetMS))
			goto out;
	} else {
		if (!l1e_e2e_offset(ppi, s, &offsetMS))
			goto out;
	}

	l1e_update_offsetFromMaster(ppi,&offsetMS); /* Update currentDS.offsetFromMaster */
	s->offsetMS_ps=pp_time_to_picos(&offsetMS);
	pp_diag(ppi, servo, 2,
			"ML: scaledDelayCoeff = %lld, delayMS = %lld, offsetMS = %lld [ps]\n",
			(long long )ppi->asymmetryCorrectionPortDS.scaledDelayCoefficient,
			(long long )s->delayMS_ps,
			(long long )s->offsetMS_ps);
	l1e_dump_timestamp(ppi,"ML: offsetMS",offsetMS);


	pp_time_hardwarize(&offsetMS, s->clock_period_ps,
		      &offset_ticks, &offset_ps);
	pp_diag(ppi, servo, 2, "offsetMS: %li.%09li (+%li)\n",
		(long)offsetMS.secs, (long)offset_ticks,
		(long)offset_ps);

	locking_poll_ret = WRH_OPER()->locking_poll(ppi, 0);
	if (locking_poll_ret != WRH_SPLL_READY
	    && locking_poll_ret != WRH_SPLL_CALIB_NOT_READY) {
		pp_diag(ppi, servo, 1, "PLL OutOfLock, should restart sync\n");
		WRH_OPER()->enable_timing_output(ppi, 0);
		/* TODO check
		 * DSPOR(ppi)->doRestart = TRUE; */
	}

	/* After each action on the hardware, we must verify if it is over. */
	if (!WRH_OPER()->adjust_in_progress()) {
		SRV(ppi)->flags &= ~PP_SERVO_FLAG_WAIT_HW;
	} else {
		pp_diag(ppi, servo, 1, "servo:busy\n");
		goto out;
	}

	/* So, we didn't return. Choose the right state */
	if (offsetMS.secs) /* so bad... */
		SRV(ppi)->state = L1E_SYNC_TAI;
	else if (offset_ticks) /* not that bad */
		SRV(ppi)->state = L1E_SYNC_NSEC;
	/* else, let the states below choose the sequence */

	pp_diag(ppi, servo, 2, "offsetMS: %li.%09li (+%li)\n",
		(long)offsetMS.secs, (long)offset_ticks,
		(long)offset_ps);

	pp_diag(ppi, servo, 1, "l1e_servo state: %s%s\n",
		l1e_servo_state_name[SRV(ppi)->state],
		SRV(ppi)->flags & PP_SERVO_FLAG_WAIT_HW ? " (wait for hw)" : "");

	/* update string state name */
	strcpy(SRV(ppi)->servo_state_name, l1e_servo_state_name[SRV(ppi)->state]);

	switch (SRV(ppi)->state) {
	case L1E_SYNC_TAI:
		WRH_OPER()->adjust_counters(offsetMS.secs, 0);
		SRV(ppi)->flags |= PP_SERVO_FLAG_WAIT_HW;
		/*
		 * If nsec wrong, code above forces SYNC_NSEC,
		 * Else, we must ensure we leave this status towards
		 * fine tuning
		 */
		SRV(ppi)->state = L1E_SYNC_PHASE;
		break;

	case L1E_SYNC_NSEC:
		WRH_OPER()->adjust_counters(0, offset_ticks);
		SRV(ppi)->flags |= PP_SERVO_FLAG_WAIT_HW;
		SRV(ppi)->state = L1E_SYNC_PHASE;
		break;

	case L1E_SYNC_PHASE:
		pp_diag(ppi, servo, 2, "oldsetp %i, offset %i:%04i\n",
			s->cur_setpoint_ps, offset_ticks,
			offset_ps);
		s->cur_setpoint_ps += offset_ps;
		pp_diag(ppi, servo, 3, "%s.%d: Adjust_phase: %d\n",__func__,__LINE__,s->cur_setpoint_ps);
		WRH_OPER()->adjust_phase(s->cur_setpoint_ps);

		SRV(ppi)->flags |= PP_SERVO_FLAG_WAIT_HW;
		SRV(ppi)->state = L1E_WAIT_OFFSET_STABLE;

		if (ARCH_IS_WRS) {
			/*
			 * Now, let's fix system time. We pass here
			 * once only, so that's the best place to do
			 * it. We can't use current WR time, as we
			 * still miss the method to get it (through IPC).
			 * So use T4, which is a good approximation.
			 */
			unix_time_ops.set(ppi, &ppi->t4);
			pp_diag(ppi, time, 1, "system time set to %li TAI\n",
				(long)ppi->t4.secs);
		}
		break;

	case L1E_WAIT_OFFSET_STABLE:

		/* ts_to_picos() below returns phase alone */
		remaining_offset = abs(pp_time_to_picos(&offsetMS));
		if(remaining_offset < WRH_SERVO_OFFSET_STABILITY_THRESHOLD) {
			WRH_OPER()->enable_timing_output(ppi, 1);
			s->prev_delayMS_ps = s->delayMS_ps;
			SRV(ppi)->state = L1E_TRACK_PHASE;
		} else {
			s->missed_iters++;
		}
		if (s->missed_iters >= 10) {
			s->missed_iters = 0;
			SRV(ppi)->state = L1E_SYNC_PHASE;
		}
		break;

	case L1E_TRACK_PHASE:
		s->skew_ps = s->delayMS_ps - s->prev_delayMS_ps;

		/* Can be disabled for manually tweaking and testing */
		if(l1e_tracking_enabled) {
			if (abs(offset_ps) >
			    2 * WRH_SERVO_OFFSET_STABILITY_THRESHOLD) {
				SRV(ppi)->state = WR_SYNC_PHASE;
				break;
			}

			// adjust phase towards offset = 0 make ck0 0
			s->cur_setpoint_ps += (offset_ps / 4);

			pp_diag(ppi, servo, 3, "%s.%d: Adjust_phase: %d\n",__func__,__LINE__,s->cur_setpoint_ps);
			WRH_OPER()->adjust_phase(s->cur_setpoint_ps);
			pp_diag(ppi, time, 1, "adjust phase %i\n",
				s->cur_setpoint_ps);

			s->prev_delayMS_ps = s->delayMS_ps;
		}
		break;

	}

	SRV(ppi)->servo_locked=SRV(ppi)->state==L1E_TRACK_PHASE;

	/* Increase number of servo updates with state different than
	 * L1E_TRACK_PHASE. (Used by SNMP) */
	if (SRV(ppi)->state != L1E_TRACK_PHASE)
		s->n_err_state++;

	/* Increase number of servo updates with offset exceeded
	 * SNMP_MAX_OFFSET_PS (Used by SNMP) */
	if (abs(s->offsetMS_ps) > SNMP_MAX_OFFSET_PS)
		s->n_err_offset++;

	/* Increase number of servo updates with delta rtt exceeded
	 * SNMP_MAX_DELTA_RTT_PS (Used by SNMP) */
	if (abs(prev_delayMM_ps - s->delayMM_ps) > SNMP_MAX_DELTA_RTT_PS)
		s->n_err_delta_rtt++;

out:
	/* shmem unlock */
	wrs_shm_write(ppsi_head, WRS_SHM_WRITE_END);

	if (WRH_OPER()->servo_hook)
		WRH_OPER()->servo_hook(ppi, WRH_SERVO_LEAVE);

	return 0;
}
