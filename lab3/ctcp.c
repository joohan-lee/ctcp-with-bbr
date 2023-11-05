/******************************************************************************
 * ctcp.c
 * ------
 * Implementation of cTCP done here. This is the only file you need to change.
 * Look at the following files for references and useful functions:
 *   - ctcp.h: Headers for this file.
 *   - ctcp_iinked_list.h: Linked list functions for managing a linked list.
 *   - ctcp_sys.h: Connection-related structs and functions, cTCP segment
 *                 definition.
 *   - ctcp_utils.h: Checksum computation, getting the current time.
 *
 *****************************************************************************/

#include "ctcp.h"
#include "ctcp_linked_list.h"
#include "ctcp_sys.h"
#include "ctcp_utils.h"

/**
 * Connection state.
 *
 * Stores per-connection information such as the current sequence number,
 * unacknowledged packets, etc.
 *
 * You should add to this to store other fields you might need.
 */
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
  
};

/**
 * Linked list of connection states. Go through this in ctcp_timer() to
 * resubmit segments and tear down connections.
 */
static ctcp_state_t *state_list;

/* FIXME: Feel free to add as many helper functions as needed. Don't repeat
          code! Helper functions make the code clearer and cleaner. */


ctcp_state_t *ctcp_init(conn_t *conn, ctcp_config_t *cfg) {
  /* Connection could not be established. */
  if (conn == NULL) {
    return NULL;
  }

  /* Established a connection. Create a new state and update the linked list
     of connection states. */
  ctcp_state_t *state = calloc(sizeof(ctcp_state_t), 1);
  state->next = state_list;
  state->prev = &state_list;
  if (state_list)
    state_list->prev = &state->next;
  state_list = state;

  /* Set fields. */
  state->conn = conn;
  /* FIXME: Do any other initialization here. */
  state->segments = ll_create();
  state->waiting_segments = ll_create();
  state->received_segments = ll_create();

  state->curr_seqno=1;
  state->curr_ackno=1;
  state->rx_next_output_seqno=1;
  
  state->tx_in_flight_bytes=0;
  state->rx_waiting_bytes=0;

  state->termination_state=CONN_ESTABLISHED;
  state->time_wait_in_ms=0;

  state->config = *cfg;

  return state;
}

void ctcp_destroy(ctcp_state_t *state) {
  /* Update linked list. */
  if (state->next)
    state->next->prev = state->prev;

  *state->prev = state->next;
  conn_remove(state->conn);

  /* FIXME: Do any other cleanup here. */
  // Free up the memory taken up by the objects contained within the nodes 
  // because ll_destroy DOES NOT free up them.
  ll_free_objects(state->segments);
  ll_destroy(state->segments);
  ll_free_objects(state->waiting_segments);
  ll_destroy(state->waiting_segments);
  ll_free_objects(state->received_segments);
  ll_destroy(state->received_segments);

  free(state);
  end_client();
}

