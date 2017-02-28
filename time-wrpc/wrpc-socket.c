/*
 * Copyright (C) 2011 CERN (www.cern.ch)
 * Author: Alessandro Rubini
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */
#include <ppsi/ppsi.h>
#include "ptpdump.h"

#include <syscon.h> /* wrpc-sw */
#include <ptpd_netif.h> /* wrpc-sw */

int frame_rx_delay_us; /* set by faults.c */

/*
 * we know we create one socket only in wrpc. The buffer size used to be
 * 512. Let's keep it unchanged, because we might enqueue a few frames.
 * I think this can be decreased to 256, but I'd better play safe
 */
static uint8_t __ptp_queue[512];
static struct wrpc_socket __static_ptp_socket = {
	.queue.buff = __ptp_queue,
	.queue.size = sizeof(__ptp_queue),
};


/* This function should init the minic and get the mac address */
static int wrpc_open_ch(struct pp_instance *ppi)
{
	struct wrpc_socket *sock;
	mac_addr_t mac;
	struct wr_sockaddr addr;
	char *macaddr = PP_MCAST_MACADDRESS;

	if (CONFIG_HAS_P2P && ppi->mech == PP_P2P_MECH)
		macaddr = PP_PDELAY_MACADDRESS;
	addr.ethertype = htons(ETH_P_1588);
	memcpy(addr.mac, macaddr, sizeof(mac_addr_t));
	sock = ptpd_netif_create_socket(&__static_ptp_socket, &addr,
					PTPD_SOCK_RAW_ETHERNET, 0);
	if (!sock)
		return -1;

	ptpd_netif_get_hw_addr(sock, &mac);
	memcpy(ppi->ch[PP_NP_EVT].addr, &mac, sizeof(mac_addr_t));
	ppi->ch[PP_NP_EVT].custom = sock;
	memcpy(ppi->ch[PP_NP_GEN].addr, &mac, sizeof(mac_addr_t));
	ppi->ch[PP_NP_GEN].custom = sock;

	return 0;
}

/* To receive and send packets, we call the minic low-level stuff */
static int wrpc_net_recv(struct pp_instance *ppi, void *pkt, int len,
			 struct pp_time *t)
{
	int got;
	struct wrpc_socket *sock;
	struct wr_timestamp wr_ts;
	struct wr_sockaddr addr;
	sock = ppi->ch[PP_NP_EVT].custom;
	got = ptpd_netif_recvfrom(sock, &addr, pkt, len, &wr_ts);
	if (got <= 0)
		return got;

	if (t) {
		t->secs = wr_ts.sec;
		t->scaled_nsecs = (int64_t)wr_ts.nsec << 16;
		t->scaled_nsecs += wr_ts.phase * (1 << 16) / 1000;
		if (!wr_ts.correct)
			mark_incorrect(t);
	}

/* wrpc-sw may pass this in USER_CFLAGS, to remove footprint */
#ifndef CONFIG_NO_PTPDUMP
	/* The header is separate, so dump payload only */
	if (pp_diag_allow(ppi, frames, 2))
		dump_payloadpkt("recv: ", pkt, got, t);
#endif

	if (CONFIG_HAS_WRPC_FAULTS && ppsi_drop_rx()) {
		pp_diag(ppi, frames, 1, "Drop received frame\n");
		return -2;
	}

	if (CONFIG_HAS_WRPC_FAULTS)
		usleep(frame_rx_delay_us);
	return got;
}

static int wrpc_net_send(struct pp_instance *ppi, void *pkt, int len,
			 int msgtype)
{
	int snt, drop;
	struct wrpc_socket *sock;
	struct wr_timestamp wr_ts;
	struct wr_sockaddr addr;
	struct pp_time *t = &ppi->last_snt_time;
	int is_pdelay = pp_msgtype_info[msgtype].is_pdelay;
	static const uint8_t macaddr[2][ETH_ALEN] = {
		[PP_E2E_MECH] = PP_MCAST_MACADDRESS,
		[PP_P2P_MECH] = PP_PDELAY_MACADDRESS,
	};

	/*
	 * To fake a packet loss, we must corrupt the frame; we need
	 * to transmit it for real, if we want to get back our
	 * hardware stamp. Thus, remember if we drop, and use this info.
	 */
	if (CONFIG_HAS_WRPC_FAULTS)
		drop = ppsi_drop_tx();

	sock = ppi->ch[PP_NP_EVT].custom;

	addr.ethertype = htons(ETH_P_1588);
	memcpy(&addr.mac, macaddr[is_pdelay], sizeof(mac_addr_t));
	if (CONFIG_HAS_WRPC_FAULTS && drop) {
		addr.ethertype = 1;
		addr.mac[0] = 0x22; /* pfilter uses mac; drop for nodes too */
	}

	snt = ptpd_netif_sendto(sock, &addr, pkt, len, &wr_ts);

	if (t) {
		t->secs = wr_ts.sec;
		t->scaled_nsecs = (int64_t)wr_ts.nsec << 16;
		if (!wr_ts.correct)
			mark_incorrect(t);

		pp_diag(ppi, frames, 2, "%s: snt=%d, sec=%ld, nsec=%ld\n",
			__func__, snt, (long)t->secs,
			(long)(t->scaled_nsecs >> 16));
	}
	if (CONFIG_HAS_WRPC_FAULTS && drop) {
		pp_diag(ppi, frames, 1, "Drop sent frame\n");
		return -2;
	}

/* wrpc-sw may pass this in USER_CFLAGS, to remove footprint */
#ifndef CONFIG_NO_PTPDUMP
	/* The header is separate, so dump payload only */
	if (snt >0 && pp_diag_allow(ppi, frames, 2))
		dump_payloadpkt("send: ", pkt, len, t);
#endif

	return snt;
}

static int wrpc_net_exit(struct pp_instance *ppi)
{
	ptpd_netif_close_socket(ppi->ch[PP_NP_EVT].custom);
	return 0;
}

/* This function must be able to be called twice, and clean-up internally */
static int wrpc_net_init(struct pp_instance *ppi)
{
	if (ppi->ch[PP_NP_EVT].custom)
		wrpc_net_exit(ppi);
	pp_prepare_pointers(ppi);
	wrpc_open_ch(ppi);

	return 0;

}

struct pp_network_operations wrpc_net_ops = {
	.init = wrpc_net_init,
	.exit = wrpc_net_exit,
	.recv = wrpc_net_recv,
	.send = wrpc_net_send,
};
