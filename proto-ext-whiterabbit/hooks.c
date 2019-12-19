#include <ppsi/ppsi.h>

/* ext-whiterabbit must offer its own hooks */

int wrTmoIdx=0; /* TimeOut Index */

static int wr_init(struct pp_instance *ppi, void *buf, int len)
{
	pp_diag(ppi, ext, 2, "hook: %s\n", __func__);

	if ( wrTmoIdx==0)
		wrTmoIdx=pp_timeout_get_timer(ppi,"WR_EXT_0",TO_RAND_NONE, TMO_CF_INSTANCE_DEPENDENT);

	wr_reset_process(ppi,WR_ROLE_NONE);
	ppi->pdstate = PP_PDSTATE_WAIT_MSG;

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

	if ( is_slaveOnly(DSDEF(ppi)) ||
			( is_externalPortConfigurationEnabled(DSDEF(ppi)) &&
					ppi->externalPortConfigurationPortDS.desiredState==PPS_SLAVE)
	   ){
		WR_DSPOR(ppi)->wrConfig = WR_S_ONLY;
	} else {
		WR_DSPOR(ppi)->wrConfig = ( ppi->portDS->masterOnly ||
				( is_externalPortConfigurationEnabled(DSDEF(ppi)) &&
						ppi->externalPortConfigurationPortDS.desiredState==PPS_MASTER)) ?
								WR_M_ONLY :
								WR_M_AND_S;
	}
	return 0;
}

static int wr_handle_resp(struct pp_instance *ppi)
{
	pp_diag(ppi, ext, 2, "hook: %s\n", __func__);

	/* This correction_field we received is already part of t4 */
	if ( ppi->extState==PP_EXSTATE_ACTIVE ) {
		wr_servo_got_resp(ppi);
		if ( ppi->pdstate==PP_PDSTATE_PDETECTED)
			pdstate_set_state_pdetected(ppi); // Maintain state Protocol detected on MASTER side
	}
	else {
		pp_servo_got_resp(ppi,OPTS(ppi)->ptpFallbackPpsGen);
	}
	return 0;
}

static int wr_handle_dreq(struct pp_instance *ppi)
{
	pp_diag(ppi, ext, 2, "hook: %s\n", __func__);
	if ( ppi->extState==PP_EXSTATE_ACTIVE ) {
		if ( ppi->pdstate==PP_PDSTATE_PDETECTED)
			pdstate_set_state_pdetected(ppi); // Maintain state Protocol detected on MASTER side
	}

	return 0;
}

