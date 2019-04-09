/*
 * Copyright (C) 2011 CERN (www.cern.ch)
 * Author: Alessandro Rubini
 *
 * Released to the public domain
 */
#include <ppsi/ppsi.h>

unsigned long pp_global_d_flags; /* This is the only "global" file in ppsi */

/*
 * This is somehow a duplicate of __pp_diag, but I still want
 * explicit timing in the fsm enter/stay/leave messages,
 * while there's no need to add times to all diagnostic messages
 */
static void pp_fsm_printf(struct pp_instance *ppi, char *fmt, ...)
{
	va_list args;
	struct pp_time t;
	unsigned long oflags = pp_global_d_flags;

	if (!pp_diag_allow(ppi, fsm, 1))
		return;

	/* temporarily set NOTIMELOG, as we'll print the time ourselves */
	pp_global_d_flags |= PP_FLAG_NOTIMELOG;
	ppi->t_ops->get(ppi, &t);
	pp_global_d_flags = oflags;

	pp_printf("diag-fsm-1-%s: %09d.%03d: ", ppi->port_name,
		  (int)t.secs, (int)((t.scaled_nsecs >> 16)) / 1000000);
	va_start(args, fmt);
	pp_vprintf(fmt, args);
	va_end(args);
}

/*
 * Diagnostics about state machine, enter, leave, remain
 */
enum {
	STATE_ENTER,
	STATE_LOOP,
	STATE_LEAVE
};

static void pp_diag_fsm(struct pp_instance *ppi, char *name, int sequence,
			int len)
{
	if (sequence == STATE_ENTER) {
		/* enter with or without a packet len */
		pp_fsm_printf(ppi, "ENTER %s, packet len %i\n",
			  name, len);
		return;
	}
	if (sequence == STATE_LOOP) {
		pp_fsm_printf(ppi, "%s: reenter in %i ms\n", name,
				ppi->next_delay);
		return;
	}
	/* leave has one \n more, so different states are separate */
	pp_fsm_printf(ppi, "LEAVE %s (next: %3i)\n\n",
		      name, ppi->next_state);
}

static struct pp_state_table_item *
get_current_state_table_item(struct pp_instance *ppi)
{
	struct pp_state_table_item *ip = ppi->current_state_item;

	/* Avoid searching if we already know where we are */
	ppi->is_new_state = 0;
	if (ip && ip->state == ppi->state)
		return ip;
	ppi->is_new_state = 1;

	/* a linear search is affordable up to a few dozen items */
	for (ip = pp_state_table; ip->state != PPS_END_OF_TABLE; ip++)
		if (ip->state == ppi->state) {
			ppi->current_state_item = ip;
			return ip;
		}
	return NULL;
}

char *get_state_as_string(struct pp_instance *ppi, int state) {
	static char *def="INVALID";
	struct pp_state_table_item *ip = ppi->current_state_item;

	for (ip = pp_state_table; ip->state != PPS_END_OF_TABLE; ip++)
		if (ip->state == state) {
			return ip->name;
		}
	return def;
}

/*
 * Returns delay to next state, which is always zero.
 */
int pp_leave_current_state(struct pp_instance *ppi)
{
	/* If something has to be done in an extension */
	if ( ppi->ext_hooks->state_change)
		ppi->ext_hooks->state_change(ppi);
	
	/* if the next or old state is non standard PTP reset all timeouts */
	if ((ppi->state > PPS_LAST_STATE) || (ppi->next_state > PPS_LAST_STATE))
		pp_timeout_setall(ppi);

	ppi->state = ppi->next_state;
	ppi->flags &= ~PPI_FLAGS_WAITING;
	pp_diag_fsm(ppi, ppi->current_state_item->name, STATE_LEAVE, 0);
	/* next_delay unused: go to new state now */
	return 0;
}

/*
 * Checks whether a packet has to be discarded and maybe updates port status
 * accordingly. Returns new packet length (0 if packet has to be discarded)
 */
