/*
 * Copyright (C) 2011 CERN (www.cern.ch)
 * Author: Alessandro Rubini
 *
 * Released to the public domain
 */

/*
 * This is the main loop for the wr-switch architecture. It's amost
 * the same as the unix main loop, but we must serve RPC calls too
 */
#include <stdlib.h>
#include <errno.h>
#include <sys/select.h>
#include <netinet/if_ether.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

#include <ppsi/ppsi.h>
#include <ppsi-wrs.h>
#include <hal_exports.h>
#include <common-fun.h>

/* Call pp_state_machine for each instance. To be called periodically,
 * when no packets are incoming */
static int run_all_state_machines(struct pp_globals *ppg)
{
	int j;
	int delay_ms = 0, delay_ms_j;

	for (j = 0; j < ppg->nlinks; j++) {
		struct pp_instance *ppi = INST(ppg, j);
		int old_lu = ppi->link_up;
		struct hal_port_state *p;

		/* FIXME: we should save this pointer in the ppi itself */
		p = pp_wrs_lookup_port(ppi->iface_name);
		if (!p) {
			fprintf(stderr, "ppsi: can't find %s in shmem\n",
				ppi->iface_name);
			continue;
		}

		ppi->link_up =
			(p->state != HAL_PORT_STATE_LINK_DOWN &&
			 p->state != HAL_PORT_STATE_DISABLED);

		if (old_lu != ppi->link_up) {

			pp_diag(ppi, fsm, 1, "iface %s went %s\n",
				ppi->iface_name, ppi->link_up ? "up":"down");

			if (ppi->link_up) {
				uint32_t bit_slide_ps;

				ppi->state = PPS_INITIALIZING;
				if ( wrs_read_correction_data(ppi,NULL,NULL,&bit_slide_ps)!= WRH_HW_CALIB_OK ) {
				      pp_diag(ppi, fsm, 1, "Cannot read bit_slide value values\n");
				      bit_slide_ps=0;
				}
		        pp_diag(ppi, fsm, 1, "semistaticLatency(bit-slide)=%u [ps]\n",(unsigned int)bit_slide_ps);
				ppi->timestampCorrectionPortDS.semistaticLatency= picos_to_interval(bit_slide_ps);
			}
			else {
				ppi->next_state = PPS_DISABLED;
				pp_leave_current_state(ppi);
				ppi->n_ops->exit(ppi);
				ppi->frgn_rec_num = 0;
				ppi->frgn_rec_best = -1;
				if (ppg->ebest_idx == ppi->port_idx)
					if( ppi->ext_hooks->servo_reset)
						(*ppi->ext_hooks->servo_reset)(ppi);
			}
		}

		/* Do not call state machine if link is down */
		if (ppi->link_up)
			delay_ms_j = pp_state_machine(ppi, NULL, 0);
		else
			delay_ms_j = PP_DEFAULT_NEXT_DELAY_MS;

		/* delay_ms is the least delay_ms among all instances */
		if (j == 0)
			delay_ms = delay_ms_j;
		if (delay_ms_j < delay_ms)
			delay_ms = delay_ms_j;
	}

	/* BMCA must run at least once per announce interval 9.2.6.8 */
	if (pp_gtimeout(ppg, PP_TO_BMC)) {
		bmc_calculate_ebest(ppg); /* Calculation of erbest, ebest ,... */
		pp_gtimeout_set(ppg, PP_TO_BMC,TMO_DEFAULT_BMCA_MS);
		delay_ms=0;
	} else {
		/* check if the BMC timeout is the next to run */
		int delay_bmca;
		if ( (delay_bmca=pp_gnext_delay_1(ppg,PP_TO_BMC))<delay_ms )
			delay_ms=delay_bmca;
	}

	return delay_ms;
}

static int  alarmDetected=1;

static void sched_handler(int sig, siginfo_t *si, void *uc)
{
  alarmDetected=1;
}

static void init_alarm(timer_t *timerid) {
	    struct sigevent sev;
	    struct sigaction sa;

	    /* Set the signal handler */
	    sa.sa_flags = SA_SIGINFO;
	    sa.sa_sigaction = sched_handler;
	    sigemptyset(&sa.sa_mask);
	    if (sigaction(SIGALRM, &sa, NULL) == -1) {
			fprintf(stderr, "ppsi: cannot set signal handler\n");
			exit(1);
	    }

	    /* Create the timer */
		sev.sigev_notify = SIGEV_SIGNAL;
		sev.sigev_signo = SIGALRM;
		sev.sigev_value.sival_ptr = timerid;
		if (timer_create(CLOCK_MONOTONIC, &sev, timerid) == -1) {
			fprintf(stderr, "ppsi: Cannot create timer\n");
			exit(1);
		}
}

