#include <ppsi/ppsi.h>

/* ext-whiterabbit must offer its own hooks */

static int wr_init(struct pp_instance *ppi, void *buf, int len)
{
	struct wr_dsport *wrp = WR_DSPOR(ppi);

	pp_diag(ppi, ext, 2, "hook: %s\n", __func__);

	wrp->wrStateTimeout = WR_DEFAULT_STATE_TIMEOUT_MS;
	wrp->calPeriod = WR_DEFAULT_CAL_PERIOD;
	wrp->head.extModeOn = 0;
	wrp->parentWrConfig = NON_WR;
	wrp->parentExtModeOn = 0;
	wrp->calibrated = !WR_DEFAULT_PHY_CALIBRATION_REQUIRED;

#ifdef CONFIG_ABSCAL
        /* absolute calibration only exists in arch-wrpc, so far */
        extern int ptp_mode;
        if (ptp_mode == 4 /* WRC_MODE_ABSCAL */)
                ppi->next_state = WRS_WR_LINK_ON;
#endif

	if ((wrp->wrConfig & WR_M_AND_S) == WR_M_ONLY
#ifdef CONFIG_ABSCAL
	    && ptp_mode != 4 /* WRC_MODE_ABSCAL -- not defined in wrs build */
#endif
	   )
	   WRH_OPER()->enable_timing_output(ppi, 1);	
	else
		WRH_OPER()->enable_timing_output(ppi, 0);
	return 0;
}

/* open hook called only for each WR pp_instances */
static int wr_open(struct pp_instance *ppi, struct pp_runtime_opts *rt_opts)
{
	pp_diag(NULL, ext, 2, "hook: %s\n", __func__);

	if (ppi->protocol_extension == PPSI_EXT_WR) {
		if ( DSDEF(ppi)->slaveOnly ) {
			WR_DSPOR(ppi)->wrConfig = WR_S_ONLY;
		} else {
			if ( ppi->portDS->masterOnly ) {
				WR_DSPOR(ppi)->wrConfig = WR_M_ONLY;
			} else {
				WR_DSPOR(ppi)->wrConfig = WR_M_AND_S;
			}

		}
		return 0;
	}
	else {
		WR_DSPOR(ppi)->wrConfig = NON_WR;
		pp_diag(ppi, ext, 1, "hook: %s called but is not a WR extension \n", __func__);
		return 1;
	}
}

static int wr_listening(struct pp_instance *ppi, void *buf, int len)
{
	struct wr_dsport *wrp = WR_DSPOR(ppi);

	pp_diag(ppi, ext, 2, "hook: %s\n", __func__);
	wrp->wrMode = NON_WR;
	return 0;
}

static int wr_master_msg(struct pp_instance *ppi, void *buf, int len,
			 int msgtype)
{
	MsgSignaling wrsig_msg;

	if (msgtype != PPM_NO_MESSAGE)
		pp_diag(ppi, ext, 2, "hook: %s\n", __func__);

	if ( msgtype== PPM_SIGNALING ) {
  	    /* This is missing in the standard protocol */
		msg_unpack_wrsig(ppi, buf, &wrsig_msg,
				 &(WR_DSPOR(ppi)->msgTmpWrMessageID));
		if ((WR_DSPOR(ppi)->msgTmpWrMessageID == SLAVE_PRESENT) &&
		    (WR_DSPOR(ppi)->wrConfig & WR_M_ONLY)) {
			/* We must start the handshake as a WR master */
			wr_handshake_init(ppi, PPS_MASTER);
		}
		msgtype = PPM_NO_MESSAGE;
	}

	return msgtype;
}

static int wr_new_slave(struct pp_instance *ppi, void *buf, int len)
{
	pp_diag(ppi, ext, 2, "hook: %s\n", __func__);
	wr_servo_init(ppi);
	return 0;
}

