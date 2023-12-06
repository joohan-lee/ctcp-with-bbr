#include "ctcp_bbr.h"
#include "ctcp.h"
#include "ctcp_utils.h"

/* To estimate if BBR_STARTUP mode (i.e. high_gain) has filled pipe. */
static uint32_t bbr_full_bw_thresh = BBR_UNIT * 5 / 4;  /* bw up 1.25x per round? */
static uint32_t bbr_full_bw_cnt    = 3;    /* N rounds w/o bw growth -> pipe full */

/* Do we estimate that STARTUP filled the pipe? */
static bool bbr_full_bw_reached(ctcp_bbr_t* bbr)
{
	return bbr->full_bw_cnt >= bbr_full_bw_cnt;
}

/* Return the windowed max recent bandwidth sample, in pkts/uS << BW_SCALE. */
static uint32_t bbr_max_bw(ctcp_bbr_t* bbr)
{
	return minmax_get(&bbr->bw);
}

static void bbr_reset_startup_mode(ctcp_bbr_t* bbr)
{
	bbr->mode = BBR_STARTUP;
	bbr->pacing_gain = bbr_high_gain;
	bbr->cwnd_gain	 = bbr_high_gain;
}

/* Estimate the bandwidth based on how fast packets are delivered */
static void bbr_update_bw(ctcp_bbr_t* bbr, ctcp_transmission_info_t* trans_info)
{
	ctcp_rs_t* rs = trans_info->rs;
	if (rs->delivered < 0)
		return; /* Not a valid observation */

	bbr->delivered_pkts_num += 1;
	bbr->prior_delivered_time_us = trans_info->ack_time_us;

	uint32_t sent_pkts_this_time = bbr->delivered_pkts_num - rs->delivered; // num of packets sent
	uint32_t delivery_rate = (sent_pkts_this_time << BW_SCALE) / (bbr->prior_delivered_time_us - rs->prior_mstamp); // bw

	/* If this sample is application-limited, it is likely to have a very
	 * low delivered count that represents application behavior rather than
	 * the available network rate. Such a sample could drag down estimated
	 * bw, causing needless slow-down. Thus, to continue to send at the
	 * last measured network rate, we filter out app-limited samples unless
	 * they describe the path bw at least as well as our bw model.
	 *
	 * So the goal during app-limited phase is to proceed with the best
	 * network rate no matter how long. We automatically leave this
	 * phase when app writes faster than the network can deliver :)
	 */
	if (!rs->is_app_limited || delivery_rate >= bbr_max_bw(bbr)) {
		/* Incorporate new sample into our max bw filter. */
		minmax_insert(&bbr->bw, bbr->rtt_cnt, delivery_rate);
	}
}

/* 
 * returns BDP in bytes that was calculated by max_bw and min_rtt_us
*/
static uint64_t bdp_in_bytes(ctcp_bbr_t* bbr, uint32_t gain){
	uint32_t bw = bbr_max_bw(bbr); // This is bw based on number of pkts. Should multiply MAX_SEG_DATA_SIZE to present in bytes.
    uint64_t bdp = (uint64_t)bw * bbr->min_rtt_us;
	fprintf(stderr, "bdp: %lu\n", bdp); // XXX

	uint64_t bdp_btyes;

	bdp_btyes = (((bdp * gain) >> BBR_SCALE) * MAX_SEG_DATA_SIZE);
	bdp_btyes >>= BW_SCALE;
	// fprintf(stderr, "bdp_btyes: %lu\n", bdp_btyes); // XXX
	// fprintf(stderr, "bdp_btyes >> BW_SCALE: %lu\n", bdp_btyes >> BW_SCALE); // XXX

	return bdp_btyes;
}

/* End cycle phase if it's time and/or we hit the phase's in-flight target. */
static bool bbr_is_next_cycle_phase(ctcp_state_t* state,
				    ctcp_bbr_t* bbr)
{
	uint16_t is_full_length =
		(bbr->prior_delivered_time_us - bbr->cycle_mstamp) >
		bbr->min_rtt_us;
	
	uint32_t inflight;

	/* The pacing_gain of 1.0 paces at the estimated bw to try to fully
	 * use the pipe without increasing the queue.
	 */
	if (bbr->pacing_gain == BBR_UNIT)
		return is_full_length;

	inflight = state->tx_in_flight_bytes;  /* what was in-flight before ACK? */

	/* A pacing_gain > 1.0 probes for bw by trying to raise inflight to at
	 * least pacing_gain*BDP; this may take more than min_rtt if min_rtt is
	 * small (e.g. on a LAN). We do not persist if packets are lost, since
	 * a path with small buffers may not hold that much.
	 */
	if (bbr->pacing_gain > BBR_UNIT)
		return is_full_length &&
			(inflight >= bdp_in_bytes(bbr, bbr->pacing_gain));

	/* A pacing_gain < 1.0 tries to drain extra queue we added if bw
	 * probing didn't find more bw. If inflight falls to match BDP then we
	 * estimate queue is drained; persisting would underutilize the pipe.
	 */
	return is_full_length ||
		bdp_in_bytes(bbr, BBR_UNIT);
}