static void start_alarm(timer_t *timerid, int delay_ms) {
	struct itimerspec its;

    its.it_value.tv_sec = delay_ms/1000;
    its.it_value.tv_nsec = (delay_ms%1000) * 1000000;
    its.it_interval.tv_sec =
    its.it_interval.tv_nsec = 0;

    if (timer_settime(*timerid, 0, &its, NULL) == -1){
		fprintf(stderr, "ppsi: Cannot start timer\n");
	}

}

static int stop_alarm(timer_t *timerid) {
	struct itimerspec its;
	struct itimerspec ito;

    its.it_value.tv_sec =
    its.it_value.tv_nsec =
    its.it_interval.tv_sec =
    its.it_interval.tv_nsec = 0;

    if (timer_settime(*timerid, 0, &its, &ito) == -1){
		fprintf(stderr, "ppsi: Cannot stop timer\n");
		return 0;
	}
    return (int) (ito.it_value.tv_sec*1000+ito.it_value.tv_nsec/1000000);
}


void wrs_main_loop(struct pp_globals *ppg)
{
	struct pp_instance *ppi;
	int skipped_checks=0;
	int delay_ms;
    timer_t timerid;
	int j;

	/* Initialize each link's state machine */
	for (j = 0; j < ppg->nlinks; j++) {

		ppi = INST(ppg, j);

		/*
		* The main loop here is based on select. While we are not
		* doing anything else but the protocol, this allows extra stuff
		* to fit.
		*/
		ppi->is_new_state = 1;
	}

	delay_ms = run_all_state_machines(ppg);

    /* Create the timer */
	init_alarm(&timerid);

	while (1) {
		int packet_available;

		/* Checking of received frames or minirpc are not done if the delay
		 * is 0 or the scheduling alarm is raised. These checks must be forced sometime
		 * (see skipped_checks)
		 */
		if ( !alarmDetected && (skipped_checks > 10 || delay_ms !=0) ) {
			minipc_server_action(ppsi_ch, 1 /* ms */);
			packet_available = wrs_net_ops.check_packet(ppg, delay_ms);
			skipped_checks=0;
		} else {
			skipped_checks++;
			packet_available=0;
		}
#if 0
		/*
		 * If Ebest was changed in previous loop, run best
		 * master clock before checking for new packets, which
		 * would affect port state again
		 */
		if (ppg->ebest_updated) {
			for (j = 0; j < ppg->nlinks; j++) {
				int new_state;
				struct pp_instance *ppi = INST(ppg, j);
				new_state = bmc(ppi);
				if (new_state != ppi->state) {
					ppi->state = new_state;
					ppi->is_new_state = 1;
				}
			}
			ppg->ebest_updated = 0;
		}
#endif

		if ((packet_available<=0) && (alarmDetected || (delay_ms==0))) {
			/* Time to run the state machine */
			stop_alarm(&timerid); /* Clear previous alarm */
		    delay_ms = run_all_state_machines(ppg);
		    alarmDetected=0;
		    if ( delay_ms != 0 ) {
		    	/* Start the alarm */
				start_alarm(&timerid,delay_ms);
		    }
			continue;
		}

		if (packet_available > 0 ) {
			/* If delay_ms is -1, the above ops.check_packet will continue
			 * consuming the previous timeout (see its implementation).
			 * This ensures that every state machine is called at least once
			 * every delay_ms */
			delay_ms = -1;
			for (j = 0; j < ppg->nlinks; j++) {
				int tmp_d,i;
				ppi = INST(ppg, j);

				if ((ppi->ch[PP_NP_GEN].pkt_present) ||
					(ppi->ch[PP_NP_EVT].pkt_present)) {

					i = __recv_and_count(ppi, ppi->rx_frame,
							PP_MAX_FRAME_LENGTH - 4,
							&ppi->last_rcv_time);

					if (i == PP_RECV_DROP) {
						continue; /* dropped or not for us */
					}
					if (i == -1) {
						pp_diag(ppi, frames, 1,	"Receive Error %i: %s\n",errno, strerror(errno));
						continue;
					}

					tmp_d = pp_state_machine(ppi, ppi->rx_ptp,
						i - ppi->rx_offset);

					if ((delay_ms == -1) || (tmp_d < delay_ms))
						delay_ms = tmp_d;
				}
			}
			if ((delay_ms>0) &&  !alarmDetected ) {
				int rem_delay_ms=stop_alarm(&timerid); /* Stop alarm and get remaining delay */

				if ( rem_delay_ms < delay_ms) {
					/* Re-adjust delay_ms */
					delay_ms=rem_delay_ms;
				}
				alarmDetected=0;
				start_alarm(&timerid,delay_ms); /* Start alarm */
			}
		}
	}
}