static int  wr_sync_followup(struct pp_instance *ppi) {

	if ( ppi->extState==PP_EXSTATE_ACTIVE ) {
		wr_servo_got_sync(ppi);
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
	if ( ppi->extState==PP_EXSTATE_ACTIVE ) {
		wr_servo_got_presp(ppi);
		if ( ppi->pdstate==PP_PDSTATE_PDETECTED)
			pdstate_set_state_pdetected(ppi); // Maintain state Protocol detected on MASTER side
	}
	else
		pp_servo_got_presp(ppi);
	return 0;
}

static int wr_pack_announce(struct pp_instance *ppi)
{
	pp_diag(ppi, ext, 2, "hook: %s\n", __func__);
	/* Even though the extension is disable we continue to send WR TLV */
	if ( WR_DSPOR(ppi)->wrConfig != WR_S_ONLY) {
		msg_pack_announce_wr_tlv(ppi);
		return WR_ANNOUNCE_LENGTH;
	}
	return PP_ANNOUNCE_LENGTH;
}

static void wr_unpack_announce(struct pp_instance *ppi,void *buf, MsgAnnounce *ann)
{
	MsgHeader *hdr = &ppi->received_ptp_header;
	int msg_len = ntohs(*(UInteger16 *) (buf + 2));
	Boolean parentIsWRnode=FALSE;
	Boolean resetWrProtocol=FALSE;
	int slaveUncalState=ppi->state==PPS_UNCALIBRATED || ppi->state==PPS_SLAVE;
	struct wr_dsport *wrp = WR_DSPOR(ppi);

	pp_diag(NULL, ext, 2, "hook: %s\n", __func__);

	// If the message is not coming from erbest, it must be discarded
	if ( !bmc_is_erbest(ppi,&hdr->sourcePortIdentity))
		return;

	if (msg_len >= WR_ANNOUNCE_LENGTH) {
		UInteger16 wr_flags;
		MsgHeader *hdr = &ppi->received_ptp_header;
		struct PortIdentity *pid = &hdr->sourcePortIdentity;


		msg_unpack_announce_wr_tlv(buf, ann, &wr_flags);
		parentIsWRnode=(wr_flags & WR_NODE_MODE)!=NON_WR;

		// Check if a new parent is detected.
		// This part is needed to cover the following use case :
		// on the master side, the WR extension is disabled (WR  calibration failure or PTP profile selected)
		// then the PPSi process is restarted using the WR profile. On the slave side, if the timeout ANN_RECEIPT has not fired,
		// we must detect that the PPSi process has been restarted and then replay the calibration protocol.
		// Checked parameters :
		// - The parent is a WR node
		// - ptp state=(slave|uncalibrated|listening)
		// - Same parent port ID but with a not continuous sequence ID (With a margin of 1)
		// - The port identity is different
		if ( parentIsWRnode &&
				(slaveUncalState ||	ppi->state==PPS_LISTENING))  {

			Boolean samePid=!bmc_pidcmp(pid, &wrp->parentAnnPortIdentity);
			if ( !samePid  ||
					(samePid &&
							(hdr->sequenceId!=(UInteger16) (wrp->parentAnnSequenceId+1) &&
							hdr->sequenceId!=(UInteger16) (wrp->parentAnnSequenceId+2))
							)) {
				/* For other states, it is done in the state_change hook */
				resetWrProtocol=slaveUncalState;
				pdstate_enable_extension(ppi);
			}
		} else {
			parentIsWRnode=FALSE;
			resetWrProtocol=ppi->extState==PP_EXSTATE_ACTIVE  &&  slaveUncalState;
		}
		memcpy(&wrp->parentAnnPortIdentity,pid,sizeof(struct PortIdentity));
		wrp->parentAnnSequenceId=hdr->sequenceId;


		/* Update the WR parent state */
		if ( !parentIsWRnode  )
			/* Not a WR node */
			wr_flags=0; /* Forget all bits. They are not relevant */
		wrp->parentIsWRnode = parentIsWRnode;
		wrp->parentWrModeOn = (wr_flags & WR_IS_WR_MODE) != 0;
		wrp->parentCalibrated =(wr_flags & WR_IS_CALIBRATED) != 0;
		wrp->parentWrConfig = wr_flags & WR_NODE_MODE;
	} else {
		resetWrProtocol=ppi->extState==PP_EXSTATE_ACTIVE  &&  slaveUncalState;
	}

	if ( resetWrProtocol ) {
		ppi->next_state=PPS_UNCALIBRATED;
		wrp->next_state=WRS_PRESENT;
		wrp->wrMode=WR_SLAVE;
	}
}

static void wr_state_change(struct pp_instance *ppi)
{
	struct wr_dsport *wrp = WR_DSPOR(ppi);
	
	pp_diag(ppi, ext, 2, "hook: %s\n", __func__);
	if ( ppi->extState==PP_EXSTATE_PTP &&  ppi->next_state==PPS_UNCALIBRATED ) {
		// Extension need to be re-enabled
		pdstate_enable_extension(ppi);
	}

	if ( ppi->extState==PP_EXSTATE_ACTIVE ) {

		// Check leaving state
		if ( wrp->wrModeOn &&
				((ppi->state == PPS_SLAVE) ||
						(ppi->state == PPS_MASTER))) {
			/* if we are leaving the MASTER or SLAVE state */
			wr_reset_process(ppi,WR_ROLE_NONE);
		}

		// Check entering state
		switch (ppi->next_state) {
		case PPS_MASTER : /* Enter in MASTER_STATE */
			wrp->next_state=WRS_IDLE;
			wr_reset_process(ppi,WR_MASTER);
			break;
		case PPS_UNCALIBRATED : /* Enter in UNCALIBRATED state */
			wrp->next_state=WRS_PRESENT;
			wr_reset_process(ppi,WR_SLAVE);
			break;
		case PPS_LISTENING : /* Enter in LISTENING state */
			wr_reset_process(ppi,WR_ROLE_NONE);
			break;
		}

	} else {
		wr_reset_process(ppi,WR_ROLE_NONE);
	}

	if (ppi->state == PPS_SLAVE) {
		/* We are leaving SLAVE state, so we must reset locking
		 * This must be done on all cases (extension ACTIVE or NOT) because it may be possible we entered
		 * in SLAVE state with the extension active and now it can be inactive
		 */
		WRH_OPER()->locking_reset(ppi);
		if ( ppi->next_state!=PPS_UNCALIBRATED ) {
			/* Leave SLAVE/UNCALIB states : We must stop the PPS generation */
			if ( !GOPTS(GLBS(ppi))->forcePpsGen )
				TOPS(ppi)->enable_timing_output(GLBS(ppi),0);
		}
	}
}

int wr_ready_for_slave(struct pp_instance *ppi) {
	if ( ppi->extState==PP_EXSTATE_ACTIVE ) {
		struct wr_dsport *wrp = WR_DSPOR(ppi);
		return wrp->wrModeOn && wrp->parentWrModeOn && wrp->state == WRS_IDLE;
	} else
		return 1; /* Allow to go to slave state as we will not do a WR handshake */
}


static int wr_require_precise_timestamp(struct pp_instance *ppi) {
	return ppi->extState==PP_EXSTATE_ACTIVE && WR_DSPOR(ppi)->wrModeOn;
}

static int wr_get_tmo_lstate_detection(struct pp_instance *ppi) {
	/* The WR protocol detection comes :
	 * - for a SLAVE: from the announce message (WR TLV)
	 * - for a MASTER: from the reception of the WR_PRESENT message
	 * To cover this 2 cases we will use 2 times the announce receipt time-out
	 */
	return is_externalPortConfigurationEnabled(DSDEF(ppi)) ?
			20000 : /* 20s: externalPortConfiguration enable means no ANN_RECEIPT timeout */
			pp_timeout_get(ppi,PP_TO_ANN_RECEIPT)<<3;
}

static TimeInterval wr_get_latency (struct pp_instance *ppi) {
	return 0;
}

/* WR extension is not compliant with the standard concerning the contents of the correction fields */
static int wr_is_correction_field_compliant (struct pp_instance *ppi) {
	return 0;
}

static int wr_extension_state_changed( struct pp_instance * ppi) {
	if ( ppi->extState==PP_EXSTATE_DISABLE || ppi->extState==PP_EXSTATE_PTP) {
		wr_reset_process(ppi,WR_ROLE_NONE);
	}
	return 0;
}

struct pp_ext_hooks wr_ext_hooks = {
	.init = wr_init,
	.open = wr_open,
	.handle_resp = wr_handle_resp,
	.handle_dreq = wr_handle_dreq,
	.handle_sync = wr_handle_sync,
	.handle_followup = wr_handle_followup,
	.ready_for_slave = wr_ready_for_slave,
	.run_ext_state_machine = wr_run_state_machine,
#if CONFIG_HAS_P2P
	.handle_presp = wr_handle_presp,
#endif
	.pack_announce = wr_pack_announce,
	.unpack_announce = wr_unpack_announce,
	.state_change = wr_state_change,
	.servo_reset= wr_servo_reset,
	.require_precise_timestamp=wr_require_precise_timestamp,
	.get_tmo_lstate_detection=wr_get_tmo_lstate_detection,
	.get_ingress_latency=wr_get_latency,
	.get_egress_latency=wr_get_latency,
	.is_correction_field_compliant=wr_is_correction_field_compliant,
	.extension_state_changed= wr_extension_state_changed
};