static void bbr_advance_cycle_phase(ctcp_bbr_t* bbr)
{
	bbr->cycle_idx = (bbr->cycle_idx + 1) & (CYCLE_LEN - 1);
	bbr->cycle_mstamp = monotonic_current_time_us();
	bbr->pacing_gain = bbr_pacing_gain[bbr->cycle_idx];
}

/* Gain cycling: cycle pacing gain to converge to fair share of available bw. */
static void bbr_update_cycle_phase(ctcp_state_t* state, ctcp_bbr_t* bbr)
{
	if ((bbr->mode == BBR_PROBE_BW) && 
	    bbr_is_next_cycle_phase(state, bbr))
		bbr_advance_cycle_phase(bbr); // update pacing gain
}

/* Estimate when the pipe is full, using the change in delivery rate: BBR
 * estimates that STARTUP filled the pipe if the estimated bw hasn't changed by
 * at least bbr_full_bw_thresh (25%) after bbr_full_bw_cnt (3) non-app-limited
 * rounds. Why 3 rounds: 1: rwin autotuning grows the rwin, 2: we fill the
 * higher rwin, 3: we get higher delivery rate samples. Or transient
 * cross-traffic or radio noise can go away. CUBIC Hystart shares a similar
 * design goal, but uses delay and inter-ACK spacing instead of bandwidth.
 *
 * \summary: Check if bw is full by checking whether there is significant bw increase in 3 RTTs.
 */
static void bbr_check_full_bw_reached(ctcp_state_t* state,
				      ctcp_bbr_t* bbr, ctcp_rs_t* rs)
{
	uint32_t bw_thresh;

	if (bbr_full_bw_reached(bbr) || rs->is_app_limited)
		return;

	bw_thresh = (uint32_t)((uint64_t)bbr->full_bw * bbr_full_bw_thresh >> BBR_SCALE);
	if (bbr_max_bw(bbr) >= bw_thresh) {
		bbr->full_bw = bbr_max_bw(bbr);
		bbr->full_bw_cnt = 0;
		return;
	}
	++bbr->full_bw_cnt;
}

static void bbr_reset_probe_bw_mode(ctcp_bbr_t* bbr)
{
	bbr->mode = BBR_PROBE_BW;
	bbr->pacing_gain = BBR_UNIT;
	bbr->cwnd_gain = bbr_cwnd_gain;
	bbr->cycle_idx = 0;
	// bbr->cycle_idx = CYCLE_LEN - 1 - prandom_u32_max(bbr_cycle_rand);
	bbr_advance_cycle_phase(bbr);	/* flip to next phase of gain cycle */
}

/* If pipe is probably full, drain the queue and then enter steady-state. */
static void bbr_check_drain(ctcp_state_t* state, ctcp_bbr_t* bbr, ctcp_rs_t* rs)
{
	if (bbr->mode == BBR_STARTUP && bbr_full_bw_reached(bbr)) {
		bbr->mode = BBR_DRAIN;	/* drain queue we created */
		bbr->pacing_gain = bbr_drain_gain;	/* pace slow to drain */
		bbr->cwnd_gain = bbr_high_gain;	/* maintain cwnd */
	}	/* fall through to check if in-flight is already small: */
	if (bbr->mode == BBR_DRAIN && state->tx_in_flight_bytes <=
	    bdp_in_bytes(bbr, BBR_UNIT))
		bbr_reset_probe_bw_mode(bbr);  /* we estimate queue is drained */
}

static void bbr_reset_mode(ctcp_bbr_t* bbr)
{
	if (!bbr_full_bw_reached(bbr))
		bbr_reset_startup_mode(bbr);
	else
		bbr_reset_probe_bw_mode(bbr);
}

