#include <stdio.h>
#include <ppsi/ppsi.h>
#include <ppsi-wrs.h>
#include "wrs_dump_shmem.h"


/* map for fields of ppsi structures */
#undef DUMP_STRUCT
#define DUMP_STRUCT struct pp_globals
struct dump_info ppg_info [] = {
	DUMP_FIELD(pointer, pp_instances),	/* FIXME: follow this */
	DUMP_FIELD(pointer, servo),		/* FIXME: follow this */
	DUMP_FIELD(pointer, rt_opts),
	DUMP_FIELD(pointer, defaultDS),
	DUMP_FIELD(pointer, currentDS),
	DUMP_FIELD(pointer, parentDS),
	DUMP_FIELD(pointer, timePropertiesDS),
	DUMP_FIELD(int, ebest_idx),
	DUMP_FIELD(int, ebest_updated),
	DUMP_FIELD(int, nlinks),
	DUMP_FIELD(int, max_links),
	//DUMP_FIELD(struct pp_globals_cfg cfg),
	DUMP_FIELD(int, rxdrop),
	DUMP_FIELD(int, txdrop),
	DUMP_FIELD(pointer, arch_data),
	DUMP_FIELD(pointer, global_ext_data),
};

#undef DUMP_STRUCT
#define DUMP_STRUCT DSDefault /* Horrible typedef */
struct dump_info dsd_info [] = {
	DUMP_FIELD(Boolean, twoStepFlag),
	DUMP_FIELD(ClockIdentity, clockIdentity),
	DUMP_FIELD(UInteger16, numberPorts),
	DUMP_FIELD(ClockQuality, clockQuality),
	DUMP_FIELD(UInteger8, priority1),
	DUMP_FIELD(UInteger8, priority2),
	DUMP_FIELD(UInteger8, domainNumber),
	DUMP_FIELD(Boolean, slaveOnly),
};

#undef DUMP_STRUCT
#define DUMP_STRUCT DSCurrent /* Horrible typedef */
struct dump_info dsc_info [] = {
	DUMP_FIELD(UInteger16, stepsRemoved),
	DUMP_FIELD(time, offsetFromMaster),
	DUMP_FIELD(time, meanPathDelay), /* oneWayDelay */
	DUMP_FIELD(UInteger16, primarySlavePortNumber),
};

#undef DUMP_STRUCT
#define DUMP_STRUCT DSParent /* Horrible typedef */
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
#define DUMP_STRUCT DSTimeProperties /* Horrible typedef */
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
#define DUMP_STRUCT struct wr_servo_state
struct dump_info servo_state_info [] = {
	DUMP_FIELD_SIZE(char, if_name, 16),
	DUMP_FIELD(unsigned_long, flags),
	DUMP_FIELD(int, state),
	DUMP_FIELD(Integer32, delta_tx_m),
	DUMP_FIELD(Integer32, delta_rx_m),
	DUMP_FIELD(Integer32, delta_tx_s),
	DUMP_FIELD(Integer32, delta_rx_s),
	DUMP_FIELD(Integer32, fiber_fix_alpha),
	DUMP_FIELD(Integer32, clock_period_ps),
	DUMP_FIELD(time, t1),
	DUMP_FIELD(time, t2),
	DUMP_FIELD(time, t3),
	DUMP_FIELD(time, t4),
	DUMP_FIELD(time, t5),
	DUMP_FIELD(time, t6),
	DUMP_FIELD(Integer32, delta_ms_prev),
	DUMP_FIELD(int, missed_iters),
	DUMP_FIELD(time, mu),		/* half of the RTT */
	DUMP_FIELD(Integer64, picos_mu),
	DUMP_FIELD(Integer32, cur_setpoint),
	DUMP_FIELD(Integer32, delta_ms),
	DUMP_FIELD(UInteger32, update_count),
	DUMP_FIELD(int, tracking_enabled),
	DUMP_FIELD_SIZE(char, servo_state_name, 32),
	DUMP_FIELD(Integer64, skew),
	DUMP_FIELD(Integer64, offset),
	DUMP_FIELD(UInteger32, n_err_state),
	DUMP_FIELD(UInteger32, n_err_offset),
	DUMP_FIELD(UInteger32, n_err_delta_rtt),
	DUMP_FIELD(time, update_time),
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
	DUMP_FIELD(unsigned_long, d_flags),
	DUMP_FIELD(unsigned_char, flags),
	DUMP_FIELD(int, role),
	DUMP_FIELD(int, proto),
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
	DUMP_FIELD(time, last_rcv_time),
	DUMP_FIELD(time, last_snt_time),
	DUMP_FIELD(UInteger16, frgn_rec_num),
	DUMP_FIELD(Integer16,  frgn_rec_best),
	//DUMP_FIELD(struct pp_frgn_master frgn_master[PP_NR_FOREIGN_RECORDS]),
	DUMP_FIELD(pointer, portDS),
	//DUMP_FIELD(unsigned long timeouts[__PP_TO_ARRAY_SIZE]),
	DUMP_FIELD(UInteger16, recv_sync_sequence_id),
	//DUMP_FIELD(UInteger16 sent_seq[__PP_NR_MESSAGES_TYPES]),
	DUMP_FIELD_SIZE(bina, received_ptp_header, sizeof(MsgHeader)),
	//DUMP_FIELD(pointer, iface_name),
	//DUMP_FIELD(pointer, port_name),
	DUMP_FIELD(int, port_idx),
	DUMP_FIELD(int, vlans_array_len),
	/* pass the size of a vlans array in the nvlans field */
	DUMP_FIELD_SIZE(array_int, vlans, offsetof(DUMP_STRUCT, nvlans)),
	DUMP_FIELD(int, nvlans),

