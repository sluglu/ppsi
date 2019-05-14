#include <ppsi/ppsi.h>
#include <ppsi/assert.h>
#include <libwr/shmem.h>
#include "../proto-standard/common-fun.h"

/* Enable tracking by default. Disabling the tracking is used for demos. */
static int wr_tracking_enabled = 1;

void wr_servo_enable_tracking(int enable)
{
	wr_tracking_enabled = enable;
}

int wr_servo_init(struct pp_instance *ppi)
{
	if ( wrh_servo_init(ppi) ) {
		/* Reset extension servo data */
		memset(WRE_SRV(ppi),0,sizeof(wr_servo_ext_t));
		return 1;
	} else
		return 0;
}

int wr_servo_got_sync(struct pp_instance *ppi) {
	/* Re-adjust T1 and T2 */
	wr_servo_ext_t *se=WRE_SRV(ppi);
	wrh_servo_t *s=WRH_SRV(ppi);

	if (s->doRestart) {
		// Error detected by the servo.
		s->doRestart=FALSE;
		if ( ppi->state==PPS_SLAVE ) {
			// Restart calibration
			ppi->next_state=PPS_UNCALIBRATED;
		}
	}
	pp_time_add(&ppi->t1,&se->delta_txm);
	pp_time_sub(&ppi->t2,&se->delta_rxs);
	return wrh_servo_got_sync(ppi);
}

int wr_servo_got_resp(struct pp_instance *ppi) {
	/* Re-adjust T3 and T4 */
	wr_servo_ext_t *se=WRE_SRV(ppi);

	pp_time_add(&ppi->t3,&se->delta_txs);
	pp_time_sub(&ppi->t4,&se->delta_rxm);
	return wrh_servo_got_resp(ppi);
}

int wr_servo_got_presp(struct pp_instance *ppi) {
	/* Re-adjust T3,T4,T5 and T6 */
	wr_servo_ext_t *se=WRE_SRV(ppi);

	pp_time_add(&ppi->t3,&se->delta_txs);
	pp_time_sub(&ppi->t4,&se->delta_rxm);
	pp_time_add(&ppi->t5,&se->delta_txm);
	pp_time_sub(&ppi->t6,&se->delta_rxs);
	return wrh_servo_got_presp(ppi);
}