/* The goal of PROBE_RTT mode is to have BBR flows cooperatively and
 * periodically drain the bottleneck queue, to converge to measure the true
 * min_rtt (unloaded propagation delay). This allows the flows to keep queues
 * small (reducing queuing delay and packet loss) and achieve fairness among
 * BBR flows.
 *
 * The min_rtt filter window is 10 seconds. When the min_rtt estimate expires,
 * we enter PROBE_RTT mode and cap the cwnd at bbr_cwnd_min_target=4 packets.
 * After at least bbr_probe_rtt_mode_ms=200ms and at least one packet-timed
 * round trip elapsed with that flight size <= 4, we leave PROBE_RTT mode and
 * re-enter the previous mode. BBR uses 200ms to approximately bound the
 * performance penalty of PROBE_RTT's cwnd capping to roughly 2% (200ms/10s).
 *
 * Note that flows need only pay 2% if they are busy sending over the last 10
 * seconds. Interactive applications (e.g., Web, RPCs, video chunks) often have
 * natural silences or low-rate periods within 10 seconds where the rate is low
 * enough for long enough to drain its queue in the bottleneck. We pick up
 * these min RTT measurements opportunistically with our min_rtt filter. :-)
 */
static void bbr_update_min_rtt(ctcp_state_t* state, ctcp_bbr_t* bbr, ctcp_transmission_info_t* trans_info)
{
	uint64_t measured_rtt = trans_info->ack_time_us - trans_info->send_time_us;
	_log_info("[BBR] measured rtt: %lu\n", measured_rtt);

	uint64_t now_in_us = monotonic_current_time_us();

	int32_t filter_expired;

	/* Track min RTT seen in the min_rtt_win_sec filter window: */
	filter_expired = ((now_in_us - bbr->min_rtt_stamp) >= (bbr_min_rtt_win_sec * 1000000llu));

	/* Update min_rtt_us if either of followings is true.
	* - BBR_STARTUP
	* - measured_rtt <= current min_rtt_us,
	* - bbr_min_rtt_win_sec(10) seconds elapsed after last update of min_rtt_us, or
	*/
	if (bbr->mode == BBR_STARTUP || bbr->min_rtt_us >= measured_rtt || filter_expired) {
		bbr->min_rtt_us = measured_rtt;
		bbr->min_rtt_stamp = now_in_us;
	}

	if ((bbr_probe_rtt_mode_ms * 1000) > 0 && filter_expired &&
	    bbr->mode != BBR_PROBE_RTT) {
		bbr->mode = BBR_PROBE_RTT;  /* dip, drain queue */
		bbr->pacing_gain = BBR_UNIT;
		bbr->cwnd_gain = BBR_UNIT;
		// bbr_save_cwnd(sk);  /* note cwnd so we can restore it */
		bbr->prior_cwnd = MAX(bbr->prior_cwnd, state->cwnd); /* note cwnd so we can restore it */
		bbr->probe_rtt_done_stamp_us = 0; /* microsecond(usec) */
	}

	if (bbr->mode == BBR_PROBE_RTT) {
		/* Ignore low rate samples during this mode. */
		ctcp_rs_t* rs = trans_info->rs;
		rs->is_app_limited =
			(rs->delivered + state->tx_in_flight_bytes) ? : 1;
		
		/* Maintain min packets in flight for max(200 ms, 1 round). */
		if (!bbr->probe_rtt_done_stamp_us &&
		    ll_length(state->segments) <= bbr_cwnd_min_target) {
			bbr->probe_rtt_done_stamp_us = monotonic_current_time_us() +
				bbr_probe_rtt_mode_ms * 1000;
			// bbr->probe_rtt_round_done = 0;
			// bbr->next_rtt_delivered = tp->delivered;
		} else if (bbr->probe_rtt_done_stamp_us) {
			/* Check if BBR_PROBE_RTT mode is timed out. If so, change the mode. */
			if (bbr->probe_rtt_done_stamp_us &&
				(bbr->probe_rtt_done_stamp_us - monotonic_current_time_us()) >= 0) {
				bbr->min_rtt_stamp = monotonic_current_time_us();
				bbr->probe_rtt_done_stamp_us = 0;

				// bbr->restore_cwnd = 1;  /* snap to prior_cwnd */
				state->cwnd = MAX(1, bbr->prior_cwnd);
				bbr_reset_mode(bbr);
			}
		}
	}
}