static int pp_packet_prefilter(struct pp_instance *ppi)
{
	MsgHeader *hdr = &ppi->received_ptp_header;

	/*
	 * 9.5.1:
	 * Only PTP messages where the domainNumber field of the PTP message
	 * header (see 13.3.2.5) is identical to the defaultDS.domainNumber
	 * shall be accepted for processing by the protocol.
	 */
	if (hdr->domainNumber != GDSDEF(GLBS(ppi))->domainNumber) {
		pp_diag(ppi, frames, 1, "Wrong domain %i: discard\n",
			hdr->domainNumber);
		return -1;
	}

	/*
	 * Alternate masters (17.4) not supported
	 * 17.4.2, NOTE:
	 * A slave node that does not want to use information from alternate
	 * masters merely ignores all messages with alternateMasterFlag TRUE.
	 */
	if (hdr->flagField[0] & PP_ALTERNATE_MASTER_FLAG) {
		pp_diag(ppi, frames, 1, "Alternate master: discard\n");
		return -1;
	}

	/*
	 * If the message is from the same port that sent it, we should
	 * discard it (9.5.2.2)
	 */
	if (!memcmp(&ppi->received_ptp_header.sourcePortIdentity,
		    &DSPOR(ppi)->portIdentity,
		    sizeof(PortIdentity))) {
		pp_diag(ppi, frames, 1, "Looping frame: discard\n");
		return -1;
	}

	/*
	 * 9.5.2.3 & 9.5.2.2: For BCs the BMC (an extention to it)
	 * handles the Announce (go to Passive), other messages are dropped
	 */	
	if (!memcmp(&hdr->sourcePortIdentity.clockIdentity,
		&DSPOR(ppi)->portIdentity.clockIdentity,
		sizeof(ClockIdentity))) {
		if ( get_numberPorts(DSDEF(ppi)) > 1) {
			/* Announces are handled by the BMC, since otherwise the state 
			 * also the PASSIVE states in this case is overwritten */
			if (hdr->messageType != PPM_ANNOUNCE) {
				/* ignore messages, except announce coming from its own clock */
				return -1;	
			}		
		}
	}
	
	return 0;
}

/* used to make basic checks before the individual state is called */
static int type_length[__PP_NR_MESSAGES_TYPES] = {
	[PPM_SYNC]		= PP_SYNC_LENGTH,
	[PPM_DELAY_REQ]		= PP_DELAY_REQ_LENGTH,
	[PPM_PDELAY_REQ]	= PP_PDELAY_REQ_LENGTH,
	[PPM_PDELAY_RESP]	= PP_PDELAY_RESP_LENGTH,
	[PPM_FOLLOW_UP]		= PP_FOLLOW_UP_LENGTH,
	[PPM_DELAY_RESP]	= PP_DELAY_RESP_LENGTH,
	[PPM_PDELAY_R_FUP]	= PP_PDELAY_RESP_FOLLOW_UP_LENGTH,
	[PPM_ANNOUNCE]		= PP_ANNOUNCE_LENGTH,
	[PPM_SIGNALING]		= PP_HEADER_LENGTH,
	[PPM_MANAGEMENT]	= PP_MANAGEMENT_LENGTH,
};

static int fsm_unpack_verify_frame(struct pp_instance *ppi,
				   void *buf, int len)
{
	int msgtype = 0;

	if (len)
		msgtype = ((*(UInteger8 *) (buf + 0)) & 0x0F);
	if (msgtype >= __PP_NR_MESSAGES_TYPES || len < type_length[msgtype])
		return 1; /* too short */
	if (((*(UInteger8 *) (buf + 1)) & 0x0F) != 2)
		return 1; /* wrong ptp version */
	return msg_unpack_header(ppi, buf, len);
}

/*
 * This is the state machine code. i.e. the extension-independent
 * function that runs the machine. Errors are managed and reported
 * here (based on the diag module). The returned value is the time
 * in milliseconds to wait before reentering the state machine.
 * the normal protocol. If an extended protocol is used, the table used
 * is that of the extension, otherwise the one in state-table-default.c
 */

