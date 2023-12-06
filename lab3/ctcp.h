/******************************************************************************
 * ctcp.h
 * ------
 * Contains definitions for constants functions, and structs you will need for
 * the cTCP implementation. Implementations of the functions should be done in
 * ctcp.c.
 *
 *****************************************************************************/

#ifndef CTCP_H
#define CTCP_H

#include "ctcp_sys.h"
#include "ctcp_linked_list.h"
#include "ctcp_bbr.h"

/**
 * Maximum segment data size.
 *
 * For stop-and-wait, advertise a window of MAX_SEG_DATA_SIZE.
 * For sliding window, advertise a window of n * MAX_SEG_DATA_SIZE, where n is
 * any integer specified by the -w flag.
 *
 * The maximum segment data size is the maximum number of bytes of DATA that can
 * be sent or received in a single cTCP segment. It does not include the
 * headers. A cTCP segment may be smaller than MAX_SEG_DATA_SIZE.
 *
 * A sliding window of size n * MAX_SEG_DATA_SIZE may have more than n segments,
 * if not all the segments are of the full MAX_SEG_DATA_SIZE in size.
 */
#define MAX_SEG_DATA_SIZE 1440

/**
 * cTCP flags.
 *
 * These are in HOST order. Make sure to convert to network-byte order when
 * needed. Check if a segment is an ACK by doing (flags & ACK).
 */
#define SYN ntohl(TH_SYN)
#define ACK ntohl(TH_ACK)
#define FIN ntohl(TH_FIN)

/**
 * TCP Connection Termination state
 * ref for when client initiates termination: http://www.tcpipguide.com/free/t_TCPConnectionTermination-2.htm#google_vignette
 * ref for when server and client simultaneously initiates termination: http://www.tcpipguide.com/free/t_TCPConnectionTermination-4.htm
*/
enum{

  /* Connection is on established (not closing) */
  CONN_ESTABLISHED,

  /* where initiates termination*/
  FIN_WAIT_1,
  FIN_WAIT_2,

  /* where responds to termination(FIN)) */
  CLOSE_WAIT,
  LAST_ACK,

  /* Client's state (where initiates termination)*/
  CLOSING,
  TIME_WAIT,
  CLOSED
};

/**
 * Maximum Segment Lifetime.
*/
// FIN Timeout is generally 60 seconds(=60000ms) in most implementations of TCP in linux.
// You can check the value by printing 'cat /proc/sys/net/ipv4/tcp_fin_timeout'
#define FIN_TIMEOUT 60000
#define MSL (2*FIN_TIMEOUT)

/**
 * cTCP configuration struct.
 *
 * Use these values to adjust your cTCP implementation accordingly.
 */
typedef struct {
  uint16_t recv_window;    /* Receive window size (in multiples of
                              MAX_SEG_DATA_SIZE) of THIS host. For Lab 1 this
                              value will be 1 * MAX_SEG_DATA_SIZE */
  uint16_t send_window;    /* Send window size (a.k.a. receive window size of
                              the OTHER host). For Lab 1 this value
                              will be 1 * MAX_SEG_DATA_SIZE */
  int timer;               /* How often ctcp_timer() is called, in ms. =TIME_INTERVAL */
  int rt_timeout;          /* Retransmission timeout, in ms. =RT_INTERVAL */
} ctcp_config_t;

/**
 * Connection state.
 *
 * Stores per-connection information such as the current sequence number,
 * unacknowledged segments, etc.
 *
 * The definition can be found in ctcp.c. You should add to this to store other
 * fields you might need.
 */
struct ctcp_state;
typedef struct ctcp_state ctcp_state_t;


////////////////////////////////// YOUR CODE //////////////////////////////////

/**
 * Initialize state associated with a connection. This is called by the library
 * when a new connection is made. You should set up any fields you need to keep
 * track of segments being sent to and received from this connection.
 *
 * conn: Connection object associated with this connection. Is NULL if a
 *       connection to the server cannot be established. In this case, NULL
 *       should be returned. Memory management is handled by the starter code in
 *       ctcp_destroy().
 * cfg: cTCP configuration struct. Contains details about the window size,
 *      timeout interval, and timer frequency. Use the values in this struct
 *      (defined in ctcp.h) to adjust your timeouts, window sizes, etc.
 *      accordingly. You MUST free this struct when you are done with it!
 *
 * returns: Returns the state associated with this connection. If a connection
 *          cannot be established, returns NULL.
 */
