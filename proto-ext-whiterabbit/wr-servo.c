#include <ppsi/ppsi.h>
#include <ppsi/assert.h>
#include <libwr/shmem.h>
#include "../proto-standard/common-fun.h"

/* prototypes */
static int wr_p2p_delay(struct pp_instance *ppi, struct wr_servo_state *s);
static int wr_e2e_offset(struct pp_instance *ppi,
		  struct wr_servo_state *s, struct pp_time *offset_hw);
static int wr_p2p_offset(struct pp_instance *ppi,
		  struct wr_servo_state *s, struct pp_time *offset_hw);

/* Define threshold values for SNMP */
#define SNMP_MAX_OFFSET_PS 500
#define SNMP_MAX_DELTA_RTT_PS 1000

static const char *servo_name[] = {
	[WR_UNINITIALIZED] = "Uninitialized",
	[WR_SYNC_NSEC] = "SYNC_NSEC",
	[WR_SYNC_TAI] = "SYNC_SEC",
	[WR_SYNC_PHASE] = "SYNC_PHASE",
	[WR_TRACK_PHASE] = "TRACK_PHASE",
	[WR_WAIT_OFFSET_STABLE] = "WAIT_OFFSET_STABLE",
};


static void dump_timestamp(struct pp_instance *ppi, char *what,
			   struct pp_time ts)
{
	pp_diag(ppi, servo, 2, "%s = %ld:%09ld:%03ld\n", what, (long)ts.secs,
		(long)(ts.scaled_nsecs >> 16),
		/* unlikely what we had earlier, third field is not phase */
		((long)(ts.scaled_nsecs & 0xffff) * 1000 + 0x8000) >> 16);
}



/* Enable tracking by default. Disabling the tracking is used for demos. */
static int wr_tracking_enabled = 1;
extern struct wrs_shm_head *ppsi_head;

void wr_servo_enable_tracking(int enable)
{
	wr_tracking_enabled = enable;
}


static int got_sync = 0;

void wr_servo_reset(struct pp_instance *ppi)
{
	/* values from servo_state to be preserved */
	uint32_t n_err_state;
	uint32_t n_err_offset;
	uint32_t n_err_delta_rtt;

	struct wr_servo_state *s;

	s = &((struct wr_data *)ppi->ext_data)->servo_state;
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
	memset(s, 0, sizeof(struct wr_servo_state));
	/* restore values from servo_state */
	s->n_err_state = n_err_state;
	s->n_err_offset = n_err_offset;
	s->n_err_delta_rtt = n_err_delta_rtt;

	/* shmem unlock */
	wrs_shm_write(ppsi_head, WRS_SHM_WRITE_END);
}

static inline int32_t delta_to_ps(struct FixedDelta d)
{
	_UInteger64 *sps = &d.scaledPicoseconds; /* ieee type :( */

	return (sps->lsb >> 16) | (sps->msb << 16);
}

