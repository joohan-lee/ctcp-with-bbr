#ifndef CTCP_BBR_H
#define CTCP_BBR_H

#define CYCLE_LEN	8	/* number of phases in a pacing gain cycle */
#define CTCP_BBR_WINDOW_SIZE_RTTS (CYCLE_LEN + 2) /* win len of bw filter (in rounds) */

#include "ctcp.h"
#include "ctcp_bbr_minmax.h"

/* Scale factor for rate in pkt/uSec unit to avoid truncation in bandwidth
 * estimation. The rate unit ~= (1500 bytes / 1 usec / 2^24) ~= 715 bps.
 * This handles bandwidths from 0.06pps (715bps) to 256Mpps (3Tbps) in a u32.
 * Since the minimum window is >=4 packets, the lower bound isn't
 * an issue. The upper bound isn't an issue with existing technologies.
 */
#define BW_SCALE 24
#define BW_UNIT (1 << BW_SCALE)

#define BBR_SCALE 8	/* scaling factor for fractions in BBR (e.g. gains) */
#define BBR_UNIT (1 << BBR_SCALE)

/* BBR has the following modes for deciding how fast to send: */
typedef enum bbr_mode {
	BBR_STARTUP,	/* ramp up sending rate rapidly to fill pipe */
	BBR_DRAIN,	/* drain any queue created during startup */
	BBR_PROBE_BW,	/* discover, share bw: pace around estimated bw */
	BBR_PROBE_RTT,	/* cut cwnd to min to probe min_rtt */
} ctcp_bbr_mode_t;

/* XXX */
static inline
const char* convert_bbr_mode_to_str(ctcp_bbr_mode_t mode) {
    switch (mode)
        {
        case BBR_STARTUP:
            return "BBR_STARTUP";
        case BBR_DRAIN:
            return "BBR_DRAIN";
        case BBR_PROBE_BW:
            return "BBR_PROBE_BW";
        case BBR_PROBE_RTT:
            return "BBR_PROBE_RTT";
        default:
            return "ERROR-WRONG MODE";
    }
}

/* BBR congestion control block */
typedef struct {
    // TODO: Make it slim for ctcp version.
    uint32_t	min_rtt_us;	        /* min RTT in min_rtt_win_sec window */
    uint32_t delivered_pkts_num;
    uint64_t prior_delivered_time_us;
    int32_t app_limited_until;
    uint32_t	min_rtt_stamp;	        /* timestamp of min_rtt_us */
    uint32_t	probe_rtt_done_stamp_us;   /* end time for BBR_PROBE_RTT mode */
    ctcp_minmax_t bw;	/* Max recent delivery rate in pkts/uS << 24 */
    uint32_t	rtt_cnt;	    /* count of packet-timed rounds elapsed */
    // uint32_t     next_rtt_delivered; /* scb->tx.delivered at end of round */
    uint64_t cycle_mstamp;  /* time of this cycle phase start */
    uint32_t     mode:3;		     /* current bbr_mode in state machine */
    // 	prev_ca_state:3,     /* CA state on previous ACK */
    // 	packet_conservation:1,  /* use packet conservation? */
    // 	restore_cwnd:1,	     /* decided to revert cwnd to old value */
    // 	round_start:1,	     /* start of packet-timed tx->ack round? */
    // 	tso_segs_goal:7,     /* segments we want in each skb we send */
    // 	idle_restart:1,	     /* restarting after idle? */
    // 	probe_rtt_round_done:1,  /* a BBR_PROBE_RTT round at 4 pkts? */
    // 	unused:5,
    // 	lt_is_sampling:1,    /* taking long-term ("LT") samples now? */
    // 	lt_rtt_cnt:7,	     /* round trips in long-term interval */
    // 	lt_use_bw:1;	     /* use lt_bw as our bw estimate? */
    // uint32_t	lt_bw;		     /* LT est delivery rate in pkts/uS << 24 */
    // uint32_t	lt_last_delivered;   /* LT intvl start: tp->delivered */
    // uint32_t	lt_last_stamp;	     /* LT intvl start: tp->delivered_mstamp */
    // uint32_t	lt_last_lost;	     /* LT intvl start: tp->lost */
    uint32_t	pacing_gain:10,	/* current gain for setting pacing rate */
    	cwnd_gain:10,	/* current gain for setting cwnd */
    	full_bw_cnt:3,	/* number of rounds without large bw gains */
    	cycle_idx:3,	/* current index in pacing_gain cycle array */
    	unused_b:6;
    uint32_t	prior_cwnd;	/* prior cwnd upon entering loss recovery */
    uint32_t	full_bw;	/* recent bw, to estimate if pipe is full */
} ctcp_bbr_t;