ctcp_state_t *ctcp_init(conn_t *conn, ctcp_config_t *cfg);

/**
 * Destroys connection state for a connection. You should call this when all of
 * the following hold:
 *    - You have received a FIN from the other side.
 *    - You have read an EOF or error from your input (conn_input returned -1)
 *      and have sent a FIN to the other side.
 *    - All sent segments (including the FIN) have been acknowledged.
 *    - All received segments have been outputted.
 * Or:
 *    - The other side is unresponsive (after retransmitting the same segment 5
 *      times and still receiving no response).
 *
 * Free up any memory allocated for this connection.
 *
 * state: Connection state to destroy.
 */
void ctcp_destroy(ctcp_state_t *state);

/**
 * This is called if there is input to be read. To read the input, call
 * conn_input() with a buffer of the correct size. If no data is available,
 * conn_input() will return 0. ctcp_read() is called automatically by the
 * library when there is more input to read (so you never need to call it
 * yourself).
 *
 * conn_input() will return -1 when it reads an EOF. You should send a FIN to
 * the other side when this occurs. Then, you will need to destroy any
 * connection state once the conditions are satisfied (see ctcp_destroy()).
 *
 * Create a segment from the input and send it to the connection associated with
 * the passed in state (by calling conn_send()).
 *
 * state: State for the connection associated with this input. Get the
 *        associated connection object with state->conn.
 */
void ctcp_read(ctcp_state_t *state);

/**
 * This is called by the library when a segment is received. You should send
 * ACKs accordingly and output the segment's data to STDOUT if there is data.
 * To output, call on ctcp_output(), which you also must implement.
 *
 * The received segment MUST BE FREED after you are done with it.
 *
 * If you receive a FIN segment, you should output an EOF by calling
 * conn_output() with a length of 0. Then, you will need to destroy any
 * connection state once the conditions are satisfied (see ctcp_destroy()).
 *
 * state: Associated connection state.
 * segment: Segment received from the server. You should free this when you are
 *          done with it.
 * len: Length of the segment (including the headers). There might be extra
 *      padding so the received length might be larger than the length field in
 *      the segment header. The segment may have also been truncated (len is
 *      smaller than the length of the segment).
 */
void ctcp_receive(ctcp_state_t *state, ctcp_segment_t *segment, size_t len);

/**
 * Outputs cTCP segments associated with the given ctcp_state_t object. This
 * should be called by ctcp_receive() if a segment is ready to be outputted.
 *
 * Before outputting a segment, you will need to call conn_bufspace() to see
 * how many bytes can be outputted to STDOUT. If there is no room, ctcp_output()
 * will automatically be called by the library when there is. Call conn_output()
 * in order to actually output the segment. If you call conn_output() with more
 * data than conn_bufspace() says is available, not all of it may be written.
 *
 * You should flow control the sender by not acknowledging segments if there
 * is no buffer space available for conn_output().
 *
 * state: Associated connection state with the output.
 */
void ctcp_output(ctcp_state_t *state);

/**
 * Called periodically at specified rate (see the timer field in the
 * ctcp_config_t struct).
 *
 * You can use this timer to inspect segments and retransmit ones that have not
 * been acknowledged. Do not retransmit every segment every time the timer is
 * fired! A segment should only be retransmitted rt_timeout milliseconds after
 * it was last sent (also defined in the ctcp_config_t struct).
 *
 * After 5 retransmission attempts (so a total of 6 times) for a segment, you
 * should assume the other end of the connection is unresponsive and tear down
 * the connection (via a call to ctcp_destroy()).
 *
 * Note that this is called BEFORE ctcp_init() so state_list might be NULL.
 */
void ctcp_timer();

/* Send a segment at pacing rate.
  Called at doloop() in ctcp_sys_internals.c
  Timer for pacing.
  Send a segment at every interval if there are segments in tx queue 
  */
void ctcp_pacing_timer();

/**
 * Connection state.
 *
 * Stores per-connection information such as the current sequence number,
 * unacknowledged packets, etc.
 *
 * You should add to this to store other fields you might need.
 */
typedef struct linked_list linked_list_t;
typedef struct ctcp_bbr_model ctcp_bbr_model_t;
struct ctcp_state {
  struct ctcp_state *next;  /* Next in linked list */
  struct ctcp_state **prev; /* Prev in linked list */