/* Find target cwnd. Right-size the cwnd based on min RTT and the
 * estimated bottleneck bandwidth:
 *
 * cwnd = bw * min_rtt * gain = BDP * gain
 *
 * The key factor, gain, controls the amount of queue. While a small gain
 * builds a smaller queue, it becomes more vulnerable to noise in RTT
 * measurements (e.g., delayed ACKs or other ACK compression effects). This
 * noise may cause BBR to under-estimate the rate.
 *
 * To achieve full performance in high-speed paths, we budget enough cwnd to
 * fit full-sized skbs in-flight on both end hosts to fully utilize the path:
 *   - one skb in sending host Qdisc,
 *   - one skb in sending host TSO/GSO engine
 *   - one skb being received by receiver host LRO/GRO/delayed-ACK engine
 * Don't worry, at low rates (bbr_min_tso_rate) this won't bloat cwnd because
 * in such cases tso_segs_goal is 1. The minimum cwnd is 4 packets,
 * which allows 2 outstanding 2-packet sequences, to try to keep pipe
 * full even with ACK-every-other-packet delayed ACKs.
 */
static uint32_t bbr_target_cwnd(ctcp_bbr_t* bbr)
{
	uint32_t cwnd;
	uint32_t bw = bbr_max_bw(bbr);
	uint64_t bdp;

	bdp = (uint64_t)bw * bbr->min_rtt_us; // This is packet-number-wise.

	/* Apply a gain to the given value, then remove the BW_SCALE shift. */
	cwnd = (((bdp * bbr->cwnd_gain) >> BBR_SCALE) + BW_UNIT - 1) / BW_UNIT;

	return cwnd;
}

/* Slow-start up toward target cwnd (if bw estimate is growing, or packet loss
 * has drawn us down below target), or snap down to target if we're above it.
 */
static void bbr_set_cwnd(ctcp_state_t* state, ctcp_bbr_t* bbr)
{
	uint32_t cwnd = 0, target_cwnd = 0;

	/* If we're below target cwnd, slow start cwnd toward target cwnd. */
	target_cwnd = bbr_target_cwnd(bbr);
	if (bbr_full_bw_reached(bbr))  /* only cut cwnd if we filled the pipe */
		cwnd = MIN(cwnd + 1, target_cwnd);
	else if (cwnd < target_cwnd || bbr->rtt_cnt < CTCP_INITIAL_CWND)
		cwnd = cwnd + 1; // slow start
	cwnd = MAX(cwnd, bbr_cwnd_min_target);

	state->cwnd = MIN(cwnd, snd_cwnd_clamp);	/* apply global cap. snd_cwnd_clamp is upper bound of cwnd. */
	if (bbr->mode == BBR_PROBE_RTT)  /* drain queue, refresh min_rtt. In BBR_PROBE_RTT mode, use min cwnd. */
		state->cwnd = MIN(state->cwnd, bbr_cwnd_min_target);
}

static void bbr_update_model(ctcp_state_t* state, ctcp_bbr_t* bbr, ctcp_transmission_info_t* trans_info){
	bbr_update_bw(bbr, trans_info);
	bbr_update_cycle_phase(state, bbr);
	bbr_check_full_bw_reached(state, bbr, trans_info->rs);
	bbr_check_drain(state, bbr, trans_info->rs);
	bbr_update_min_rtt(state, bbr, trans_info);
}

/* Return rate in bytes per second, optionally with a gain.
 * The order here is chosen carefully to avoid overflow of u64. This should
 * work for input rates of up to 2.9Tbit/sec and gain of 2.89x.
 * rate(pkts<<BW_SCALE/us) => bytes/sec
 */
static uint64_t bbr_rate_bytes_per_sec(uint64_t rate, int gain)
{
	rate *= gain;
	rate >>= BBR_SCALE;
    rate *= USEC_PER_SEC;
	rate >>= BW_SCALE;
    return rate * MAX_SEG_DATA_SIZE; // rate is packet-number-wise, so multiply MAX_SEG_DATA_SIZE bytes.
}

/* Pace using current bw estimate and a gain factor. */
static void bbr_set_pacing_rate(ctcp_state_t* state, ctcp_bbr_t* bbr)
{
	uint64_t rate = bbr_max_bw(bbr); // packet-number-wise

	rate = bbr_rate_bytes_per_sec(rate, bbr->pacing_gain); // convert bw's format rate(pkts<<BW_SCALE/us) => bytes/sec

	// Set pacing rate and gap btw pkts by pacing rate
	// if (bbr->mode != BBR_STARTUP || rate > state->pacing_rate){
	if(rate){
		state->pacing_rate = rate;
		state->pacing_gap_us = MAX(10, ((uint64_t)(MAX_SEG_DATA_SIZE)) * USEC_PER_SEC / rate);
	}else{
		//rate==10
		state->pacing_gap_us = 0;
	}
	// }
}