void ctcp_read(ctcp_state_t *state) {
  uint8_t stdin_buf[MAX_SEG_DATA_SIZE];
  memset(stdin_buf, 0, MAX_SEG_DATA_SIZE); // Clean memory
  int stdin_data_sz = 0;

  /* Read STDIN into buf until no data is available */
  if((stdin_data_sz = conn_input(state->conn, stdin_buf, MAX_SEG_DATA_SIZE)) > 0){
    /* Create a single segment(Segment size is up to 1 * MAX_SEG_DATA_SIZE) for lab1 */
    // ctcp_transmission_info_t *trans_info = create_segment(state->curr_seqno, state->curr_ackno, TH_ACK, stdin_data_sz, stdin_buf);
    ctcp_transmission_info_t *trans_info = create_segment(state, TH_ACK, stdin_data_sz, stdin_buf);
    _log_info("[TX]Segment is created. ");
    ctcp_segment_t *segment = &(trans_info->segment);
    print_hdr_ctcp(segment);
    send_segment(state, trans_info, stdin_data_sz);
  }

  if(stdin_data_sz==-1 /* EOF */
    && ll_length(state->segments) ==0 /* No in-flight segments */
    && ll_length(state->waiting_segments) == 0 /* No pending segments to be sent */
  ){
    /* Termination */
    _log_info("[tcp termination]EOF was entered. Termination initiated.\n");
    const int FIN_SEGMENT_DATA_SIZE = 1;
    if(state->termination_state == CONN_ESTABLISHED){
      /* The application of this host using TCP signals that the connection is no longer needed. 
      This host's TCP sends a segment with the FIN bit set to request that the connection be closed. 
      ESTABLISHED -> FIN_WAIT_1*/
      uint8_t dummy = 0; /* dummy data for FIN. FIN is considered as 1-byte segment. */
      // ctcp_transmission_info_t *fin_trans_info = (ctcp_transmission_info_t*)create_segment(state->curr_seqno, state->curr_ackno, TH_FIN, FIN_SEGMENT_DATA_SIZE, &dummy);
      ctcp_transmission_info_t *fin_trans_info = (ctcp_transmission_info_t*)create_segment(state, TH_FIN, FIN_SEGMENT_DATA_SIZE, &dummy);
      _log_info("[tcp termination]Client state transitions from CONN_ESTABLISHED to FIN_WATI1.\n");
      send_segment(state, fin_trans_info, FIN_SEGMENT_DATA_SIZE);
      state->termination_state = FIN_WAIT_1;
    }
    else if(state->termination_state == CLOSE_WAIT){
      /* This host's TCP receives notice from the local application that it is done. 
      This host sends its FIN to the other host. 
      CLOSE_WAIT -> LAST_ACK */
      uint8_t dummy = 0; /* dummy data for FIN. FIN is considered as 1-byte segment. */
      // ctcp_transmission_info_t *fin_trans_info = (ctcp_transmission_info_t*)create_segment(state->curr_seqno, state->curr_ackno, TH_FIN, FIN_SEGMENT_DATA_SIZE, &dummy);
      ctcp_transmission_info_t *fin_trans_info = (ctcp_transmission_info_t*)create_segment(state, TH_FIN, FIN_SEGMENT_DATA_SIZE, &dummy);
      _log_info("[tcp termination]Server state transitions from CONN_ESTABLISHED to LAST_ACK.\n");
      send_segment(state, fin_trans_info, FIN_SEGMENT_DATA_SIZE);
      state->termination_state = LAST_ACK;
    }
  }
}

