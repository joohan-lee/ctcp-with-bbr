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
  //uint32_t curr_ackno; /* current waiting acknowledge number */
  uint32_t otherside_ackno; /* Last Acked number for the other side. */
  uint32_t rx_next_output_seqno; /* Sequence number to output next time(=seqno waiting for output). */

  uint32_t tx_in_flight_bytes; /* Outstanding bytes(sent but not acknowledged) = inflight bytes. 
                                  When this host is Tx.
                                */
  uint32_t rx_waiting_bytes; /* size of data that was received but are waiting for being outputted. 
                                Since they are not outputted, they are not acked yet.
                               */

  int32_t termination_state; /* TCP Connection Termination state */ 
  
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
  state->received_segments = ll_create();

  state->curr_seqno=1;
  // state->curr_ackno=1;
  state->otherside_ackno=1;
  state->rx_next_output_seqno=1;
  
  state->tx_in_flight_bytes=0;
  state->rx_waiting_bytes=0;

  state->termination_state=CONN_ESTABLISHED;

  return state;
}

void ctcp_destroy(ctcp_state_t *state) {
  /* Update linked list. */
  if (state->next)
    state->next->prev = state->prev;

  *state->prev = state->next;
  conn_remove(state->conn);

  /* FIXME: Do any other cleanup here. */

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
    ctcp_segment_t *segment = create_segment(state->curr_seqno, state->otherside_ackno, TH_ACK, stdin_data_sz, stdin_buf);
    state->curr_seqno += ntohs(segment->len); // Update sequence number.
    // fprintf(stderr,"Segement created.\n");
    _log_info("Segment is created. ");
    print_hdr_ctcp(segment);
    
    /* TODO: Send only if rcvr buf is available. Otherwise, store segments in waiting_segments.*/
    if(1){
      // TODO: Update otherside's ackno??
      ll_add(state->segments, segment);
      /* Send it to the connection associated with the passed in state */
      assert(conn_send(state->conn, segment, HDR_CTCP_SEGMENT + stdin_data_sz) > 0);
    }else{
      _log_info("Receiver buffer is not enough. Segment is stored in waiting_segments.\n");
      ll_add(state->waiting_segments, segment);
      _log_info("# of waiting segments: %d.\n", state->waiting_segments->length);
    }
  }

  if(stdin_data_sz==-1 /* EOF */
    && ll_length(state->segments) ==0 /* No in-flight segments */
    && ll_length(state->waiting_segments) == 0 /* No pending segments to be sent */
  ){
    /* Termination */
    // TODO: 여기부터

  }

}

void ctcp_receive(ctcp_state_t *state, ctcp_segment_t *segment, size_t len) {
  assert(ntohs(segment->len) == len);

  _log_info("Segment is received. len: %lu ", len);
  print_hdr_ctcp(segment);

  /* Check if cksum is valid. If not, drop the packet. */
  if(!is_cksum_valid(segment, len)){
    fprintf(stderr, "Invalid checksum. Drop the received packet.\n");
    free(segment);
    return;
  }

  // TODO: FIN segment면 바로 ack 보내기. FIN Segment는 1-byte segment로 간주.
  // TODO: FIN + ACK면 connection state destroy.??
  
  /* If received segment is ACK, update ackno. */
  if(is_ack(state, segment)){
    _log_info("ACK segment received.\n");
    /* Remove all sent segments that has acked from transmission buffer(='state->segments' linked list)*/
    uint32_t size_of_acked_segments = ll_remove_acked_segments(state->segments, segment->ackno);
    state->tx_in_flight_bytes -= size_of_acked_segments;

    // TODO: Send pending segments(= segments that wasn't sent due to lack of send window(other's buf size))
    free(segment);
    return;
  }
  
  /* If segment is newly received data, add it to receiver buffer in proper index.
   * Otherwise, drop it. 
   */
  if((state->otherside_ackno <= ntohl(segment->seqno)) && (ntohs(segment->len) > HDR_CTCP_SEGMENT)){
    ll_node_t *added_node = ll_add_in_order(state->received_segments, segment); // TODO: receiver buffer 적절한 위치에 들어가는지 테스트 필요.
    if(added_node){
      state->rx_waiting_bytes += len; //Increase size of data that received but not output.
      _log_info("Current Rx buffer size waiting for output: %d after added.\n", state->rx_waiting_bytes);
    }else{
      /* If it was not added into receiver buffer, Drop the packet(segment). */
      free(segment);
    }
    // TODO: Receiver buffer(state->received_segments) overflow하면 drop하기.
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

    /* TODO: Output segment only if it is in-order.
     새로 받은 segment랑 이전에 output(ack)했던 segment 사이에 hole이 없을 때만 쭉 output하기.
     =>개발 완료. 테스트 필요.
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
      state->rx_next_output_seqno += ntohs(rcvd_segment->len);
      state->rx_waiting_bytes -= ntohs(rcvd_segment->len);
      _log_info("Output data to STDOUT. Available bufspace: %lu.\n", available_bufspace);
      _log_info("state->rx_next_output_seqno: %d, state->rx_waiting_bytes: %d. after outputting.\n", state->rx_next_output_seqno, state->rx_waiting_bytes);
      assert(conn_output(state->conn, rcvd_segment->data, rcvd_data_sz));

      // Send ACK. (Can be piggyback or separate.)
      // Send ACK only.
      uint32_t new_ackno = ntohl(rcvd_segment->seqno) + ntohs(rcvd_segment->len);
      ctcp_segment_t *ack_segment = create_segment(state->curr_seqno, new_ackno, TH_ACK, 0, NULL);
      state->otherside_ackno = new_ackno; // Update the last acked number for the otherside(sender)
      assert(conn_send(state->conn, ack_segment, HDR_CTCP_SEGMENT) > 0);

      // Remove handled node from linked list(received_segments)
      ll_remove(state->received_segments, node); // This frees the node.
      //free(rcvd_segment); ??
    }
    else{
      _log_info("[ctcp_output] failed to output since output bufspace is not enough.\n");
      break;
    }
  }
}

void ctcp_timer() {
  /* FIXME */
}

ctcp_segment_t* create_segment(uint32_t seqno, uint32_t ackno,
  uint8_t flags, size_t data_sz, uint8_t data[]){
  size_t segment_total_sz = HDR_CTCP_SEGMENT + data_sz;
  ctcp_segment_t *segment = (ctcp_segment_t*)malloc(segment_total_sz);
  segment->seqno = htonl(seqno);
  segment->ackno = htonl(ackno);
  segment->len = htons(segment_total_sz);
  segment->flags = flags; // Network byte order
  segment->window = 0; // TODO: Receiver window size
  segment->cksum = 0;
  memcpy(segment->data, data, data_sz);

  // Calculate cksum
  segment->cksum = cksum(segment, segment_total_sz); // cksum function returns network byte order

  return segment;
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