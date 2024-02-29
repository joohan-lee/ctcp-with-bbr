#include "ctcp_linked_list.h"

linked_list_t *ll_create() {
  linked_list_t *list = calloc(sizeof(linked_list_t), 1);
  list->head = NULL;
  list->tail = NULL;
  list->length = 0;
  return list;
}

void ll_destroy(linked_list_t *list) {
  if (list == NULL)
    return;

  ll_node_t *curr = list->head;
  ll_node_t *next = NULL;
  while (curr != NULL) {
    next = curr->next;
    free(curr);
    curr = next;
  }
  free(list);
}

void ll_free_objects(linked_list_t *list){
  if (list == NULL)
    return;

  ll_node_t *curr = list->head;
  while(curr != NULL){
    free(curr->object);
    curr = curr->next;
  }
}

ll_node_t *ll_create_node(void *object) {
  ll_node_t *node = calloc(sizeof(ll_node_t), 1);
  node->next = NULL;
  node->prev = NULL;
  node->object = object;
  return node;
}

ll_node_t *ll_add(linked_list_t *list, void *object) {
  if (list == NULL || object == NULL)
    return NULL;

  ll_node_t *node = ll_create_node(object);
  /* List is empty. */
  if (list->head == NULL) {
    list->head = node;
    list->tail = node;
  }

  /* List has one or more elements. */
  else {
    list->tail->next = node;
    node->prev = list->tail;
    list->tail = node;
  }

  list->length++;
  return node;
}

ll_node_t *ll_add_in_order(linked_list_t *list, void *object){
  if (list == NULL || object == NULL)
    return NULL;

  /* List is empty. */
  if (list->head == NULL) {
    ll_node_t *node = ll_create_node(object);
    list->head = node;
    list->tail = node;
    list->length = 1;
    return node;
  }
  
  /* 1. If it is less prioritized than head.(has lower seqno than head's.),
   * Adds object into the front.
   */
  ctcp_segment_t *add_seg = (ctcp_segment_t*)object;
  ctcp_segment_t *head_seg = (ctcp_segment_t*)list->head->object;
  ctcp_segment_t *tail_seg = (ctcp_segment_t*)list->tail->object;
  if(head_seg->seqno > add_seg->seqno){
    return ll_add_front(list, object);
  }
  /* 2. If it is higher prioritized than tail.(has larger seqno than tail's.),
  * Adds object into the end.
  */
  if(tail_seg->seqno < add_seg->seqno){  
    return ll_add(list, object);
  }

  /* 3. If its priority is middle of receiver buffer(has seqno between (head, tail)), 
  * Adds object into the middle where in-order rule is satisfied.
  */
  ll_node_t *curr = list->head;
  ctcp_segment_t *curr_seg = (ctcp_segment_t*)list->head->object;
  for(; curr_seg->seqno < add_seg->seqno; curr_seg = (ctcp_segment_t*)curr->next->object){
    ;
  }
  if(curr == NULL){
    fprintf(stderr,"[ERROR] Node is NULL while adding object into receiver buffer.\n");
    return NULL;
  }
  if(curr_seg->seqno == add_seg->seqno){
    fprintf(stderr,"Segment with same seqno is already in receiver buffer.\n");
    return NULL;
  }
  
  return ll_add_after(list, curr->prev, object);
}

ll_node_t *ll_add_front(linked_list_t *list, void *object) {
  if (list == NULL || object == NULL)
    return NULL;

  ll_node_t *node = ll_create_node(object);
  /* List is empty. */
  if (list->head == NULL) {
    list->head = node;
    list->tail = node;
  }

  /* List has one or more elements. */
  else {
    node->next = list->head;
    list->head->prev = node;
    list->head = node;
  }

  list->length++;
  return node;
}

ll_node_t *ll_add_after(linked_list_t *list, ll_node_t *node, void *object) {
  if (list == NULL || node == NULL || object == NULL)
    return NULL;

  ll_node_t *new_node = ll_create_node(object);
  /* Update pointers. */
  new_node->prev = node;
  new_node->next = node->next;
  if (node->next != NULL)
    node->next->prev = new_node;
  node->next = new_node;

  /* Added to end of list. */
  if (node == list->tail)
    list->tail = new_node;

  list->length++;
  return new_node;
}

void *ll_remove(linked_list_t *list, ll_node_t *node) {
  if (list == NULL || node == NULL)
    return NULL;
  void *object = node->object;

  /* Update linked list pointers. */
  if (node == list->head)
    list->head = node->next;
  else
    node->prev->next = node->next;

  if (node == list->tail)
    list->tail = node->prev;
  else
    node->next->prev = node->prev;

  /* Free memory. */
  free(node);
  list->length--;

  return object;
}

ssize_t ll_remove_acked_segments(linked_list_t *list, uint32_t received_ackno){
  /* received_seqno is network byte order. */
  /* Return size of acked segments. */
  if (list == NULL){
    return -1;
  }
  
  ll_node_t *curr = ll_front(list);
  uint32_t acked_size = 0;
  while(curr){
    ctcp_transmission_info_t *curr_object = (ctcp_transmission_info_t*)curr->object;
    ctcp_segment_t *curr_segment = &(curr_object->segment);
    if(ntohl(curr_segment->seqno) < ntohl(received_ackno)){
      ll_node_t *remove_node = curr;
      curr = curr->prev;
      acked_size += (ntohs(curr_segment->len) - HDR_CTCP_SEGMENT);
      ll_remove(list, remove_node);
      free(curr_object);
    }
    if(curr==NULL){
      break;
    }else{
      curr = curr->next;
    }
  }

  return acked_size;

}

ll_node_t *ll_find(linked_list_t *list, void *object) {
  if (list == NULL || object == NULL)
    return NULL;

  ll_node_t *curr = list->head;
  while (curr != NULL) {
    if (curr->object == object) {
      return curr;
    }
    curr = curr->next;
  }
  return NULL;
}

ll_node_t *ll_front(linked_list_t *list) {
  return list->head;
}

ll_node_t *ll_back(linked_list_t *list) {
  return list->tail;
}

unsigned int ll_length(linked_list_t *list) {
  return list->length;
}
