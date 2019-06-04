/*
 * Copyright (C) 2018 CERN (www.cern.ch)
 * Author: Jean-Claude BAU & Maciej Lipinski
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */

#include <ppsi/ppsi.h>
#include <common-fun.h>
#include "wr-constants.h"
#include <math.h>

typedef struct {
	char *name;
	int (*action)(struct pp_instance *ppi, void *buf, int len, int new_state);
}wr_state_machine_t;

#define MAX_STATE_ACTIONS (sizeof(wr_state_actions)/sizeof(wr_state_machine_t))

static wr_state_machine_t wr_state_actions[] ={
		[WRS_IDLE]{
			.name="wr-idle",
			.action=wr_idle,
		},
		[WRS_PRESENT]{
			.name="wr-present",
			.action=wr_present,
		},
		[WRS_M_LOCK]{
			.name="wr_m_lock",
			.action=wr_m_lock,
		},
		[WRS_S_LOCK]{
			.name="wr-s-lock",
			.action=wr_s_lock,
		},
		[WRS_LOCKED]{
			.name="wr-locked",
			.action=wr_locked,
		},
		[WRS_CALIBRATION]{
			.name="wr-calibration",
			.action=wr_calibration,
		},
		[WRS_CALIBRATED]{
			.name="wr-calibrated",
			.action=wr_calibrated,
		},
		[WRS_RESP_CALIB_REQ]{
			.name="wr-resp-calib-req",
			.action=wr_resp_calib_req,
		},
		[WRS_WR_LINK_ON]{
			.name="wr-link-on",
			.action=wr_link_on,
		},
#ifdef CONFIG_ABSCAL
		[WRS_ABSCAL]{
			.name="absolute-calibration",
			.action=wr_abscal,
		},
#endif
};

/*
 * This hook is called by fsm to run the extension state machine.
 * It is used to send signaling messages.
 * It returns the ext-specific timeout value
 */
int wr_run_state_machine(struct pp_instance *ppi, void *buf, int len) {
	struct wr_dsport * wrp=WR_DSPOR(ppi);
	Boolean newState;
	int delay;

	if ( wrp->next_state>=MAX_STATE_ACTIONS ||  ppi->state==PPS_INITIALIZING ||  !(ppi->extState==PP_EXSTATE_ACTIVE)) {
		wrp->next_state=WRS_IDLE; // Force to IDLE state
	}

	/*
	 * Evaluation of events common to all states
	 */
	newState=wrp->state!=wrp->next_state;
	if ( newState ) {
		pp_diag(ppi, ext, 2, "WR state: LEAVE=%s, ENTER=%s\n",
				wr_state_actions[wrp->state].name,
				wr_state_actions[wrp->next_state].name);
		wrp->state=wrp->next_state;
	}

	delay=(*wr_state_actions[wrp->state].action) (ppi, buf,len, newState);

	return delay;
}

