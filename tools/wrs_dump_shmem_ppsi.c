#include <stdio.h>
#include <ppsi/ppsi.h>
#include <ppsi-wrs.h>
#include "wrs_dump_shmem.h"


/* map for fields of ppsi structures */
#undef DUMP_STRUCT
#define DUMP_STRUCT struct pp_globals
struct dump_info ppg_info [] = {
	DUMP_FIELD(pointer, pp_instances),	/* FIXME: follow this */
	DUMP_FIELD(pointer, rt_opts),
	DUMP_FIELD(pointer, defaultDS),
	DUMP_FIELD(pointer, currentDS),
	DUMP_FIELD(pointer, parentDS),
	DUMP_FIELD(pointer, timePropertiesDS),
	DUMP_FIELD(int, ebest_idx),
	DUMP_FIELD(int, ebest_updated),
	DUMP_FIELD(int, nlinks),
	DUMP_FIELD(int, max_links),
	// DUMP_FIELD(struct pp_globals_cfg cfg),
	DUMP_FIELD(int, rxdrop),
	DUMP_FIELD(int, txdrop),
	DUMP_FIELD(pointer, arch_data),
	DUMP_FIELD(pointer, global_ext_data),
};

#undef DUMP_STRUCT
#define DUMP_STRUCT defaultDS_t /* Horrible typedef */
struct dump_info dsd_info [] = {
	DUMP_FIELD(Boolean, twoStepFlag),
	DUMP_FIELD(ClockIdentity, clockIdentity),
	DUMP_FIELD(UInteger16, numberPorts),
	DUMP_FIELD(ClockQuality, clockQuality),
	DUMP_FIELD(UInteger8, priority1),
	DUMP_FIELD(UInteger8, priority2),
	DUMP_FIELD(UInteger8, domainNumber),
	DUMP_FIELD(Boolean, slaveOnly),
	DUMP_FIELD(Timestamp,	currentTime),
	DUMP_FIELD(Boolean,	instanceEnable),
	DUMP_FIELD(Boolean, externalPortConfigurationEnabled),
	DUMP_FIELD(Enumeration8, maxStepsRemoved),
	DUMP_FIELD(Enumeration8, SdoId),
	DUMP_FIELD(Enumeration8, instanceType),
};

#undef DUMP_STRUCT
#define DUMP_STRUCT currentDS_t/* Horrible typedef */
struct dump_info dsc_info [] = {
	DUMP_FIELD(UInteger16, stepsRemoved),
	DUMP_FIELD(TimeInterval, offsetFromMaster),
	DUMP_FIELD(TimeInterval, meanDelay), /* oneWayDelay */
	DUMP_FIELD(UInteger16, primarySlavePortNumber),
};

#undef DUMP_STRUCT
#define DUMP_STRUCT parentDS_t /* Horrible typedef */
struct dump_info dsp_info [] = {
	DUMP_FIELD(PortIdentity, parentPortIdentity),
	DUMP_FIELD(UInteger16, observedParentOffsetScaledLogVariance),
	DUMP_FIELD(Integer32, observedParentClockPhaseChangeRate),
	DUMP_FIELD(ClockIdentity, grandmasterIdentity),
	DUMP_FIELD(ClockQuality, grandmasterClockQuality),
	DUMP_FIELD(UInteger8, grandmasterPriority1),
	DUMP_FIELD(UInteger8, grandmasterPriority2),
};

#undef DUMP_STRUCT
#define DUMP_STRUCT timePropertiesDS_t /* Horrible typedef */
struct dump_info dstp_info [] = {
	DUMP_FIELD(Integer16, currentUtcOffset),
	DUMP_FIELD(Boolean, currentUtcOffsetValid),
	DUMP_FIELD(Boolean, leap59),
	DUMP_FIELD(Boolean, leap61),
	DUMP_FIELD(Boolean, timeTraceable),
	DUMP_FIELD(Boolean, frequencyTraceable),
	DUMP_FIELD(Boolean, ptpTimescale),
	DUMP_FIELD(Enumeration8, timeSource),
};