// static int bbr_bw_rtts	= CYCLE_LEN + 2; /* win len of bw filter (in rounds) */
static const uint32_t bbr_min_rtt_win_sec = 10;	 /* min RTT filter window (in sec) */
static const uint32_t bbr_probe_rtt_mode_ms = 200;	 /* min ms at cwnd=4 in BBR_PROBE_RTT */
// static int bbr_min_tso_rate	= 1200000;  /* skip TSO below here (bits/sec) */

/* We use a high_gain value chosen to allow a smoothly increasing pacing rate
 * that will double each RTT and send the same number of packets per RTT that
 * an un-paced, slow-starting Reno or CUBIC flow would.
 */
static const int bbr_high_gain  = BBR_UNIT * 2885 / 1000 + 1;	/* 2/ln(2) */
static const int bbr_drain_gain = BBR_UNIT * 1000 / 2885;	/* 1/high_gain */
static const int bbr_cwnd_gain  = BBR_UNIT * 2;	/* gain for steady-state cwnd */
/* The pacing_gain values for the PROBE_BW gain cycle: */
static const int bbr_pacing_gain[] = { BBR_UNIT * 5 / 4, BBR_UNIT * 3 / 4,
				 BBR_UNIT, BBR_UNIT, BBR_UNIT,
				 BBR_UNIT, BBR_UNIT, BBR_UNIT };
static const uint32_t bbr_cycle_rand = 7;  /* randomize gain cycling phase over N phases */

static const uint32_t	snd_cwnd_clamp = 0xffffffff; /* Do not allow cwnd to grow above this */

/* Try to keep at least this many packets in flight, if things go smoothly. For
 * smooth functioning, a sliding window protocol ACKing every other packet
 * needs at least 4 packets in flight.
 */
static const uint32_t bbr_cwnd_min_target	= 4;

#define USEC_PER_SEC 1000000

/* A rate sample measures the number of (original/retransmitted) data
 * packets delivered "delivered" over an interval of time "interval_us".
 * The tcp_rate.c code fills in the rate sample, and congestion
 * control modules that define a cong_control function to run at the end
 * of ACK processing can optionally chose to consult this sample when
 * setting cwnd and pacing rate.
 * A sample is invalid if "delivered" or "interval_us" is negative.
 */
struct rate_sample {
	uint64_t  prior_mstamp; /* starting timestamp for interval */
	// uint32_t  prior_delivered;	/* tp->delivered at "prior_mstamp" */
	signed int  delivered;		/* number of packets delivered over interval */
	// long interval_us;	/* time for tp->delivered to incr "delivered" */
	// u32 snd_interval_us;	/* snd interval for delivered packets */
	// u32 rcv_interval_us;	/* rcv interval for delivered packets */
	// long rtt_us;		/* RTT of last (S)ACKed packet (or -1) */
	// int  losses;		/* number of packets marked lost upon ACK */
	// u32  acked_sacked;	/* number of packets newly (S)ACKed upon ACK */
	// u32  prior_in_flight;	/* in flight before this ACK */
	uint16_t is_app_limited;	/* is sample from packet with bubble in pipe? */
	// bool is_retrans;	/* is sample from retransmission? */
	// bool is_ack_delayed;	/* is this (likely) a delayed ACK? */
};
typedef struct rate_sample ctcp_rs_t;

typedef struct ctcp_state ctcp_state_t;
typedef struct ctcp_transmission_info ctcp_transmission_info_t;
struct ctcp_bbr_model {
    void* bbr_object;
    void (*on_send)(ctcp_state_t*, ctcp_transmission_info_t*, void*);
    void (*on_ack)(ctcp_state_t*, ctcp_transmission_info_t*);
    // void (*destory_bbr_model)(void*);
};
typedef struct ctcp_bbr_model ctcp_bbr_model_t;

ctcp_bbr_model_t* ctcp_bbr_create_model(ctcp_state_t* state);

#endif