int wr_servo_init(struct pp_instance *ppi)
{
	struct wr_dsport *wrp = WR_DSPOR(ppi);
	struct wr_servo_state *s =
			&((struct wr_data *)ppi->ext_data)->servo_state;
	/* shmem lock */
	wrs_shm_write(ppsi_head, WRS_SHM_WRITE_BEGIN);

	/* Determine the alpha coefficient */
	if (WRH_OPER()->read_calib_data(ppi, 0, 0,
		&s->fiber_fix_alpha, &s->clock_period_ps,NULL) != WRH_HW_CALIB_OK) {
		wrs_shm_write(ppsi_head, WRS_SHM_WRITE_END);

		return -1;
	}

	/* Update scaledDelayCoefficient. Need conversion because they use two different fraction bits */
	ppi->asymmetryCorrectionPortDS.scaledDelayCoefficient= (int64_t)s->fiber_fix_alpha << (REL_DIFF_FRACBITS-FIX_ALPHA_FRACBITS);

	/*
	 * Do not reset cur_setpoint, but trim it to be less than one tick.
	 * The softpll code uses the module anyways, but if we unplug-replug
	 * the fiber it will always increase, so don't scare the user
	 */
	if (s->cur_setpoint > s->clock_period_ps)
		s->cur_setpoint %= s->clock_period_ps;
	WRH_OPER()->adjust_phase(s->cur_setpoint);
	s->missed_iters = 0;
	SRV(ppi)->state = WR_UNINITIALIZED;

	s->delta_txm_ps = delta_to_ps(wrp->otherNodeDeltaTx);
	s->delta_rxm_ps = delta_to_ps(wrp->otherNodeDeltaRx);
	s->delta_txs_ps = delta_to_ps(wrp->deltaTx);
	s->delta_rxs_ps = delta_to_ps(wrp->deltaRx);

	strcpy(SRV(ppi)->servo_state_name, "Uninitialized");

	SRV(ppi)->flags |= PP_SERVO_FLAG_VALID;
	SRV(ppi)->update_count = 0;
	ppi->t_ops->get(ppi, &s->update_time);
	s->tracking_enabled = wr_tracking_enabled;

	got_sync = 0;

	/* shmem unlock */
	wrs_shm_write(ppsi_head, WRS_SHM_WRITE_END);
	return 0;
}

int wr_servo_got_sync(struct pp_instance *ppi)
{
	struct wr_servo_state *s =
			&((struct wr_data *)ppi->ext_data)->servo_state;

	s->t1 = ppi->t1; apply_faulty_stamp(ppi, 1);
	s->t2 = ppi->t2; apply_faulty_stamp(ppi, 2);
	got_sync = 1;
	return 0;
}

int wr_servo_got_delay(struct pp_instance *ppi)
{
	struct wr_servo_state *s =
			&((struct wr_data *)ppi->ext_data)->servo_state;

	wrs_shm_write(ppsi_head, WRS_SHM_WRITE_BEGIN);

	s->t3 = ppi->t3; apply_faulty_stamp(ppi, 3);
	s->t4 = ppi->t4; apply_faulty_stamp(ppi, 4);

	if (CONFIG_HAS_P2P && ppi->delayMechanism == P2P) {
		s->t5 = ppi->t5; apply_faulty_stamp(ppi, 5);
		s->t6 = ppi->t6; apply_faulty_stamp(ppi, 6);

		wr_p2p_delay(ppi, s);
	}

	wrs_shm_write(ppsi_head, WRS_SHM_WRITE_END);
	return 0;
}

/* update currentDS.meanDelay */
static void calculate_update_meanDelay(struct pp_instance *ppi,struct wr_servo_state *s) {
	struct pp_time mtime;

	mtime=s->delayMM;
	pp_time_div2(&mtime);

	update_meanDelay(ppi,pp_time_to_interval(&mtime));
}

/* update currentDS.delayAsymmetry */
static void update_delayAsymmetry(struct pp_instance *ppi,struct wr_servo_state *s) {
	DSPOR(ppi)->delayAsymmetry=picos_to_interval(s->delayMM_ps - 2LL * s->delayMS_ps);
}

static void update_offsetFromMaster (struct pp_instance *ppi,struct pp_time *offsetFrom_master_time) {
	DSCUR(ppi)->offsetFromMaster = pp_time_to_interval(offsetFrom_master_time);
}