#undef DUMP_STRUCT
#define DUMP_STRUCT struct pp_servo
struct dump_info servo_state_info [] = {
	DUMP_FIELD(int , state),
	DUMP_FIELD(time, delayMM),
	DUMP_FIELD(time, delayMS),
	DUMP_FIELD(long_long, obs_drift),
	DUMP_FIELD(Integer64, mpd_fltr.m),
	DUMP_FIELD(Integer64, mpd_fltr.y),
	DUMP_FIELD(Integer64, mpd_fltr.s_exp),
	DUMP_FIELD(time, meanDelay),
	DUMP_FIELD(time, offsetFromMaster),
	DUMP_FIELD(unsigned_long,      flags),
	DUMP_FIELD(time, update_time),
	DUMP_FIELD(UInteger32, update_count),
	DUMP_FIELD(time, t1),
	DUMP_FIELD(time, t2),
	DUMP_FIELD(time, t3),
	DUMP_FIELD(time, t4),
	DUMP_FIELD(time, t5),
	DUMP_FIELD(time, t6),
	DUMP_FIELD_SIZE(char, servo_state_name,32),
	DUMP_FIELD(int,       servo_locked),
};

#if CONFIG_EXT_L1SYNC == 1
#undef DUMP_STRUCT
#define DUMP_STRUCT struct l1e_servo_state
struct dump_info l1e_servo_state_info [] = {
	DUMP_FIELD(Integer32, clock_period_ps),
	DUMP_FIELD(Integer64, delayMM_ps),
	DUMP_FIELD(Integer32, cur_setpoint_ps),
	DUMP_FIELD(Integer64, delayMS_ps),
	DUMP_FIELD(int,       tracking_enabled),
	DUMP_FIELD(Integer64, skew_ps),
	DUMP_FIELD(Integer64, offsetMS_ps),
	DUMP_FIELD(UInteger32, n_err_state),
	DUMP_FIELD(UInteger32, n_err_offset),
	DUMP_FIELD(UInteger32, n_err_delta_rtt),
	DUMP_FIELD(Integer64, prev_delayMS_ps),
	DUMP_FIELD(int, missed_iters),
};

#undef DUMP_STRUCT
#define DUMP_STRUCT l1e_ext_portDS_t
struct dump_info l1e_ext_portDS_info [] = {
	DUMP_FIELD(Boolean,  basic.L1SyncEnabled),
	DUMP_FIELD(Boolean,  basic.txCoherentIsRequired),
	DUMP_FIELD(Boolean,  basic.rxCoherentIsRequired),
	DUMP_FIELD(Boolean,  basic.congruentIsRequired),
	DUMP_FIELD(Boolean,  basic.optParamsEnabled),
	DUMP_FIELD(Integer8, basic.logL1SyncInterval),
	DUMP_FIELD(Integer8, basic.L1SyncReceiptTimeout),
	DUMP_FIELD(Boolean,  basic.L1SyncLinkAlive),
	DUMP_FIELD(Boolean,  basic.isTxCoherent),
	DUMP_FIELD(Boolean,  basic.isRxCoherent),
	DUMP_FIELD(Boolean,  basic.isCongruent),
	DUMP_FIELD(Enumeration8,  basic.L1SyncState),
	DUMP_FIELD(Boolean,  basic.peerTxCoherentIsRequired),
	DUMP_FIELD(Boolean,  basic.peerRxCoherentIsRequired),
	DUMP_FIELD(Boolean,  basic.peerCongruentIsRequired),
	DUMP_FIELD(Boolean,  basic.peerIsTxCoherent),
	DUMP_FIELD(Boolean,  basic.peerIsRxCoherent),
	DUMP_FIELD(Boolean,  basic.peerIsCongruent),
	DUMP_FIELD(Enumeration8,  basic.next_state),
	DUMP_FIELD(Boolean,  opt_params.timestampsCorrectedTx),
	DUMP_FIELD(Boolean,  opt_params.phaseOffsetTxValid),
	DUMP_FIELD(Boolean,  opt_params.frequencyOffsetTxValid),
	DUMP_FIELD(time,     opt_params.phaseOffsetTx),
	DUMP_FIELD(Timestamp,opt_params.phaseOffsetTxTimesatmp),
	DUMP_FIELD(time,     opt_params.frequencyOffsetTx),
	DUMP_FIELD(Timestamp,opt_params.frequencyOffsetTxTimesatmp),
};


