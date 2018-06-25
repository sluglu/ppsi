/*
 * Copyright (C) 2018 CERN (www.cern.ch)
 * Author: Jean-Claude BAU & Maciej Lipinski
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */

#include <ppsi/ppsi.h>
#include "l1e-api.h"

#define MSG_OFFSET_HEADER 0
#define MSG_OFFSET_HEADER_MESSAGE_LENGTH (MSG_OFFSET_HEADER+2)
#define MSG_OFFSET_HEADER_CONTROL_FIELD (MSG_OFFSET_HEADER+32)
#define MSG_OFFSET_TARGET_PORT_IDENTITY 34
#define MSG_OFFSET_TARGET_PORT_IDENTITY_CLOCK_IDENTITY (MSG_OFFSET_TARGET_PORT_IDENTITY+0)
#define MSG_OFFSET_TARGET_PORT_IDENTITY_PORT_NUMBER    (MSG_OFFSET_TARGET_PORT_IDENTITY+8)

#define MSG_OFFSET_TLV    44
#define MSG_OFFSET_TLV_TYPE               (MSG_OFFSET_TLV  )
#define MSG_OFFSET_TLV_LENGTH_FIELD       (MSG_OFFSET_TLV+2)
#define MSG_OFFSET_TLV_L1SYNC_PEER_CONF   (MSG_OFFSET_TLV+4)
#define MSG_OFFSET_TLV_L1SYNC_PEER_ACTIVE (MSG_OFFSET_TLV+5)


#define MSG_OFFSET_HEADER_TYPE (MSG_OFFSET_HEADER+0)

#define MSG_TYPE_SIGNALING 0xC

#define MSG_HEADER_CONTROL_FIELD_ALL_OTHERS 5


#define MSG_TYPE_

#define MSG_GET_16(buf,off) (*(UInteger16 *)(buf+off))
#define MSG_GET_8(buf,off ) *(UInteger8 *)(buf+off)

#define MSG_GET_HEADER_TYPE(buf)            (MSG_GET_8(buf,MSG_OFFSET_HEADER_TYPE) & 0x0f)
#define MSG_GET_TLV_TYPE(buf)               ntohs(MSG_GET_16(buf,MSG_OFFSET_TLV_TYPE))
#define MSG_GET_TLV_LENGTH_FIELD(buf)       ntohs(MSG_GET_16(buf,MSG_OFFSET_TLV_LENGTH_FIELD))
#define MSG_GET_TLV_L1SYNC_PEER_CONF(buf)   MSG_GET_8(buf,MSG_OFFSET_TLV_L1SYNC_PEER_CONF)
#define MSG_GET_TLV_L1SYNC_PEER_ACTIVE(buf) MSG_GET_8(buf,MSG_OFFSET_TLV_L1SYNC_PEER_ACTIVE)


#define MSG_SET_HEADER_TYPE(buf,val)  	             MSG_GET_8(buf,MSG_OFFSET_HEADER_TYPE) = \
	(MSG_GET_8(buf,MSG_OFFSET_HEADER_TYPE) & 0xF0) |  val
#define MSG_SET_HEADER_MESSAGE_LENGTH(buf,val)  	 MSG_GET_16(buf,MSG_OFFSET_HEADER_MESSAGE_LENGTH) = htons(val)
#define MSG_SET_HEADER_CONTROL_FIELD(buf, val)       MSG_GET_8(buf,MSG_OFFSET_HEADER_CONTROL_FIELD) = val
#define MSG_SET_TARGET_PORT_IDENTITY(buf,clock,port) *(ClockIdentity *)(buf+MSG_OFFSET_TARGET_PORT_IDENTITY) = clock;\
	MSG_GET_16(buf,MSG_OFFSET_TARGET_PORT_IDENTITY_PORT_NUMBER) = htons(port);
#define MSG_SET_TLV_TYPE(buf,val)                    MSG_GET_16(buf,MSG_OFFSET_TLV_TYPE)=htons(val)
#define MSG_SET_TLV_LENGTH_FIELD(buf,val)            MSG_GET_16(buf,MSG_OFFSET_TLV_LENGTH_FIELD)=htons(val)
#define MSG_SET_TLV_L1SYNC_PEER_CONF(buf,val)        MSG_GET_8(buf,MSG_OFFSET_TLV_L1SYNC_PEER_CONF)= val
#define MSG_SET_TLV_L1SYNC_PEER_ACTIVE(buf,val)      MSG_GET_8(buf,MSG_OFFSET_TLV_L1SYNC_PEER_ACTIVE)=val