void ctcp_receive(ctcp_state_t *state, ctcp_segment_t *segment, size_t len) {
  assert(ntohs(segment->len) == len);

  _log_info("[RX]Segment is received. len: %lu ", len);
  print_hdr_ctcp(segment);

  /* Check if cksum is valid. If not, drop the packet. */
  if(!is_cksum_valid(segment, len)){
    fprintf(stderr, "[Rx] Invalid checksum. Drop the received packet.\n");
    free(segment);
    return;
  }

  int is_termination_state_transitioned = 0;

  if(state->termination_state == FIN_WAIT_1){
    /* This host, having sent a FIN, is waiting for it to both be acknowledged and for the other host 
    to send its own FIN.
    In this state this host(either client or server) can still receive data from the other 
    but will no longer accept data from its local application to be sent to the other host. */
    
    /* If this host already sent FIN and receives FIN from the other before receiving its own ACK,
      transition to CLOSING and sends ACK. 
      FIN_WAIT_1 -> CLOSING.
     */
    if((segment->flags & TH_FIN) && is_new_data_segment(state, segment)){
      state->termination_state = CLOSING;
      _log_info("FIN_WAIT_1 -> CLOSING\n");
      // Send ack
      send_only_ack(state, segment);
      is_termination_state_transitioned = 1;
    }else{
      /* Waiting for ACK for its FIN. */
      if(ll_remove_acked_segments(state->segments, segment->ackno)){
        /* If this host receives the ACK for its FIN, transition to FIN_WAIT_2 
        FIN_WAIT_1 -> FIN_WAIT_2*/
        state->termination_state = FIN_WAIT_2;
        _log_info("FIN_WAIT_1 -> FIN_WAIT_2\n");
        is_termination_state_transitioned = 1;
      }
    }
  }else if(state->termination_state == FIN_WAIT_2){
    /* This host is waiting for the other's FIN.*/
    /* If this host receives the other's FIN, sends back an ACK and transition to TIME_WAIT. 
    FIN_WAIT_2 -> TIME_WAIT*/
    if((segment->flags & TH_FIN) && is_new_data_segment(state, segment)){
      send_only_ack(state, segment);
      _log_info("FIN_WAIT_2 -> TIME_WAIT\n");
      state->termination_state = TIME_WAIT;
      is_termination_state_transitioned = 1;
    }
  }else if(state->termination_state == CLOSING){
    /* If this host receives the ACK for its FIN, transition to TIME_WAIT no matter it is client or server. 
    CLOSING -> TIME_WAIT */
    if(ll_remove_acked_segments(state->segments, segment->ackno)){
      _log_info("CLOSING -> TIME_WAIT\n");
      state->termination_state = TIME_WAIT;
      is_termination_state_transitioned = 1;
    }
  }else if(state->termination_state == LAST_ACK){
    /* This host is waiting for an ACK for the FIN it sent. 
    If this host receives the ACK to its FIN, closes the connection.
    LAST_ACK -> CLOSED. DESTROY THE CONNECTION. */
    if(ll_remove_acked_segments(state->segments, segment->ackno)){
      _log_info("LAST_ACK -> CLOSED\n");
      state->termination_state = CLOSED;
      ctcp_destroy(state);
      is_termination_state_transitioned = 1;
    }
  }
  if(is_termination_state_transitioned){
    free(segment);
    return;
  }

  /* Upon receipt of FIN while ESTABLISHED, FIN responder starts Termination Procedure (CLOSE_WAIT -> LAST_ACK -> CLOSED) 
  Immediately send ACK for received FIN. FYI, FIN segment is 1-byte(=data size) segment.
  */
  if((state->termination_state == CONN_ESTABLISHED) && (segment->flags & TH_FIN)){
    _log_info("[RX] Received FIN segment. Termination initiated.\n");
    send_only_ack(state, segment);
    state->termination_state = CLOSE_WAIT;
    free(segment);
    return;
  }
  
  /* If received segment is ACK, update ackno. */
  if(is_ack(state, segment)){
    _log_info("ACK segment received.\n");
    /* Remove all sent segments that has acked from transmission buffer(='state->segments' linked list)*/
    uint32_t size_of_acked_segments = ll_remove_acked_segments(state->segments, segment->ackno);
    _log_info("%d bytes of segment data was acked. tx_in_flight_bytes %d->", size_of_acked_segments,state->tx_in_flight_bytes);
    state->tx_in_flight_bytes -= size_of_acked_segments;
    fprintf(stderr,"%d.\n", state->tx_in_flight_bytes);

    // Send pending segments(= segments that wasn't sent due to lack of send_window(other's advertised buf size))
    
    while(ll_length(state->waiting_segments)){
      ll_node_t *curr_node = ll_front(state->waiting_segments);
      // Before sending, first check if receiver's buffer is available.(Flow control)
      ctcp_transmission_info_t *trans_info = (ctcp_transmission_info_t*)(curr_node->object);
      ctcp_segment_t *pending_segment = &(trans_info->segment);
      const uint32_t data_sz = ntohs(pending_segment->len) - HDR_CTCP_SEGMENT;

      if((state->tx_in_flight_bytes + data_sz) > state->config.send_window){
        // If the other side(=receiver)'s buffer is not available, stop sending.
        _log_info("[Tx] If sending %d bytes of pending data, in-flight bytes(%d) will overflow receiver's window size(%d). Stop sending.\n",
           data_sz, state->tx_in_flight_bytes, state->config.send_window);
        break;
      }
      
      // send pending segments after update up-to-date ackno and cksum.
      ctcp_transmission_info_t *curr_trans_info = (ctcp_transmission_info_t*)(ll_remove(state->waiting_segments, curr_node));
      ctcp_segment_t *segment = &(curr_trans_info->segment);
      state->tx_in_flight_bytes += (ntohs(segment->len) - HDR_CTCP_SEGMENT);
      // update segment's acknowledgement number to up-to-date ackno.
      segment->ackno = htonl(state->curr_ackno);
      segment->cksum = 0;
      segment->cksum = cksum(segment, htons(segment->len)); // update checksum since segment's ackno might be changed.
      
      int sent = conn_send(state->conn, segment, ntohs(segment->len));
      if(sent == 0){
        _log_info("[Tx] Nothing was sent.\n");
      }else if(sent==-1){
        _log_info("[Tx] Error occured while conn_send waiting segment.\n");
      }else{
        _log_info("[Tx] waiting segment were sent.\n");
        print_hdr_ctcp(segment);
      }

      // store transmitted segments to 'segments'(transmission buffer)
      curr_trans_info->num_of_transmission += 1;
      ll_add(state->segments, curr_trans_info);

    }
    free(segment);
    return;
  }
  
  /* If segment is newly received data, add it to receiver buffer in proper index.
   * Otherwise, drop it. 
   */
  if(is_new_data_segment(state, segment)){
    ll_node_t *added_node = ll_add_in_order(state->received_segments, segment);
    if(added_node){
      state->rx_waiting_bytes += (len - HDR_CTCP_SEGMENT); //Increase size of data that received but not output.
      _log_info("Current Rx buffer size waiting for output / rcvr buffer: %d/%d after added.\n", state->rx_waiting_bytes, state->config.recv_window);

    }else{
      /* If it was not added into receiver buffer, Drop the packet(segment). */
      free(segment);
    }
    
    // if this host's receiver buffer overflows, drop packets until receiver buffer is available.
    // 아래 필요??
    // while(state->rx_waiting_bytes > state->config.recv_window){
    //   // 받았지만 output을 기다리고 있는 데이터의 크기가 sender로부터 받을 수 있는 용량보다 클 때. 
    //   // 즉, receiver buffer 적절한 위치에 hole을 채워 넣은 후 receiver buffer window size보다 크면 맨 뒤(큰 seqno)부터 제거
    //   ll_node_t *last_node = ll_back(state->received_segments);
    //   ctcp_segment_t *s = (ctcp_segment_t*)(last_node->object);
    //   state->rx_waiting_bytes -= ntohl(s->len) - HDR_CTCP_SEGMENT;
    //   ctcp_segment_t *drop = (ctcp_segment_t*)ll_remove(state->received_segments, last_node); // Drop the segment.
    //   free(drop);
    // }

  }else{
    _log_info("Data was already received. Drop it.\n");
    free(segment);
  }
  
  ctcp_output(state);
}