int pp_state_machine(struct pp_instance *ppi, void *buf, int len)
{
	struct pp_state_table_item *ip;
	struct pp_time *t = &ppi->last_rcv_time;
	int state, err = 0;
	int msgtype;

	if (len > 0) {
		msgtype = ((*(UInteger8 *) (buf + 0)) & 0x0F);
		pp_diag(ppi, frames, 1,
			"RECV %02d bytes at %9d.%09d.%03d (type %x, %s)\n",
			len, (int)t->secs, (int)(t->scaled_nsecs >> 16),
			((int)(t->scaled_nsecs & 0xffff) * 1000) >> 16,
			msgtype, pp_msgtype_name[msgtype]);
	}

	/*
	 * Discard too short packets
	 */
	if (len < PP_HEADER_LENGTH) {
		len = 0;
		buf = NULL;
	}

	/*
	 * Since all ptp frames have the same header, parse it now,
	 * and centralize some error check.
	 * In case of error continue without a frame, so the current
	 * ptp state can update ppi->next_delay and return a proper value
	 */
	err = fsm_unpack_verify_frame(ppi, buf, len);
	if (err) {
		len = 0;
		buf = NULL;
	}

	state = ppi->state;

	ip = get_current_state_table_item(ppi);
	if (!ip) {
		pp_printf("fsm: Unknown state for port %s\n", ppi->port_name);
		return 10000; /* No way out. Repeat message every 10s */
	}

	/* found: handle this state */
	ppi->next_state = state;
	ppi->next_delay = 0;
	if (ppi->is_new_state)
		pp_diag_fsm(ppi, ip->name, STATE_ENTER, len);

	/*
	 * Possibly filter out buf and maybe update port state
	 */
	if (buf) {
		err = pp_packet_prefilter(ppi);
		if (err < 0) {
			buf = NULL;
			len = 0;
		}
	}

	if (ppi->state != ppi->next_state)
		return pp_leave_current_state(ppi);

	if (len) {
		if ( ppi->ext_enabled &&  !ppi->ptp_msg_received ) {
			/* First frame received since instance initialization */
			int tmo;

			ppi->ptp_msg_received=TRUE;
			if (is_ext_hook_available(ppi,get_tmo_lstate_detection) )
				tmo=(*ppi->ext_hooks->get_tmo_lstate_detection)(ppi);
			else
				tmo= is_externalPortConfigurationEnabled(DSDEF(ppi)) ?
						6000 /* JCB: Default value. Is it correct ? */
						: pp_timeout_get(ppi,PP_TO_ANN_RECEIPT);
			pp_timeout_set(ppi,PP_TO_PROT_STATE, tmo);
			lstate_set_link_pdetection(ppi);
		}
	} else
		ppi->received_ptp_header.messageType = PPM_NO_MESSAGE;

	err = ip->f1(ppi, buf, len);
	if (err)
		pp_printf("fsm for %s: Error %i in %s\n",
			  ppi->port_name, err, ip->name);

	/* done: if new state mark it, and enter it now (0 ms) */
	if (ppi->state != ppi->next_state)
		return pp_leave_current_state(ppi);

	/* Check protocol state */
	if (  ppi->protocol_extension == PPSI_EXT_NONE ) {
		lstate_set_link_none(ppi);
	} else {
		if ( ppi->ext_enabled ) {
			if ( ppi->link_state==PP_LSTATE_PROTOCOL_ERROR ||
					( ppi->link_state!=PP_LSTATE_LINKED && ppi->ptp_msg_received  && pp_timeout(ppi, PP_TO_PROT_STATE)) ) {
				if ( ppi->ptp_support )
					lstate_disable_extension(ppi);
				else
					lstate_set_link_failure(ppi);
			}
		} else  {
			lstate_set_link_failure(ppi);
		}
	}

	/* run bmc independent of state, and since not message driven do this
	 * here 9.2.6.8 */
	if ( ppi->bmca_execute) {

		ppi->bmca_execute=0; /* Clear the trigger */
		ppi->next_state = bmc_apply_state_descision(ppi);

		/* done: if new state mark it, and enter it now (0 ms) */
		if (ppi->state != ppi->next_state)
			return pp_leave_current_state(ppi);
	}

	pp_diag_fsm(ppi, ip->name, STATE_LOOP, 0);

	/* Run the extension state machine. The extension can provide its own time-out */
	if ( is_ext_hook_available(ppi,run_ext_state_machine) ) {
		int delay = ppi->ext_hooks->run_ext_state_machine(ppi,buf,len);

		if ( ppi->ext_enabled  && ppi->link_state==PP_LSTATE_PROTOCOL_ERROR) {
			if (ppi->ptp_support ) {
				lstate_disable_extension(ppi);
			}
			else
				lstate_set_link_failure(ppi);
		}
		/* if new state mark it, and enter it now (0 ms) */
		if (ppi->state != ppi->next_state)
			return pp_leave_current_state(ppi);

		if (delay < ppi->next_delay)
			ppi->next_delay=delay;
	}

	return ppi->next_delay;
}
