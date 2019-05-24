#include <ppsi/ppsi.h>
#include "common-fun.h"

/*
 * This is similar to master state, but it only sends sync, that
 * is going to be timestampedboth internally and externally (on a scope).
 * We should send it as close as possible to the pps signal.
 */


/* Calculate when is the next pps going to happen */
static int next_pps_ms(struct pp_instance *ppi, struct pp_time *t)
{
	int nsec;

	TOPS(ppi)->get(ppi, t);
	nsec = t->scaled_nsecs >> 16;
	return  1000 - (nsec / 1000 / 1000);
}

/*
 * This is using a software loop during the last 10ms in order to get
 * right after the pps event
 */
#define WR_TMO_NAME "WR_ABSCAL"

int wr_abscal(struct pp_instance *ppi, void *buf, int plen, int new_state)
{
	struct pp_time t;
	struct wr_dsport *wrp = WR_DSPOR(ppi);
	int len, i;

	if (new_state) {
		/* add 1s to be enough in the future, the first time */
		pp_timeout_set_rename(ppi, wrTmoIdx, 990 + next_pps_ms(ppi, &t),WR_TMO_NAME);
		return 0;
	}

	i = next_pps_ms(ppi, &t) - 10;
	if (pp_timeout(ppi, wrTmoIdx)) {
		uint64_t secs = t.secs;

		WRH_OPER()->enable_timing_output(GLBS(ppi), 1);

		/* Wait for the second to tick */
		while( TOPS(ppi)->get(ppi, &t), t.secs == secs)
			;

		/* Send sync, no f-up -- actually we could send any frame */
		TOPS(ppi)->get(ppi, &t);
		len = msg_pack_sync(ppi, &t);
		__send_and_log(ppi, len, PP_NP_EVT);

		/* And again next second */
		pp_timeout_set(ppi, wrTmoIdx, next_pps_ms(ppi, &t) - 10);
		return  next_pps_ms(ppi, &t) - 10;
	}
	/* no timeout: wait according to next_pps_ms calculated earlier */
	return  i > 0 ? i : 0;
}