static int wr_p2p_delay(struct pp_instance *ppi, struct wr_servo_state *s)
{
	uint64_t big_delta_fix_ps;
	static int errcount;

	if ( is_timestamps_incorrect(ppi,&errcount, 0x3C /* mask=t3&t4&t5&t6*/))
		return 0;

	SRV(ppi)->update_count++;
	ppi->t_ops->get(ppi, &s->update_time);

	{ /* avoid modifying stamps in place */
		struct pp_time mtime, stime;

		stime = s->t6; pp_time_sub(&stime, &s->t3);
		mtime = s->t5; pp_time_sub(&mtime, &s->t4);
		s->delayMM = stime; pp_time_sub(&s->delayMM, &mtime);
    }

	if (__PP_DIAG_ALLOW(ppi, pp_dt_servo, 1)) {
		dump_timestamp(ppi, "servo:t3", s->t3);
		dump_timestamp(ppi, "servo:t4", s->t4);
		dump_timestamp(ppi, "servo:t5", s->t5);
		dump_timestamp(ppi, "servo:t6", s->t6);
		dump_timestamp(ppi, "->delayMM", s->delayMM);
	}

	s->delayMM_ps = pp_time_to_picos(&s->delayMM);
	big_delta_fix_ps = s->delta_txm_ps + s->delta_txs_ps
	    + s->delta_rxm_ps + s->delta_rxs_ps;

	s->delayMS_ps=
	    (((int64_t) (s->delayMM_ps - big_delta_fix_ps) *
	      (int64_t) s->fiber_fix_alpha) >> FIX_ALPHA_FRACBITS)
	    + ((s->delayMM_ps - big_delta_fix_ps) >> 1)
	    + s->delta_txm_ps + s->delta_rxs_ps;
	picos_to_pp_time(s->delayMS_ps,&SRV(ppi)->delayMS);

	calculate_update_meanDelay(ppi,s); /* calculate and update currentDS.meanDelay & portDS.meanLinkDelay*/
	update_delayAsymmetry(ppi,s); /* update currentDS.delayAsymmetry */

	return 1;
}

static int wr_p2p_offset(struct pp_instance *ppi,
		  struct wr_servo_state *s, struct pp_time *offset)
{
	static int errcount;

	if ( is_timestamps_incorrect(ppi,&errcount, 0x3 /* mask=t1&t2 */))
		return 0;

	got_sync = 0;

	SRV(ppi)->update_count++;
	ppi->t_ops->get(ppi, &s->update_time);

	*offset = s->t1;
	pp_time_sub(offset, &s->t2);
	pp_time_add(offset, &SRV(ppi)->delayMS);

	/* is it possible to calculate it in client,
	 * but then t1 and t2 require shmem locks */

	s->tracking_enabled =  wr_tracking_enabled;

	update_offsetFromMaster(ppi,offset); /* Update currentDS.offsetFromMaster */

	return 1;
}

static int wr_e2e_offset(struct pp_instance *ppi,
		  struct wr_servo_state *s, struct pp_time *offset)
{
	uint64_t big_delta_fix;
	uint64_t delay_ms_fix;
	static int errcount;

	if ( is_timestamps_incorrect(ppi,&errcount, 0xF /* mask=t1&t2&t3&t4 */))
		return 0;

	SRV(ppi)->update_count++;
	ppi->t_ops->get(ppi, &s->update_time);

	got_sync = 0;

	/* WR Spec: Equation 28 */
	{ /* avoid modifying stamps in place */
		struct pp_time mtime, stime;

		mtime = s->t4; pp_time_sub(&mtime, &s->t1);
		stime = s->t3; pp_time_sub(&stime, &s->t2);
		s->delayMM = mtime; pp_time_sub(&s->delayMM, &stime);
	}

	if (__PP_DIAG_ALLOW(ppi, pp_dt_servo, 1)) {
		dump_timestamp(ppi, "servo:t1", s->t1);
		dump_timestamp(ppi, "servo:t2", s->t2);
		dump_timestamp(ppi, "servo:t3", s->t3);
		dump_timestamp(ppi, "servo:t4", s->t4);
		dump_timestamp(ppi, "->delayMM", s->delayMM);
	}

	s->delayMM_ps = pp_time_to_picos(&s->delayMM);

	/* WR Spec: Equation 8 */
	big_delta_fix =  s->delta_txm_ps + s->delta_txs_ps
		       + s->delta_rxm_ps + s->delta_rxs_ps;