	/* sub structure */
	DUMP_FIELD_SIZE(char, cfg.port_name, 16),
	DUMP_FIELD_SIZE(char, cfg.iface_name, 16),
	DUMP_FIELD(int, cfg.ext),

	DUMP_FIELD(unsigned_long, ptp_tx_count),
	DUMP_FIELD(unsigned_long, ptp_rx_count),
};

int dump_ppsi_mem(struct wrs_shm_head *head)
{
	struct pp_globals *ppg;
	struct pp_instance *ppi;
	DSDefault *dsd;
	DSCurrent *dsc;
	DSParent *dsp;
	DSTimeProperties *dstp;
	struct wr_servo_state *global_ext_data;
	int i;

	if (head->version != WRS_PPSI_SHMEM_VERSION) {
		fprintf(stderr, "dump ppsi: unknown version %i (known is %i)\n",
			head->version, WRS_PPSI_SHMEM_VERSION);
		return -1;
	}
	ppg = (void *)head + head->data_off;
	printf("ppsi globals:\n");
	dump_many_fields(ppg, ppg_info, ARRAY_SIZE(ppg_info));

	dsd = wrs_shm_follow(head, ppg->defaultDS);
	printf("default data set:\n");
	dump_many_fields(dsd, dsd_info, ARRAY_SIZE(dsd_info));

	dsc = wrs_shm_follow(head, ppg->currentDS);
	printf("current data set:\n");
	dump_many_fields(dsc, dsc_info, ARRAY_SIZE(dsc_info));

	dsp = wrs_shm_follow(head, ppg->parentDS);
	printf("parent data set:\n");
	dump_many_fields(dsp, dsp_info, ARRAY_SIZE(dsp_info));

	dstp = wrs_shm_follow(head, ppg->timePropertiesDS);
	printf("time properties data set:\n");
	dump_many_fields(dstp, dstp_info, ARRAY_SIZE(dstp_info));

	global_ext_data = wrs_shm_follow(head, ppg->global_ext_data);
	printf("global external data set:\n");
	dump_many_fields(global_ext_data, servo_state_info,
			 ARRAY_SIZE(servo_state_info));

	ppi = wrs_shm_follow(head, ppg->pp_instances);
	for (i = 0; i < ppg->nlinks; i++) {
		printf("ppsi instance %i:\n", i);
		dump_many_fields(ppi + i, ppi_info, ARRAY_SIZE(ppi_info));
	}

	return 0; /* this is complete */
}
