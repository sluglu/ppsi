#include <ppsi/ppsi.h>

/* ext-whiterabbit must offer its own hooks */

int wrTmoIdx=0; /* TimeOut Index */

static int wr_init(struct pp_instance *ppi, void *buf, int len)
{
	struct wr_dsport *wrp = WR_DSPOR(ppi);

	pp_diag(ppi, ext, 2, "hook: %s\n", __func__);

	if ( wrTmoIdx==0)
		wrTmoIdx=pp_timeout_get_timer(ppi,"WR_EXT_0",TO_RAND_NONE, TMO_CF_INSTANCE_DEPENDENT);

	wrp->wrStateTimeout = WR_DEFAULT_STATE_TIMEOUT_MS;
	wrp->calPeriod = WR_DEFAULT_CAL_PERIOD;
	wrp->wrModeOn =
	wrp->parentWrModeOn = FALSE;
	wrp->parentWrConfig = NON_WR;
	wrp->calibrated = !WR_DEFAULT_PHY_CALIBRATION_REQUIRED;

#ifdef CONFIG_ABSCAL
        /* absolute calibration only exists in arch-wrpc, so far */
        extern int ptp_mode;
        if (ptp_mode == 4 /* WRC_MODE_ABSCAL */)
                ppi->next_state = WRS_WR_LINK_ON;
#endif

	return 0;
}

/* open hook called only for each WR pp_instances */
static int wr_open(struct pp_instance *ppi, struct pp_runtime_opts *rt_opts)
{
	pp_diag(NULL, ext, 2, "hook: %s\n", __func__);

	if (ppi->protocol_extension == PPSI_EXT_WR) {
		if ( is_slaveOnly(DSDEF(ppi)) ) {
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
	pp_diag(ppi, ext, 2, "hook: %s\n", __func__);

	/* This correction_field we received is already part of t4 */

	if ( ppi->ext_enabled ) {
		wr_servo_got_delay(ppi);
		wr_servo_update(ppi);
	} else {
		pp_servo_got_resp(ppi,OPTS(ppi)->ptpFallbackPpsGen);
	}
	return 0;
}

static void wr_s1(struct pp_instance *ppi, struct pp_frgn_master *frgn_master)
{

	pp_diag(ppi, ext, 2, "hook: %s\n", __func__);
	WR_DSPOR(ppi)->parentIsWRnode =
		((frgn_master->ext_specific & WR_NODE_MODE) != NON_WR);
	WR_DSPOR(ppi)->parentWrModeOn =
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
	    (!WR_DSPOR(ppi)->wrModeOn || !WR_DSPOR(ppi)->parentWrModeOn)) {
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
			pp_timeout_reset(ppi, PP_TO_ANN_RECEIPT);
	}

	/* handshake is started in slave mode */
	return 0;
}

static int  wr_sync_followup(struct pp_instance *ppi) {

	if ( ppi->ext_enabled ) {
		wr_servo_got_sync(ppi);
		if (CONFIG_HAS_P2P && ppi->delayMechanism == P2P)
			wr_servo_update(ppi);
	}
	else {
		pp_servo_got_sync(ppi,OPTS(ppi)->ptpFallbackPpsGen);
	}

	return 1; /* the caller returns too */
}

static int wr_handle_sync(struct pp_instance *ppi)
{
	/* This handle is called in case of one step clock */
	pp_diag(ppi, ext, 2, "hook: %s\n", __func__);
	return wr_sync_followup(ppi);
}

static int wr_handle_followup(struct pp_instance *ppi)
{
	/* This handle is called in case of two step clock */
	pp_diag(ppi, ext, 2, "hook: %s\n", __func__);
	return wr_sync_followup(ppi);
}

static int wr_handle_presp(struct pp_instance *ppi)
{
	/* FIXME: verify that last-received cField is already accounted for */
	if ( ppi->ext_enabled )
		wr_servo_got_delay(ppi);
	else
		pp_servo_got_presp(ppi);

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
	int msg_len = ntohs(*(UInteger16 *) (buf + 2));

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
		wrp->wrModeOn &&
		((ppi->state == PPS_SLAVE) ||
		 (ppi->state == PPS_MASTER))) {
		
		wrp->wrStateTimeout = WR_DEFAULT_STATE_TIMEOUT_MS;
		wrp->calPeriod = WR_DEFAULT_CAL_PERIOD;
		wrp->wrModeOn =
				wrp->parentWrModeOn = FALSE;
		wrp->parentWrConfig = NON_WR;
		wrp->calibrated = !WR_DEFAULT_PHY_CALIBRATION_REQUIRED;
		
		if (ppi->state == PPS_SLAVE)
			WRH_OPER()->locking_reset(ppi);
	}		 

	if ( ppi->state==PPS_SLAVE && ppi->next_state!=PPS_UNCALIBRATED ) {
		/* Leave SLAVE state : We must stop the PPS generation */
		WRH_OPER()->enable_timing_output(GLBS(ppi),0);
	}
}

static int wr_require_precise_timestamp(struct pp_instance *ppi) {
	return WR_DSPOR(ppi)->wrModeOn;
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
	.handle_sync = wr_handle_sync,
	.handle_followup = wr_handle_followup,
#if CONFIG_HAS_P2P
	.handle_presp = wr_handle_presp,
#endif
	.pack_announce = wr_pack_announce,
	.unpack_announce = wr_unpack_announce,
	.state_decision = wr_state_decision,
	.state_change = wr_state_change,
	.servo_reset= wr_servo_reset,
	.require_precise_timestamp=wr_require_precise_timestamp,
};