	if (s->delayMM_ps < (int64_t)big_delta_fix) {
		/* avoid negatives in calculations */
		s->delayMM_ps = big_delta_fix;
	}

	/* WR Spec: Equation 30 */
	delay_ms_fix = (((int64_t)(s->delayMM_ps - big_delta_fix)
			* (int64_t)s->fiber_fix_alpha) >> FIX_ALPHA_FRACBITS)
			+ ((s->delayMM_ps - big_delta_fix) >> 1)
			+ s->delta_txm_ps + s->delta_rxs_ps;

	{ /* again, use temps to avoid modifying tx in place */
		struct pp_time tmp = s->t1, tmp2;

		pp_time_sub(&tmp, &s->t2);
		picos_to_pp_time(delay_ms_fix, &tmp2);
		pp_time_add(&tmp, &tmp2);

		*offset = tmp;
	}

	/* is it possible to calculate it in client,
	 * but then t1 and t2 require shmem locks */
	s->tracking_enabled =  wr_tracking_enabled;

	s->delayMS_ps = delay_ms_fix;
	picos_to_pp_time(s->delayMS_ps,&SRV(ppi)->delayMS);

	update_offsetFromMaster(ppi,offset); /* Update currentDS.offsetFromMaster */
	calculate_update_meanDelay(ppi,s); /* calculate and update currentDS.meanDelay & portDS.meanLinkDelay*/
	update_delayAsymmetry(ppi,s); /* update currentDS.delayAsymmetry */

	return 1;
}