static void bbr_on_send(ctcp_state_t* state, ctcp_transmission_info_t* trans_info, ctcp_bbr_t* bbr){
	trans_info->rs = malloc(sizeof(ctcp_rs_t));
	
	trans_info->rs->delivered = bbr->delivered_pkts_num;
	trans_info->rs->prior_mstamp = bbr->prior_delivered_time_us;
	trans_info->rs->is_app_limited = (bbr->app_limited_until > 0);

	/* Log timestamp, BDP to bdp.txt */
	long _timestamp = current_time();
	uint64_t _bdp = bdp_in_bytes(bbr, BBR_UNIT);
	
	_ctcp_bbr_log_data(_timestamp, _bdp);
}

/**
 * Similar to bbr_main in ref: https://patchwork.ozlabs.org/project/netdev/patch/1474051743-13311-15-git-send-email-ncardwell@google.com/
 * Update bbr model(max BW, min RTT, mode, cycle phase), pacing rate, cwnd.
 */
static void bbr_on_ack(ctcp_state_t* state, ctcp_transmission_info_t* trans_info) {
    ctcp_bbr_t* bbr = state->bbr_model->bbr_object;
    if (trans_info->rs) {
		bbr->rtt_cnt += 1; // reached next rtt because we received ack.
        bbr_update_model(state, bbr, trans_info);
		bbr_set_pacing_rate(state, bbr);
        bbr_set_cwnd(state, bbr);
        free(trans_info->rs);
    }
    _log_info( "[on_ack] mode: %s, bdp: %lu, cwnd: %u, pacing_rate %lu, bw: %u, min_rtt: %u us\n\n", 
            convert_bbr_mode_to_str(bbr->mode), bdp_in_bytes(bbr, BBR_UNIT),  state->cwnd, state->pacing_rate, bbr_max_bw(bbr), bbr->min_rtt_us);
}

static void ctcp_bbr_init(ctcp_state_t* state, ctcp_bbr_t* bbr) {
    uint64_t now = monotonic_current_time_us();
    
    /*---*/
	bbr->delivered_pkts_num = 0;
	bbr->prior_delivered_time_us = now;
	bbr->app_limited_until = 0;
    bbr->prior_cwnd = state->cwnd;
	// bbr->tso_segs_goal = 0;	 /* default segs per skb until first ACK */
	bbr->rtt_cnt = 0;
	// bbr->next_rtt_delivered = 0;
	// bbr->prev_ca_state = TCP_CA_Open;
	// bbr->packet_conservation = 0;

	bbr->probe_rtt_done_stamp_us = 0;
	// bbr->probe_rtt_round_done = 0;
	// bbr->min_rtt_us = tcp_min_rtt(tp);
    bbr->min_rtt_us = state->config.rt_timeout * 1000; /* 10?? min RTT in min_rtt_win_sec window. rt_timeout: 200(Retransmission interval in milliseconds.)*/
	bbr->min_rtt_stamp = now;

	minmax_reset(&bbr->bw, bbr->rtt_cnt, 0, CTCP_BBR_WINDOW_SIZE_RTTS);  /* init max bw to 0 */

	/* Initialize pacing rate to: high_gain * init_cwnd / RTT. */
	// bw = state->cwnd * BW_UNIT;
	// do_div(bw, (tp->srtt_us >> 3) ? : USEC_PER_MSEC);
	// sk->sk_pacing_rate = 0;		/* force an update of sk_pacing_rate */
	// bbr_set_pacing_rate(sk, bw, bbr_high_gain); // I do this in ctcp_init of ctcp.c

	// bbr->restore_cwnd = 0;
	// bbr->round_start = 0;
	// bbr->idle_restart = 0;
	bbr->full_bw = 0;
	bbr->full_bw_cnt = 0;
	bbr->cycle_mstamp = now;
	bbr->cycle_idx = 0;
	// bbr_reset_lt_bw_sampling(sk);
	bbr_reset_startup_mode(bbr);
}

ctcp_bbr_model_t* ctcp_bbr_create_model(ctcp_state_t* state) {
	fprintf(stderr, "TEST state->tx_in_flight_bytes: %d\n", state->tx_in_flight_bytes); // XXX
    ctcp_bbr_model_t* bbr_model = (ctcp_bbr_model_t*)malloc(sizeof(ctcp_bbr_model_t));
    ctcp_bbr_t* bbr = (ctcp_bbr_t*)malloc(sizeof(ctcp_bbr_t));
    ctcp_bbr_init(state, bbr);
    bbr_model->bbr_object = bbr;
    bbr_model->on_send = bbr_on_send;
	bbr_model->on_ack = bbr_on_ack;
    // bbr_model->destory_bbr_model = bbr_destroy;
    
    return bbr_model;
}