#undef DUMP_STRUCT
#define DUMP_STRUCT l1e_ext_portDS_t
l1e_ext_portDS_t l1eLocalPortDS= { /* Local structure used only to print that L1Sync is disabled */
		.basic.L1SyncEnabled=0,
		.basic.L1SyncState=L1SYNC_DISABLED
};
struct dump_info l1e_local_portDS_info [] = {
		DUMP_FIELD(Boolean,  basic.L1SyncEnabled),
		DUMP_FIELD(Enumeration8,  basic.L1SyncState),
};

#endif

#if CONFIG_EXT_WR == 1
#undef DUMP_STRUCT
#define DUMP_STRUCT struct wr_servo_state
struct dump_info wr_servo_state_info [] = {
	DUMP_FIELD(Integer32, delta_txm_ps),
	DUMP_FIELD(Integer32, delta_rxm_ps),
	DUMP_FIELD(Integer32, delta_txs_ps),
	DUMP_FIELD(Integer32, delta_rxs_ps),
	DUMP_FIELD(Integer32, fiber_fix_alpha),
	DUMP_FIELD(Integer32, clock_period_ps),
	DUMP_FIELD(time,      delayMM),
	DUMP_FIELD(Integer64, delayMM_ps),
	DUMP_FIELD(Integer32, cur_setpoint),
	DUMP_FIELD(Integer64, delayMS_ps),
	DUMP_FIELD(int,       tracking_enabled),
	DUMP_FIELD(Integer64, skew),
	DUMP_FIELD(UInteger32, n_err_state),
	DUMP_FIELD(UInteger32, n_err_offset),
	DUMP_FIELD(UInteger32, n_err_delta_rtt),
	DUMP_FIELD(time, update_time),
	DUMP_FIELD(time, t1),
	DUMP_FIELD(time, t2),
	DUMP_FIELD(time, t3),
	DUMP_FIELD(time, t4),
	DUMP_FIELD(time, t5),
	DUMP_FIELD(time, t6),
	DUMP_FIELD(Integer64, prev_delayMS_ps),
	DUMP_FIELD(int, missed_iters),
};
#endif

#undef DUMP_STRUCT
#define DUMP_STRUCT portDS_t
struct dump_info portDS_info [] = {
	DUMP_FIELD(PortIdentity, portIdentity),
	DUMP_FIELD(Integer8, logMinDelayReqInterval),
	DUMP_FIELD(Integer8, logAnnounceInterval),
	DUMP_FIELD(UInteger8, announceReceiptTimeout),
	DUMP_FIELD(Integer8, logSyncInterval),
	DUMP_FIELD(pointer, ext_dsport),
	DUMP_FIELD(Integer8, logMinPdelayReqInterval),
	DUMP_FIELD(TimeInterval, meanLinkDelay),
	DUMP_FIELD(UInteger4, versionNumber),
	DUMP_FIELD(UInteger4, minorVersionNumber),
	DUMP_FIELD(TimeInterval, delayAsymmetry),
	DUMP_FIELD(RelativeDifference, delayAsymCoeff),
	DUMP_FIELD(Boolean, portEnable),
	DUMP_FIELD(Boolean, masterOnly)
};

#undef DUMP_STRUCT
#define DUMP_STRUCT struct pp_instance
struct dump_info ppi_info [] = {
	DUMP_FIELD(int, state),
	DUMP_FIELD(int, next_state),
	DUMP_FIELD(int, next_delay),
	DUMP_FIELD(int, is_new_state),
	DUMP_FIELD(pointer, arch_data),
	DUMP_FIELD(pointer, ext_data),
	DUMP_FIELD(int, protocol_extension),
	DUMP_FIELD(pointer, ext_hooks),
	DUMP_FIELD(pointer, servo),		/* FIXME: follow this */
	DUMP_FIELD(unsigned_long, d_flags),
	DUMP_FIELD(unsigned_char, flags),
	DUMP_FIELD(int, proto),
	DUMP_FIELD(int, delayMechanism),
	DUMP_FIELD(pointer, glbs),
	DUMP_FIELD(pointer, n_ops),
	DUMP_FIELD(pointer, t_ops),
	DUMP_FIELD(pointer, __tx_buffer),
	DUMP_FIELD(pointer, __rx_buffer),
	DUMP_FIELD(pointer, tx_frame),
	DUMP_FIELD(pointer, rx_frame),
	DUMP_FIELD(pointer, tx_ptp),
	DUMP_FIELD(pointer, rx_ptp),

