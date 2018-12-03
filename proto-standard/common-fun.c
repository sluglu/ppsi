/*
 * Copyright (C) 2011 CERN (www.cern.ch)
 * Author: Aurelio Colosimo
 * Based on PTPd project v. 2.1.0 (see AUTHORS for details)
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */
#include <ppsi/ppsi.h>
#include "common-fun.h"
#include "../lib/network_types.h"

void *msg_copy_header(MsgHeader *dest, MsgHeader *src)
{
	return memcpy(dest, src, sizeof(MsgHeader));
}

static void *__align_pointer(void *p)
{
	unsigned long ip, align = 0;

	ip = (unsigned long)p;
	if (ip & 3)
		align = 4 - (ip & 3);
	return p + align;
}

void pp_prepare_pointers(struct pp_instance *ppi)
{
	/*
	 * Horrible thing: when we receive vlan, we get standard eth header,
	 * but when we send we must fill the complete vlan header.
	 * So we reserve a different number of bytes.
	 */
	switch(ppi->proto) {
	case PPSI_PROTO_RAW:
		ppi->tx_offset = ETH_HLEN; /* 14, I know! */
		ppi->rx_offset = ETH_HLEN;
	#ifdef CONFIG_ARCH_WRPC
		ppi->tx_offset = 0; /* Currently, wrpc has a separate header */
		ppi->rx_offset = 0;
	#endif
		break;
	case PPSI_PROTO_VLAN:
		ppi->tx_offset = sizeof(struct pp_vlanhdr);
		ppi->rx_offset = ETH_HLEN;
		break;
	case PPSI_PROTO_UDP:
		ppi->tx_offset = 0;
		ppi->rx_offset = 0;
		break;
	}
	ppi->tx_ptp = __align_pointer(ppi->__tx_buffer + ppi->tx_offset);
	ppi->rx_ptp = __align_pointer(ppi->__rx_buffer + ppi->rx_offset);

	/* Now that ptp payload is aligned, get back the header */
	ppi->tx_frame = ppi->tx_ptp - ppi->tx_offset;
	ppi->rx_frame = ppi->rx_ptp - ppi->rx_offset;

	if (0) { /* enable to verify... it works for me though */
		pp_printf("%p -> %p %p\n",
			  ppi->__tx_buffer, ppi->tx_frame, ppi->tx_ptp);
		pp_printf("%p -> %p %p\n",
			  ppi->__rx_buffer, ppi->rx_frame, ppi->rx_ptp);
	}
}

static int is_grand_master(struct pp_instance *ppi) {
	int has_slave= 0;
	int has_master=0;
	int i=0;

#if CODEOPT_ONE_PORT()
	{
#else
	for (; i < DSDEF(ppi)->numberPorts; i++) {
#endif
		switch (INST(GLBS(ppi), i)->state) {
		case PPS_UNCALIBRATED:
		case PPS_SLAVE:
			has_slave=1;
			break;
		case PPS_MASTER:
		case PPS_PRE_MASTER:
		case PPS_PASSIVE:
			has_master=1;
		}
	}
	return has_master && !has_slave;
}

int st_com_check_announce_receive_timeout(struct pp_instance *ppi)
{
	if (pp_timeout(ppi, PP_TO_ANN_RECEIPT)) {
		/* 9.2.6.11 b) reset timeout when an announce timeout happened */
		pp_timeout_set(ppi, PP_TO_ANN_RECEIPT);

		if ( !DSDEF(ppi)->slaveOnly ) {
			if ( is_grand_master(ppi) ) {
				bmc_m1(ppi);
			} else {
				bmc_m3(ppi);
			}
			if ( DSDEF(ppi)->externalPortConfigurationEnabled ) {
				/* Clause 17.6.5.3 : The announce receipt timeout mechanism shall not be active */
				return 0;
			}
			ppi->next_state = PPS_MASTER;
		} else {
			ppi->next_state = PPS_LISTENING;
		}
		bmc_flush_erbest(ppi); /* ErBest is removed from the foreign master list and ErBest need to be re-computed */
	}
	return 0;
}

int st_com_handle_announce(struct pp_instance *ppi, void *buf, int len)
{
	/* Clause 9.2.2.2 MasterOnly PTP ports :
	 * Announce messages received on a masterOnly PTP Port shall not be considered
	 * in the operation of the best master clock algorithm or in the update of data sets.
	 */
	if ( ! DSPOR(ppi)->masterOnly ) {
		bmc_add_frgn_master(ppi, buf, len);
	}
	if (ppi->ext_hooks->handle_announce)
		return ppi->ext_hooks->handle_announce(ppi);
	return 0;
}

int st_com_handle_signaling(struct pp_instance *ppi, void *buf, int len)
{
	if (ppi->ext_hooks->handle_signaling)
		return ppi->ext_hooks->handle_signaling(ppi,buf,len);
	return 0;
}


int __send_and_log(struct pp_instance *ppi, int msglen, int chtype,enum pp_msg_format msg_fmt)
{
	struct pp_msgtype_info *mf = pp_msgtype_info + msg_fmt;
	struct pp_time *t = &ppi->last_snt_time;
	int ret;

	ret = ppi->n_ops->send(ppi, ppi->tx_frame, msglen + ppi->tx_offset,msg_fmt);
	if (ret == PP_SEND_DROP)
		return 0; /* don't report as error, nor count nor log as sent */
	if (ret < msglen) {
		pp_diag(ppi, frames, 1, "%s(%d) Message can't be sent\n",
				pp_msgtype_name[mf->msg_type], mf->msg_type);
		return PP_SEND_ERROR;
	}
	/* The send method updates ppi->last_snt_time with the Tx timestamp. */
	/* This timestamp must be corrected with the egressLatency */
	pp_time_add_interval(t,ppi->timestampCorrectionPortDS.egressLatency);

	/* FIXME: diagnostics should be looped back in the send method */
	pp_diag(ppi, frames, 1, "SENT %02d bytes at %d.%09d.%03d (%s)\n",
		msglen, (int)t->secs, (int)(t->scaled_nsecs >> 16),
		((int)(t->scaled_nsecs & 0xffff) * 1000) >> 16,
		pp_msgtype_name[mf->msg_type]);
	if (chtype == PP_NP_EVT && is_incorrect(&ppi->last_snt_time))
		return PP_SEND_NO_STAMP;

	/* count sent packets */
	ppi->ptp_tx_count++;

	return 0;
}

/* Update currentDS.meanDelay
 * This function can be redeclared if P2P mechanism is compiled
 */
void __attribute__((weak)) update_meanDelay(struct pp_instance *ppi, TimeInterval meanDelay) {
	DSCUR(ppi)->meanDelay=meanDelay;
}