static int wr_handle_resp(struct pp_instance *ppi)
{
	struct pp_time *ofm = &SRV(ppi)->offsetFromMaster;
	struct wr_dsport *wrp = WR_DSPOR(ppi);

	pp_diag(ppi, ext, 2, "hook: %s\n", __func__);

	/* This correction_field we received is already part of t4 */

	/*
	 * If no WR mode is on, run normal code, if T2/T3 are valid.
	 * After we adjusted the pps counter, stamps are invalid, so
	 * we'll have the Unix time instead, marked by "correct"
	 */
	if (!wrp->head.extModeOn) {
		if ( is_timestamps_incorrect(ppi, NULL, 0x6 /* mask=t2&t3 */) ) {
			pp_diag(ppi, servo, 1,
				"T2 or T3 incorrect, discarding tuple\n");
			return 0;
		}
		pp_servo_got_resp(ppi);
		/*
		 * pps always on if offset less than 1 second,
		 * until ve have a configurable threshold */
		WRH_OPER()->enable_timing_output(ppi, ofm->secs==0);

	}
	wr_servo_got_delay(ppi);
	wr_servo_update(ppi);
	return 0;
}

static void wr_s1(struct pp_instance *ppi, struct pp_frgn_master *frgn_master)
{

	pp_diag(ppi, ext, 2, "hook: %s\n", __func__);
	WR_DSPOR(ppi)->parentIsWRnode =
		((frgn_master->ext_specific & WR_NODE_MODE) != NON_WR);
	WR_DSPOR(ppi)->parentExtModeOn =
		(frgn_master->ext_specific & WR_IS_WR_MODE) ? TRUE : FALSE;
	WR_DSPOR(ppi)->parentCalibrated =
			((frgn_master->ext_specific & WR_IS_CALIBRATED) ? 1 : 0);
	WR_DSPOR(ppi)->parentWrConfig = frgn_master->ext_specific & WR_NODE_MODE;
	DSCUR(ppi)->primarySlavePortNumber =
		DSPOR(ppi)->portIdentity.portNumber;
}

int wr_execute_slave(struct pp_instance *ppi)
{
	pp_diag(ppi, ext, 2, "hook: %s\n", __func__);

	if (pp_timeout(ppi, PP_TO_FAULT))
		wr_servo_reset(ppi); /* the caller handles ptp state machine */

	if ((ppi->state == PPS_SLAVE) &&
		(WR_DSPOR(ppi)->wrConfig & WR_S_ONLY) &&
		(WR_DSPOR(ppi)->parentWrConfig & WR_M_ONLY) &&
	    (!WR_DSPOR(ppi)->head.extModeOn || !WR_DSPOR(ppi)->parentExtModeOn)) {
		/* We must start the handshake as a WR slave */
		wr_handshake_init(ppi, PPS_SLAVE);
	}
	
	/* The doRestart thing is not  used, it seems */
	if (!WR_DSPOR(ppi)->doRestart)
		return 0;
	ppi->next_state = PPS_INITIALIZING;
	WR_DSPOR(ppi)->doRestart = FALSE;
	return 1; /* the caller returns too */
}

static int wr_handle_announce(struct pp_instance *ppi)
{
	pp_diag(ppi, ext, 2, "hook: %s\n", __func__);

	switch (ppi->state) {
		case WRS_PRESENT:
		case WRS_S_LOCK :
		case WRS_LOCKED :
		case WRS_CALIBRATION :
		case WRS_CALIBRATED :
		case WRS_RESP_CALIB_REQ :
		case WRS_WR_LINK_ON :
			/* reset announce timeout when in the WR slave states */
			pp_timeout_set(ppi, PP_TO_ANN_RECEIPT);
	}

	/* handshake is started in slave mode */
	return 0;
}

static int wr_handle_followup(struct pp_instance *ppi,
			      struct pp_time *t1) /* t1 == &ppi->t1 */
{
	pp_diag(ppi, ext, 2, "hook: %s\n", __func__);
	if (!WR_DSPOR(ppi)->head.extModeOn)
		return 0;

	wr_servo_got_sync(ppi, t1, &ppi->t2);

	if (CONFIG_HAS_P2P && ppi->delayMechanism == P2P)
		wr_servo_update(ppi);

	return 1; /* the caller returns too */
}

static __attribute__((used)) int wr_handle_presp(struct pp_instance *ppi)
{
	struct wr_dsport *wrp = WR_DSPOR(ppi);
	struct pp_time *ofm = &SRV(ppi)->offsetFromMaster;

	/*
	 * If no WR mode is on, run normal code, if T2/T3 are valid.
	 * After we adjusted the pps counter, stamps are invalid, so
	 * we'll have the Unix time instead, marked by "correct"
	 */

	if (!wrp->head.extModeOn) {
		if ( is_timestamps_incorrect(ppi, NULL, 0x24 /* mask=t3&t6 */) ) {
			pp_diag(ppi, servo, 1,
				"T3 or T6 incorrect, discarding tuple\n");
			return 0;
		}
		pp_servo_got_presp(ppi);
		/*
		 * pps always on if offset less than 1 second,
		 * until ve have a configurable threshold */
		WRH_OPER()->enable_timing_output(ppi, ofm->secs==0);

		return 0;
	}

	/* FIXME: verify that last-received cField is already accounted for */
	wr_servo_got_delay(ppi);
	return 0;
}