	/* This is a sub-structure */
	DUMP_FIELD(int, ch[0].fd),
	DUMP_FIELD(pointer, ch[0].custom),
	DUMP_FIELD(pointer, ch[0].arch_data),
	DUMP_FIELD_SIZE(bina, ch[0].addr, 6),
	DUMP_FIELD(int, ch[0].pkt_present),
	DUMP_FIELD(int, ch[1].fd),
	DUMP_FIELD(pointer, ch[1].custom),
	DUMP_FIELD(pointer, ch[1].arch_data),
	DUMP_FIELD_SIZE(bina, ch[1].addr, 6),
	DUMP_FIELD(int, ch[1].pkt_present),

	DUMP_FIELD(ip_address, mcast_addr),
	DUMP_FIELD(int, tx_offset),
	DUMP_FIELD(int, rx_offset),
	DUMP_FIELD_SIZE(bina, peer, 6),
	DUMP_FIELD(uint16_t, peer_vid),

	DUMP_FIELD(time, t1),
	DUMP_FIELD(time, t2),
	DUMP_FIELD(time, t3),
	DUMP_FIELD(time, t4),
	DUMP_FIELD(time, t5),
	DUMP_FIELD(time, t6),
	DUMP_FIELD(uint64_t, syncCF),
	DUMP_FIELD(Integer32, t4_cf),
	DUMP_FIELD(Integer32, t6_cf),
	DUMP_FIELD(time, last_rcv_time),
	DUMP_FIELD(time, last_snt_time),
	DUMP_FIELD(UInteger16, frgn_rec_num),
	DUMP_FIELD(Integer16,  frgn_rec_best),
	//DUMP_FIELD(struct pp_frgn_master frgn_master[PP_NR_FOREIGN_RECORDS]),
	DUMP_FIELD(pointer, portDS),
	DUMP_FIELD(Boolean,asymmetryCorrectionPortDS.enable),
	DUMP_FIELD(TimeInterval,asymmetryCorrectionPortDS.constantAsymmetry),
	DUMP_FIELD(RelativeDifference,asymmetryCorrectionPortDS.scaledDelayCoefficient),
	DUMP_FIELD(TimeInterval,timestampCorrectionPortDS.egressLatency),
	DUMP_FIELD(TimeInterval,timestampCorrectionPortDS.ingressLatency),
	DUMP_FIELD(TimeInterval,timestampCorrectionPortDS.messageTimestampPointLatency),
	DUMP_FIELD(TimeInterval,timestampCorrectionPortDS.semistaticLatency),
	DUMP_FIELD(Enumeration8,externalPortConfigurationPortDS.desiredState),

	//DUMP_FIELD(unsigned long timeouts[__PP_TO_ARRAY_SIZE]),
	DUMP_FIELD(UInteger16, recv_sync_sequence_id),
	//DUMP_FIELD(UInteger16 sent_seq[__PP_NR_MESSAGES_TYPES]),
	DUMP_FIELD_SIZE(bina, received_ptp_header, sizeof(MsgHeader)),
	DUMP_FIELD(Boolean, link_up),

	DUMP_FIELD_SIZE(pointer, iface_name,16),
	DUMP_FIELD_SIZE(pointer, port_name,16),
	DUMP_FIELD(int, port_idx),
	DUMP_FIELD(int, vlans_array_len),
	/* pass the size of a vlans array in the nvlans field */
	DUMP_FIELD_SIZE(array_int, vlans, offsetof(DUMP_STRUCT, nvlans)),
	DUMP_FIELD(int, nvlans),

	/* sub structure */
	DUMP_FIELD_SIZE(char, cfg.port_name, 16),
	DUMP_FIELD_SIZE(char, cfg.iface_name, 16),
	DUMP_FIELD(int, cfg.profile),

	DUMP_FIELD(unsigned_long, ptp_tx_count),
	DUMP_FIELD(unsigned_long, ptp_rx_count),
};

#undef DUMP_STRUCT
#define DUMP_STRUCT struct wr_dsport
struct dump_info wr_dsport_info [] = {
	DUMP_FIELD(UInteger16, otherNodeCalSendPattern),
	DUMP_FIELD(UInteger8, otherNodeCalRetry),
	DUMP_FIELD(UInteger32, otherNodeCalPeriod),
	DUMP_FIELD(scaledPicoseconds, otherNodeDeltaTx),
	DUMP_FIELD(scaledPicoseconds, otherNodeDeltaRx),
};