void ctcp_output(ctcp_state_t *state) {
  
  while(ll_length(state->received_segments) > 0){
    ll_node_t *node = ll_front(state->received_segments);
    ctcp_segment_t *rcvd_segment = node->object;

    /* Output segment only if it is in-order.
     새로 받은 segment랑 이전에 output(ack)했던 segment 사이에 hole이 없을 때만 쭉 output하기.
    */
    if(rcvd_segment->seqno != htonl(state->rx_next_output_seqno)){
      _log_info("Received seqno %d, but seqno %d is waiting for output.\n", ntohl(rcvd_segment->seqno), state->rx_next_output_seqno);
      break;
    }

    // Check if STDOUT buffer is available for output.
    size_t available_bufspace = conn_bufspace(state->conn);
    size_t rcvd_data_sz = ntohs(rcvd_segment->len) - HDR_CTCP_SEGMENT;

    // If STDOUT buffer available space is enough, output data to STDOUT and send ACK.
    if(available_bufspace >= rcvd_data_sz){
      state->rx_next_output_seqno += (ntohs(rcvd_segment->len) - HDR_CTCP_SEGMENT);
      state->rx_waiting_bytes -= (ntohs(rcvd_segment->len) - HDR_CTCP_SEGMENT);
      _log_info("Output to STDOUT. Available bufspace: %lu.\n", available_bufspace);
      _log_info("state->rx_next_output_seqno: %d, state->rx_waiting_bytes: %d. after outputting.\n", state->rx_next_output_seqno, state->rx_waiting_bytes);
      assert(conn_output(state->conn, rcvd_segment->data, rcvd_data_sz));

      // Send ACK. (Can be piggyback or separate.)
      // Send ACK only.
      send_only_ack(state, rcvd_segment);

      // Remove handled node from linked list(received_segments)
      ll_remove(state->received_segments, node); // This frees the node.
      free(rcvd_segment);
    }
    else{
      _log_info("[ctcp_output] failed to output since output bufspace is not enough.\n");
      break;
    }
  }
}