static int wr_pack_announce(struct pp_instance *ppi)
{
	pp_diag(ppi, ext, 2, "hook: %s\n", __func__);
	if (WR_DSPOR(ppi)->wrConfig != NON_WR &&
		WR_DSPOR(ppi)->wrConfig != WR_S_ONLY) {
		msg_pack_announce_wr_tlv(ppi);
		return WR_ANNOUNCE_LENGTH;
	}
	return PP_ANNOUNCE_LENGTH;
}

static void wr_unpack_announce(void *buf, MsgAnnounce *ann)
{
	int msg_len = htons(*(UInteger16 *) (buf + 2));

	pp_diag(NULL, ext, 2, "hook: %s\n", __func__);
	if (msg_len >= WR_ANNOUNCE_LENGTH)
		msg_unpack_announce_wr_tlv(buf, ann);
	else
		ann->ext_specific = 0;
}

/* State decision algorithm 9.3.3 Fig 26 with extension for wr */
static int wr_state_decision(struct pp_instance *ppi, int next_state)
{
	pp_diag(ppi, ext, 2, "hook: %s\n", __func__);
	
	/* 
	 * if in one of the WR states stay in them, 
	 * they will eventually go back to the normal states 
	 */
	switch (ppi->state ) {
		case WRS_PRESENT :
		case WRS_M_LOCK :
		case WRS_S_LOCK :
		case WRS_LOCKED :
		case WRS_CALIBRATION :
		case WRS_CALIBRATED :
		case WRS_RESP_CALIB_REQ :
		case WRS_WR_LINK_ON :
			return ppi->state;
	}
	
	/* else do the normal statemachine */
	return next_state;
}

static void wr_state_change(struct pp_instance *ppi)
{
	struct wr_dsport *wrp = WR_DSPOR(ppi);
	
	pp_diag(ppi, ext, 2, "hook: %s\n", __func__);
	
	switch (ppi->next_state) {
		case WRS_PRESENT :
		case WRS_M_LOCK :
		case WRS_S_LOCK :
		case WRS_LOCKED :
		case WRS_CALIBRATION :
		case WRS_CALIBRATED :
		case WRS_RESP_CALIB_REQ :
		case WRS_WR_LINK_ON :
			return;
	}
	
	/* if we are leaving the WR locked states reset the WR process */
	if ((ppi->next_state != ppi->state) &&	
		(wrp->head.extModeOn == TRUE) &&
		((ppi->state == PPS_SLAVE) ||
		 (ppi->state == PPS_MASTER))) {
		
		wrp->wrStateTimeout = WR_DEFAULT_STATE_TIMEOUT_MS;
		wrp->calPeriod = WR_DEFAULT_CAL_PERIOD;
		wrp->head.extModeOn = FALSE;
		wrp->parentWrConfig = NON_WR;
		wrp->parentExtModeOn = FALSE;
		wrp->calibrated = !WR_DEFAULT_PHY_CALIBRATION_REQUIRED;
		
		if (ppi->state == PPS_SLAVE)
			WRH_OPER()->locking_reset(ppi);
	}		 
}

struct pp_ext_hooks wr_ext_hooks = {
	.init = wr_init,
	.open = wr_open,
	.listening = wr_listening,
	.master_msg = wr_master_msg,
	.new_slave = wr_new_slave,
	.handle_resp = wr_handle_resp,
	.s1 = wr_s1,
	.execute_slave = wr_execute_slave,
	.handle_announce = wr_handle_announce,
	.handle_followup = wr_handle_followup,
#if CONFIG_HAS_P2P
	.handle_presp = wr_handle_presp,
#endif
	.pack_announce = wr_pack_announce,
	.unpack_announce = wr_unpack_announce,
	.state_decision = wr_state_decision,
	.state_change = wr_state_change,
};