int wr_servo_update(struct pp_instance *ppi)
{
	struct wr_servo_state *s = &((struct wr_data *)ppi->ext_data)->servo_state;
	int remaining_offset_ps;
	int64_t prev_delayMM_ps;
	int64_t offsetFromMaster_ps;
	struct pp_time offset;
	int32_t  offset_ticks;
	int32_t  offset_ps;
	int locking_poll_ret;

	if (!got_sync)
		return 0;

	/* shmem lock */
	wrs_shm_write(ppsi_head, WRS_SHM_WRITE_BEGIN);

	prev_delayMM_ps = s->delayMM_ps;
	if (CONFIG_HAS_P2P && ppi->delayMechanism == P2P) {
		if (!wr_p2p_offset(ppi, s, &offset))
			goto out;
	} else {
		if (!wr_e2e_offset(ppi, s, &offset))
			goto out;
	}

	pp_time_hardwarize(&offset, s->clock_period_ps,
		      &offset_ticks, &offset_ps);
	pp_diag(ppi, servo, 2, "offset_hw= %li.%09li (+%li), clock_period= %dps\n",
		(long)offset.secs, (long)offset_ticks,
		(long)offset_ps,s->clock_period_ps);

	locking_poll_ret = WRH_OPER()->locking_poll(ppi);
	if (locking_poll_ret != WRH_SPLL_READY
	    && locking_poll_ret != WRH_SPLL_CALIB_NOT_READY) {
		pp_diag(ppi, servo, 1, "PLL OutOfLock, should restart sync\n");
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
	if (offset.secs) /* so bad... */
		SRV(ppi)->state = WR_SYNC_TAI;
	else if (offset_ticks) /* not that bad */
		SRV(ppi)->state = WR_SYNC_NSEC;
	else if (SRV(ppi)->state == WR_UNINITIALIZED) {
		/* Likely a restart; already good, but force phase fsm */
		SRV(ppi)->state = WR_SYNC_PHASE;
	}

	/* else, let the states below choose the sequence */

	pp_diag(ppi, servo, 1, "wr_servo state: %s%s\n",
		servo_name[SRV(ppi)->state],
		SRV(ppi)->flags & PP_SERVO_FLAG_WAIT_HW ? " (wait for hw)" : "");

	/* update string state name */
	strcpy(SRV(ppi)->servo_state_name, servo_name[SRV(ppi)->state]);

	switch (SRV(ppi)->state) {
	case WR_SYNC_TAI:
		WRH_OPER()->adjust_counters(offset.secs, 0);
		SRV(ppi)->flags |= PP_SERVO_FLAG_WAIT_HW;
		/*
		 * If nsec wrong, code above forces SYNC_NSEC,
		 * Else, we must ensure we leave this status towards
		 * fine tuning
		 */
		SRV(ppi)->state = WR_SYNC_PHASE;
		break;

	case WR_SYNC_NSEC:
		WRH_OPER()->adjust_counters(0, offset_ticks);
		SRV(ppi)->flags |= PP_SERVO_FLAG_WAIT_HW;
		SRV(ppi)->state = WR_SYNC_PHASE;
		break;

	case WR_SYNC_PHASE:
		pp_diag(ppi, servo, 2, "oldsetp %i, offset %i:%04i\n",
			s->cur_setpoint, offset_ticks,
			offset_ps);
		s->cur_setpoint += offset_ps;
		WRH_OPER()->adjust_phase(s->cur_setpoint);

		SRV(ppi)->flags |= PP_SERVO_FLAG_WAIT_HW;
		SRV(ppi)->state = WR_WAIT_OFFSET_STABLE;

		if (CONFIG_ARCH_IS_WRS) {
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

	case WR_WAIT_OFFSET_STABLE:

		remaining_offset_ps = abs(offset_ps);
		if(remaining_offset_ps < WRH_SERVO_OFFSET_STABILITY_THRESHOLD) {
			WRH_OPER()->enable_timing_output(GLBS(ppi),1);
			s->prev_delayMS_ps = s->delayMS_ps;
			SRV(ppi)->state = WR_TRACK_PHASE;
		} else {
			s->missed_iters++;
		}
		if (s->missed_iters >= 10) {
			s->missed_iters = 0;
			SRV(ppi)->state = WR_SYNC_PHASE;
		}
		break;

	case WR_TRACK_PHASE:
		s->skew = s->delayMS_ps - s->prev_delayMS_ps;

		/* Can be disabled for manually tweaking and testing */
		if(wr_tracking_enabled) {
			if (abs(offset_ps) >
			    2 * WRH_SERVO_OFFSET_STABILITY_THRESHOLD) {
				SRV(ppi)->state = WR_SYNC_PHASE;
				break;
			}

			// adjust phase towards offset = 0 make ck0 0
			s->cur_setpoint += (offset_ps / 4);

			WRH_OPER()->adjust_phase(s->cur_setpoint);
			pp_diag(ppi, time, 1, "adjust phase %i\n",
				s->cur_setpoint);

			s->prev_delayMS_ps = s->delayMS_ps;
		}
		break;

	}

	SRV(ppi)->servo_locked=SRV(ppi)->state==WR_TRACK_PHASE;

	/* Increase number of servo updates with state different than
	 * WR_TRACK_PHASE. (Used by SNMP) */
	if (SRV(ppi)->state != WR_TRACK_PHASE)
		s->n_err_state++;

	/* Increase number of servo updates with offset exceeded
	 * SNMP_MAX_OFFSET_PS (Used by SNMP) */
	offsetFromMaster_ps=pp_time_to_picos(&SRV(ppi)->offsetFromMaster);
	if (abs(offsetFromMaster_ps) > SNMP_MAX_OFFSET_PS)
		s->n_err_offset++;

	/* Increase number of servo updates with delta rtt exceeded
	 * SNMP_MAX_DELTA_RTT_PS (Used by SNMP) */
	if (abs(prev_delayMM_ps - s->delayMM_ps) > SNMP_MAX_DELTA_RTT_PS)
		s->n_err_delta_rtt++;

out:
	/* shmem unlock */
	wrs_shm_write(ppsi_head, WRS_SHM_WRITE_END);

	return 0;
}