int dump_ppsi_mem(struct wrs_shm_head *head)
{
	struct pp_globals *ppg;
	struct pp_instance *pp_instances;
	defaultDS_t *dsd;
	currentDS_t *dsc;
	parentDS_t *dsp;
	timePropertiesDS_t *dstp;
	int i;
	char prefix[64];

	if (head->version != WRS_PPSI_SHMEM_VERSION) {
		fprintf(stderr, "dump ppsi: unknown version %i (known is %i)\n",
			head->version, WRS_PPSI_SHMEM_VERSION);
		return -1;
	}
	ppg = (void *)head + head->data_off;
	dump_many_fields(ppg, ppg_info, ARRAY_SIZE(ppg_info),"ppsi.globalDS");

	dsd = wrs_shm_follow(head, ppg->defaultDS);
	dump_many_fields(dsd, dsd_info, ARRAY_SIZE(dsd_info),"ppsi.defaulDS");

	dsc = wrs_shm_follow(head, ppg->currentDS);
	dump_many_fields(dsc, dsc_info, ARRAY_SIZE(dsc_info),"ppsi.currentDS");

	dsp = wrs_shm_follow(head, ppg->parentDS);
	dump_many_fields(dsp, dsp_info, ARRAY_SIZE(dsp_info),"ppsi.parentDS");

	dstp = wrs_shm_follow(head, ppg->timePropertiesDS);
	dump_many_fields(dstp, dstp_info, ARRAY_SIZE(dstp_info),"ppsi.timePropertiesDS");

	pp_instances = wrs_shm_follow(head, ppg->pp_instances);
	/* print extension servo data set */
	for (i = 0; i < ppg->nlinks; i++) {
		struct pp_instance * ppi= pp_instances+i;

		sprintf(prefix,"ppsi.inst.%d.portDS",i);
		dump_many_fields( wrs_shm_follow(head, ppi->portDS)
				, portDS_info,
				 ARRAY_SIZE(portDS_info),prefix);

		if ( ppi->state == PPS_SLAVE ) {
			sprintf(prefix,"ppsi.inst.%d.servo",i);
			dump_many_fields( wrs_shm_follow(head, ppi->servo)
					, servo_state_info,
					 ARRAY_SIZE(servo_state_info),prefix);
#if CONFIG_EXT_WR == 1
			if ( ppi->protocol_extension == PPSI_EXT_WR) {
				struct wr_data *data;
				data = wrs_shm_follow(head, ppi->ext_data);
				sprintf(prefix,"ppsi.inst.%d.servo.wr",i);
				dump_many_fields(&data->servo_state, wr_servo_state_info, ARRAY_SIZE(wr_servo_state_info),prefix);
			}
#endif
#if CONFIG_EXT_L1SYNC == 1
			if ( ppi->protocol_extension == PPSI_EXT_L1S) {
				struct l1e_data *data;
				data = wrs_shm_follow(head, ppi->ext_data);
				sprintf(prefix,"ppsi.inst.%d.servo.l1sync",i);
				dump_many_fields(&data->servo_state, l1e_servo_state_info, ARRAY_SIZE(l1e_servo_state_info),prefix);
			}
#endif
		}
	}

	for (i = 0; i < ppg->nlinks; i++) {
		struct pp_instance * ppi= pp_instances+i;

		sprintf(prefix,"ppsi.inst.%d.info",i);
		dump_many_fields(ppi, ppi_info, ARRAY_SIZE(ppi_info),prefix);
#if CONFIG_EXT_L1SYNC == 1
		sprintf(prefix,"ppsi.inst.%d.l1sync_portDS",i);
		if ( ppi->protocol_extension == PPSI_EXT_L1S) {
			portDS_t *portDS=wrs_shm_follow(head, ppi->portDS);
			l1e_ext_portDS_t *data=wrs_shm_follow(head, portDS->ext_dsport);
			dump_many_fields(data, l1e_ext_portDS_info, ARRAY_SIZE(l1e_ext_portDS_info),prefix);
		} else  {
			dump_many_fields(&l1eLocalPortDS, l1e_local_portDS_info, ARRAY_SIZE(l1e_local_portDS_info),prefix);
		}
#endif
	}
	return 0; /* this is complete */
}