void ctcp_timer() {
  if(!state_list){
    /* If NO connection state, do nothing. */
    return;
  }
  ctcp_state_t *curr_state = state_list;
  /* Go through state_list to resubmit segments and tear down connections. */
  while(curr_state){
    if(curr_state->termination_state == TIME_WAIT || curr_state->termination_state == LAST_ACK){
      /* TIME_WAIT or LAST_ACK */
      curr_state->time_wait_in_ms += curr_state->config.timer;

      if((curr_state->time_wait_in_ms > 2 * MSL) || (ll_length(curr_state->segments) == 0)){
        /* The host waits for a period of time equal to double the maximum segment life (MSL) time, 
        to ensure the ACK it sent was received.
        Terminate TCP connection if
          - Already waited for double the maximum segment life(MSL) time.
          - All sent segments were ACKed.
        TIME_WAIT -> CLOSED. */
        curr_state->termination_state = CLOSED;
        ctcp_state_t *destroy_state = curr_state;
        curr_state = curr_state->next;
        ctcp_destroy(destroy_state);
      }else{
        curr_state = curr_state->next;
      }
    }else{
      /* RETRANSMISSION */
      ll_node_t *curr_node = ll_front(curr_state->segments);
      while(curr_node){
        ctcp_transmission_info_t *trans_info = (ctcp_transmission_info_t*)curr_node->object;
        trans_info->time_elapsed += curr_state->config.timer;
        
        if(trans_info->num_of_transmission >= 6 || trans_info->time_elapsed >= 6*(curr_state->config.rt_timeout)){
          // Tear down if 6th retransmission happens.(segment can be sent up to 6 times in total.)
          _log_info("Tear down. 6th retransmission was tried to be sent.\n");
          ctcp_state_t *destroy_state = curr_state;
          curr_state = curr_state->next;
          ctcp_destroy(destroy_state);
          break;
        }else if(trans_info->time_elapsed % curr_state->config.rt_timeout == 0){
          _log_info("[RETRANSMIT] Transmit %d-th time.\n", trans_info->num_of_transmission);
          // Retransmit if it took retransmission timeout.
          ctcp_segment_t *segment = &(trans_info->segment);
          // update segment's acknowledgement number because it could change while waiting for retransmission.
          segment->ackno = htonl(curr_state->curr_ackno);
          segment->cksum = 0;
          segment->cksum = cksum(segment, htons(segment->len)); // update checksum since segment's ackno might be changed.
          int sent = conn_send(curr_state->conn, segment, ntohs(segment->len));
          if(sent == 0){
            _log_info("[Tx] Nothing was sent.\n");
          }else if(sent==-1){
            _log_info("[Tx] Error occured while conn_send for retransmission.\n");
          }
          trans_info->num_of_transmission += 1;
        }

        curr_node = curr_node->next;
      }
      curr_state = curr_state->next;
    }
  }
}

ctcp_transmission_info_t* create_segment(ctcp_state_t *state,
  uint8_t flags, size_t data_sz, uint8_t data[]){
  size_t segment_total_sz = HDR_CTCP_SEGMENT + data_sz;
  ctcp_transmission_info_t *trans_info = (ctcp_transmission_info_t *)malloc(sizeof(ctcp_transmission_info_t) + segment_total_sz);
  trans_info->time_elapsed = 0;
  trans_info->num_of_transmission = 0;

  ctcp_segment_t *segment = &(trans_info->segment);
  segment->seqno = htonl(state->curr_seqno);
  segment->ackno = htonl(state->curr_ackno);
  segment->len = htons(segment_total_sz);
  segment->flags = flags; // Network byte order
  segment->window = htons(state->config.recv_window); // Advertise the size of bytes that can be received from sender.
  segment->cksum = 0;
  memcpy(segment->data, data, data_sz);

  // Calculate cksum
  segment->cksum = cksum(segment, segment_total_sz); // cksum function returns network byte order

  return trans_info;
}

int is_cksum_valid(ctcp_segment_t* segment, size_t len){
  uint16_t rcvd_cksum = segment->cksum;
  segment->cksum = 0;
  return rcvd_cksum == cksum(segment, len);
}

