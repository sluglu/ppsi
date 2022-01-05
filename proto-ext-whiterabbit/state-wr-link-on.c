/*
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Aurelio Colosimo
 * Based on ptp-noposix project (see AUTHORS for details)
 *
 * Released to the public domain
 */

#include <ppsi/ppsi.h>
#include "wrc_ptp.h"

/*
 * This is the last WR state: ack the other party and go master or slave.
 * There is no timeout nor a check for is_new_state: we just do things once
 */
int wr_link_on(struct pp_instance *ppi, void *buf, int len, int new_state)
{
	struct wr_dsport *wrp = WR_DSPOR(ppi);

	if (wrp->wrMode == WR_MASTER)
		if ( msg_issue_wrsig(ppi, WR_MODE_ON) )
			return 0; /* Retry next time */

	// Success
	WRH_OPER()->enable_ptracker(ppi);
	wrp->wrModeOn =
			wrp->parentWrModeOn = TRUE;
	wrp->next_state=WRS_IDLE;

#ifdef CONFIG_ABSCAL
	/*
	 * absolute calibration only exists in arch-wrpc, so far, but
	 * we can't include wrpc headers, not available in wrs builds
	 */
	extern int ep_get_bitslide(void);

	if (wrc_ptp_is_abscal() /* WRC_MODE_ABSCAL */) {
		wrp->next_state = WRS_ABSCAL;
		/* print header for the serial port stream of stamps */
		pp_printf("### t4.phase is already corrected for bitslide\n");
		pp_printf("t1:                     t4:                  "
			  "bitslide: %d\n",ep_get_bitslide());
		pp_printf("      sec.       ns.pha       sec.       ns.pha\n");
	}
#endif

	return 0;
}