#define  TLV_TYPE_L1_SYNC		0x8001

#define MSG_L1SYNC_LEN 50

int l1e_pack_signal(struct pp_instance *ppi)
{
	void *buf;
	uint8_t local_config, local_active;
	L1SyncBasicPortDS_t * bds=L1E_DSPOR_BS(ppi);

	buf = ppi->tx_ptp;

	/* Changes in header */
	MSG_SET_HEADER_TYPE(buf,MSG_TYPE_SIGNALING);

	MSG_SET_HEADER_CONTROL_FIELD(buf,MSG_HEADER_CONTROL_FIELD_ALL_OTHERS);

	/* target portIdentity */
	MSG_SET_TARGET_PORT_IDENTITY(buf,
			DSPAR(ppi)->parentPortIdentity.clockIdentity,
			DSPAR(ppi)->parentPortIdentity.portNumber);


	/* L1SyncTLV */
	MSG_SET_TLV_TYPE(buf,TLV_TYPE_L1_SYNC);
	MSG_SET_TLV_LENGTH_FIELD(buf,2);
	/* O.6.4 */
	local_config = l1e_creat_L1Sync_bitmask(bds->txCoherentIsRequired,
			bds->rxCoherentIsRequired,
			bds->congruentIsRequired);
	if(bds->optParamsEnabled)
		  local_config |= L1E_OPT_PARAMS;
	
	local_active = l1e_creat_L1Sync_bitmask(bds->isTxCoherent,
			bds->isRxCoherent,
			bds->isCongruent);
	MSG_SET_TLV_L1SYNC_PEER_CONF(buf,local_config);
	MSG_SET_TLV_L1SYNC_PEER_ACTIVE(buf,local_active);

	l1e_print_L1Sync_basic_bitmaps(ppi, local_config,local_active, "Sent");

	/* header len */
	MSG_SET_HEADER_MESSAGE_LENGTH(buf,MSG_L1SYNC_LEN);

	return MSG_L1SYNC_LEN;
}

int l1e_unpack_signal(struct pp_instance *ppi, void *buf, int plen)
{
	L1SyncBasicPortDS_t * basicDS=L1E_DSPOR_BS(ppi);
	int l1sync_peer_conf, l1sync_peer_acti;

	if ( MSG_GET_HEADER_TYPE(buf) != MSG_TYPE_SIGNALING) {
		pp_diag(ppi, ext, 1, "Not a signaling message, ignore\n");
		return -1;
	}
	if (MSG_GET_TLV_TYPE(buf) != TLV_TYPE_L1_SYNC) {
		pp_diag(ppi, ext, 1, "Not L1Sync TLV, ignore\n");
		return -1;
	}
	if ( MSG_GET_TLV_LENGTH_FIELD(buf) != 2 || plen != MSG_L1SYNC_LEN) {
		pp_diag(ppi, ext, 1, "L1Sync TLV wrong length, ignore\n");
		return -1;
	}
	l1sync_peer_conf = MSG_GET_TLV_L1SYNC_PEER_CONF(buf);
	l1sync_peer_acti = MSG_GET_TLV_L1SYNC_PEER_ACTIVE(buf);

	l1e_print_L1Sync_basic_bitmaps(ppi, l1sync_peer_conf,l1sync_peer_acti, "Received");

	basicDS->peerTxCoherentIsRequired = (l1sync_peer_conf & L1E_TX_COHERENT) == L1E_TX_COHERENT;
	basicDS->peerRxCoherentIsRequired = (l1sync_peer_conf & L1E_RX_COHERENT) == L1E_RX_COHERENT;
	basicDS->peerCongruentIsRequired  = (l1sync_peer_conf & L1E_CONGRUENT)   == L1E_CONGRUENT;

	basicDS->peerIsTxCoherent     = (l1sync_peer_acti & L1E_TX_COHERENT) == L1E_TX_COHERENT;
	basicDS->peerIsRxCoherent     = (l1sync_peer_acti & L1E_RX_COHERENT) == L1E_RX_COHERENT;
	basicDS->peerIsCongruent      = (l1sync_peer_acti & L1E_CONGRUENT)   == L1E_CONGRUENT;

	return 0;
}