int is_ack(ctcp_state_t* state, ctcp_segment_t* segment){
  const uint16_t data_sz = ntohs(segment->len) - HDR_CTCP_SEGMENT;
  // If flag is ACK and length of data part is zero, it is ACK.
  // segment->flags and TH_ACK are network byte order.
  return (segment->flags & TH_ACK) && (data_sz == 0);

}

void send_segment(ctcp_state_t* state, ctcp_transmission_info_t* trans_info, size_t data_len){
  /* Send only if the other(receiver)'s buf is available. Otherwise, store segments in waiting_segments.*/
  ctcp_segment_t *segment = &trans_info->segment;
  state->curr_seqno += data_len; // Update sequence number.
  if((state->tx_in_flight_bytes + data_len) <= state->config.send_window){
    state->tx_in_flight_bytes += data_len;
    trans_info->num_of_transmission += 1; // When 7, terminate??
    // update segment's acknowledgement number because this host could ack received segments while transmitting.
    segment->ackno = htonl(state->curr_ackno);
    segment->cksum = 0;
    segment->cksum = cksum(segment, htons(segment->len)); // update checksum since segment's ackno might be changed.
    ll_add(state->segments, trans_info);
    /* Send it to the connection associated with the passed in state */
    _log_info("[TX] Sent segment.\n");
    print_hdr_ctcp(segment);
    int sent = conn_send(state->conn, segment, data_len + HDR_CTCP_SEGMENT);
    if(sent == 0){
      _log_info("[Tx] Nothing was sent.\n");
    }else if(sent==-1){
      _log_info("[Tx] Error occured while conn_send.\n");
    }
  }else{
    _log_info("[TX]Receiver buffer(%d) is not enough to send %lu bytes of data on connection w/ %d in-flight bytes. Segment is stored in waiting_segments.\n",
      state->config.send_window, data_len, state->tx_in_flight_bytes);
    ll_add(state->waiting_segments, trans_info);
    _log_info("# of waiting segments: %d.\n", state->waiting_segments->length);
  }
}

int is_new_data_segment(ctcp_state_t *state, ctcp_segment_t *segment){
  /* returns: Check if segment is newly received and it has data(not only header). 
   FYI, FIN segment also can be data segment because it is considered as 1-byte segment.*/
  return (state->curr_ackno <= ntohl(segment->seqno)) && (ntohs(segment->len) > HDR_CTCP_SEGMENT);
}

void send_only_ack(ctcp_state_t* state, ctcp_segment_t* rcvd_segment){
  /* ACK segment's data size is 0. So, it can be directly sent to the other host 
  no matter how much space the receiver buffer has. */
  assert(state->curr_ackno == ntohl(rcvd_segment->seqno));
  uint32_t new_ackno = ntohl(rcvd_segment->seqno) + (ntohs(rcvd_segment->len) - HDR_CTCP_SEGMENT);
  state->curr_ackno = new_ackno;
  rcvd_segment->seqno = htonl(state->curr_seqno);
  rcvd_segment->ackno = htonl(state->curr_ackno);
  rcvd_segment->len = htons(HDR_CTCP_SEGMENT);
  rcvd_segment->flags = TH_ACK;
  rcvd_segment->window = htons(state->config.recv_window); // Advertise the size of bytes that can be received from sender.
  rcvd_segment->cksum = 0;
  
  rcvd_segment->cksum = cksum(rcvd_segment, HDR_CTCP_SEGMENT);
  int sent = conn_send(state->conn, rcvd_segment, HDR_CTCP_SEGMENT);
  if(sent == 0){
    _log_info("[Tx] Nothing was sent.\n");
  }else if(sent==-1){
    _log_info("[Tx] Error occured while conn_send ack.\n");
  }
  _log_info("[Rx] ACK sent.\n");
  print_hdr_ctcp(rcvd_segment);

  // uint32_t new_ackno = ntohl(rcvd_segment->seqno) + ntohs(rcvd_segment->len);
  // ctcp_segment_t *ack_segment = create_segment(state->curr_seqno, new_ackno, TH_ACK, 0, NULL);
  // state->curr_ackno = new_ackno; // Update the last acked number for the otherside(sender)
  // assert(conn_send(state->conn, ack_segment, HDR_CTCP_SEGMENT) > 0);
  
}