  conn_t *conn;             /* Connection object -- needed in order to figure
                               out destination when sending */
  linked_list_t *segments;  /* Linked list of segments sent to this connection.
                               It may be useful to have multiple linked lists
                               for unacknowledged segments, segments that
                               haven't been sent, etc. Lab 1 uses the
                               stop-and-wait protocol and therefore does not
                               necessarily need a linked list. You may remove
                               this if this is the case for you */
                            /* Consider this segments linked list as sender(transmission) buffer. */
  
  linked_list_t *waiting_segments; /* Linked list of segments that was failed to send
                                      because receiver buffer is not available.
                                    */
  linked_list_t *received_segments; /* Receiver buffer. Once segment is received,
                                      it is stored in received buffer(received_segments).
                                      If STDOUT buffer is available, it moves to application
                                      layer and should be removed from received buffer.
                                      Note that data can be moved to application layer
                                      only if it is in order.
                                      This has received segments in its object of each node.
                                      i.e. stores segments that are received, but not yet 
                                      processed(output).*/

  uint32_t curr_seqno; /* current sequence number of in-flight segment */
  uint32_t curr_ackno; /* Last Acked number for the other side. */
  uint32_t rx_next_output_seqno; /* Sequence number to output next time(=seqno waiting for output). */

  uint32_t tx_in_flight_bytes; /* Outstanding bytes(sent but not acknowledged) = inflight bytes. 
                                  When this host is Tx.
                                */
  uint32_t rx_waiting_bytes; /* size of data that was received but are waiting for being outputted. 
                                Since they are not outputted, they are not acked yet.
                               */

  ctcp_config_t config; /* cTCP configuration struct. */

  int32_t termination_state; /* TCP Connection Termination state */
  uint32_t time_wait_in_ms; /* The host waits for a period of time equal to double 
                        the maximum segment life (MSL) time, 
                        to ensure the ACK it sent was received. 
                        Here, MSL time is same as timer value in ctcp_config_t(=40ms=TIME_INTERVAL). */

  ctcp_bbr_model_t* bbr_model; /* This contains ctcp_bbr_t and functions for bbr strategy such as on_ack, on_send. */
  uint32_t cwnd; /* congestion control window size */

  /* pacing */
  uint64_t pacing_rate;        /* bandwidth (byte/sec) */
  uint64_t pacing_gap_us;      /* This means gap(interval) between packets.
                                 It depends on bbr mode(,so pacing gain). */
  struct timespec pacing_last_timeout; /* This is for pacing timer. */
};

/* LOG */
#define _log_info(...){ \
  fprintf(stderr, "[_INFO] ");\
  fprintf(stderr, __VA_ARGS__);\
}

#define _log_debug(...){ \
  fprintf(stderr, "[DEBUG] ");\
  fprintf(stderr, __VA_ARGS__);\
}

/**
 * Transmitter should know about transmitted time of each segment to retransmit in the future.
 *
*/
typedef struct rate_sample ctcp_rs_t;

struct ctcp_transmission_info{
  uint32_t time_elapsed; /* time elapsed from last transmission of this segment. */
  uint32_t num_of_transmission; /* The number of transmissions of this segment. (not only retransmission) */
  uint64_t send_time_us;  /* time sent in usec. */
  uint64_t ack_time_us;  /* time acked in usec. */
  ctcp_rs_t* rs;
  ctcp_segment_t segment;
};
typedef struct ctcp_transmission_info ctcp_transmission_info_t;

/* Define constant */
#define HDR_CTCP_SEGMENT sizeof(ctcp_segment_t)

ctcp_transmission_info_t* create_segment(ctcp_state_t *state, uint8_t flags, size_t data_sz, uint8_t data[]);
int is_cksum_valid(ctcp_segment_t* segment, size_t len);
int is_ack(ctcp_state_t* state, ctcp_segment_t* segment);
void send_segment(ctcp_state_t* state, ctcp_transmission_info_t* trans_info, size_t len);
int is_new_data_segment(ctcp_state_t *state, ctcp_segment_t *rcvd_segment);
void send_only_ack(ctcp_state_t* state, ctcp_segment_t* rcvd_segment);

#define MAX(x, y) ( x > y ? x:y)
#define MIN(x, y) ( x < y ? x:y)

#define CTCP_INITIAL_CWND 10

#endif /* CTCP_H */