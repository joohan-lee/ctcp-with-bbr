/**********************************************************************
 * file:  sr_router.c
 * date:  Mon Feb 18 12:50:42 PST 2002
 * Contact: casado@stanford.edu
 *
 * Description:
 *
 * This file contains all the functions that interact directly
 * with the routing table, as well as the main entry method
 * for routing.
 *
 **********************************************************************/

#include <stdio.h>
#include <assert.h>


#include "sr_if.h"
#include "sr_rt.h"
#include "sr_router.h"
#include "sr_protocol.h"
#include "sr_arpcache.h"
#include "sr_utils.h"

/*---------------------------------------------------------------------
 * Method: sr_init(void)
 * Scope:  Global
 *
 * Initialize the routing subsystem
 *
 *---------------------------------------------------------------------*/

void sr_init(struct sr_instance* sr)
{
    /* REQUIRES */
    assert(sr);

    /* Initialize cache and cache cleanup thread */
    sr_arpcache_init(&(sr->cache));

    pthread_attr_init(&(sr->attr));
    pthread_attr_setdetachstate(&(sr->attr), PTHREAD_CREATE_JOINABLE);
    pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
    pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
    pthread_t thread;

    pthread_create(&thread, &(sr->attr), sr_arpcache_timeout, sr);
    pthread_detach(thread);
    
    /* Add initialization code here! */

} /* -- sr_init -- */

/*---------------------------------------------------------------------
 * Method: sr_handlepacket(uint8_t* p,char* interface)
 * Scope:  Global
 *
 * This method is called each time the router receives a packet on the
 * interface.  The packet buffer, the packet length and the receiving
 * interface are passed in as parameters. The packet is complete with
 * ethernet headers.
 *
 * Note: Both the packet buffer and the character's memory are handled
 * by sr_vns_comm.c that means do NOT delete either.  Make a copy of the
 * packet instead if you intend to keep it around beyond the scope of
 * the method call.
 *
 *---------------------------------------------------------------------*/

void sr_handlepacket(struct sr_instance* sr,
        uint8_t * packet/* lent */,
        unsigned int len /* bytes */,
        char* interface/* lent. receiving interface */)
{
  /* REQUIRES */
  assert(sr);
  assert(packet);
  assert(interface);

  printf("*** -> Received packet of length %d \n",len);

  /* fill in code here */

  /************* region - Debugging ****************/
  /*printf("-- All headers --\n");
  print_hdrs(packet, len);
  printf("-- End All headers --\n");
  /************* endregion - Debugging ************/
  
  struct sr_if *received_sr_if = sr_get_interface(sr, interface);
  struct sr_ethernet_hdr* e_hdr = 0;
  e_hdr = (struct sr_ethernet_hdr*)packet;

  /* Determine if the packet is IP Packet or ARP Packet */
  if(ethertype(packet)==ethertype_ip){
    /* Drop the packet, if it is too small. */
    if (len < sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t)) {
      return;
    }
    
    /* copy received packet */
    uint8_t *copied_pkt = (uint8_t*)malloc(len);
    memcpy(copied_pkt, packet, len);
    sr_ethernet_hdr_t* copied_e_hdr = (sr_ethernet_hdr_t*)(copied_pkt);
    sr_ip_hdr_t* copied_iphdr = (sr_ip_hdr_t *)((uint8_t*)(copied_pkt) + sizeof(sr_ethernet_hdr_t));

    /* Check if header has a correct checksum */
    sr_ip_hdr_t *temp_iphdr = (sr_ip_hdr_t*)malloc(sizeof(sr_ip_hdr_t)); /* for checksum */
    memcpy(temp_iphdr, copied_iphdr, sizeof(sr_ip_hdr_t));

    temp_iphdr->ip_sum = 0; /* When calculating checksum, header checksum value should be zero. */
    uint16_t calculated_cksum= cksum(temp_iphdr, (temp_iphdr->ip_hl)*sizeof(uint32_t));
    free(temp_iphdr);
    if (calculated_cksum != copied_iphdr->ip_sum) {
      fprintf(stderr, "ERR: Invalid check sum.\n");
      return;
    }

    sr_ip_hdr_t* iphdr = (sr_ip_hdr_t *)((uint8_t*)(packet) + sizeof(sr_ethernet_hdr_t));

    if (is_router_ip(sr, iphdr->ip_dst)) {
      /* pointers to each header in copied packet. */
      sr_icmp_t3_hdr_t* copied_icmp_hdr = (sr_icmp_t3_hdr_t *)((uint8_t*)(copied_iphdr) + sizeof(sr_ip_hdr_t));

      /* LPM to find outgoing interface to send back ICMP reply */
      struct sr_rt* rt_matched_entry = find_sr_rt_by_ip(sr, copied_iphdr->ip_src);
      assert(rt_matched_entry);
      uint8_t rcvd_ip_p = ip_protocol((uint8_t*)copied_iphdr);
      if(rcvd_ip_p == ip_protocol_icmp){
        if (copied_icmp_hdr->icmp_type == 8) {
          /* -- ICMP echo request. -> Echo reply. -- */
          
          struct sr_if* icmp_out_sr_if = sr_get_interface(sr, rt_matched_entry->interface);

          /* Set Ethernet header */
          set_eth_hdr(copied_e_hdr, copied_e_hdr->ether_shost, icmp_out_sr_if->addr, htons(ethertype_ip));

          /* Set IP header */
          /* src and dest ip addr of reply. */
          uint32_t src_ip = copied_iphdr->ip_dst;
          uint32_t dst_ip = copied_iphdr->ip_src;
          set_ip_hdr(copied_iphdr, len - sizeof(sr_ethernet_hdr_t), ip_protocol_icmp, dst_ip, src_ip);

          /* Set ICMP header */
          set_icmp3_hdr(copied_icmp_hdr, 0, 0, len);

          /* Send ICMP reply */
          int send_res = sr_send_packet(sr, copied_pkt, len, icmp_out_sr_if->name);
        }
      }
      else if( rcvd_ip_p == ip_protocol_tcp || rcvd_ip_p == ip_protocol_udp){
        /* IP packet containing a UDP or TCP payload. -> PORT Unreachable.*/
        struct sr_if* rcv_sr_if = sr_get_interface(sr, interface);

        /* Set Ethernet header */
        set_eth_hdr(copied_e_hdr, copied_e_hdr->ether_shost, rcv_sr_if->addr, htons(ethertype_ip)); /* dhost = mac of iface of incoming packet */

        /* Set the IP header */
        /* src and dest ip addr of reply. */
        uint32_t dst_ip = copied_iphdr->ip_src;
        uint32_t src_ip = rcv_sr_if->ip; /* IP addr of incoming interface.*/
        set_ip_hdr(copied_iphdr, len - sizeof(sr_ethernet_hdr_t), ip_protocol_icmp, dst_ip, src_ip);

        /* Set the ICMP header */
        set_icmp3_hdr(copied_icmp_hdr, 3, 3, len);

        /* if icmp message is Destination unreachable or Time exceeded, should fill data part with original IP(UDP) packet */
        copy_iphdr_and_data_for_icmp(copied_icmp_hdr, packet, len);

        /* Send ICMP reply */
        int send_res = sr_send_packet(sr, copied_pkt, len, rcv_sr_if->name);
        
      }
      else{
        /* Otherwise, ignore packet. */
        ;
      }
      
    } else { /** If not for me, normal forwarding logic.*/
      /***
      * if the frame contains an IP packet whose destination is not one of the router’s interfaces:
      * 1.  Check that the packet is valid (is large enough to hold an IP header and has a correct checksum).
      * 2.  Decrement the TTL by 1, and recompute the packet checksum over the modified header.
      * 3.  Find out which entry in the routing table has the longest prefix match with the destination IP address.
      * 4. Check the ARP cache for the next-hop MAC address corresponding to the next-hop IP. If it’s there, send it. 
      *    Otherwise, send an ARP request for the next-hop IP (if one hasn’t been sent within the last second), and add the 
      *    packet to the queue of packets waiting on this ARP request.
      */

      /* 
      * Decrement the TTL by 1, and recompute the packet checksum over the modified header.
      */
      copied_iphdr->ip_ttl--;
      copied_iphdr->ip_sum = 0;
      copied_iphdr->ip_sum = cksum(copied_iphdr, sizeof(sr_ip_hdr_t));

      /* After decrementing ttl by 1,
      * if ttl is equal to 0,
      * Send ICMP message(Time exceeded) and discard the packet. 
      */
      if(copied_iphdr->ip_ttl==0){/* ICMP message - Time exceeded (type 11, code 0) */
        /* Set each header's fields which are needed to send ICMP message. */
        struct sr_if *rcv_sr_if = sr_get_interface(sr, interface);
        /* Set the Ethernet header */
        set_eth_hdr(copied_e_hdr, copied_e_hdr->ether_shost, rcv_sr_if->addr, htons(ethertype_ip)); /* shost = mac of iface of incoming packet */

        /* Set the IP header */
        uint32_t dst_ip = copied_iphdr->ip_src;
        uint32_t src_ip = rcv_sr_if->ip; /* IP addr of incoming interface.*/
        /*set_ip_hdr(copied_iphdr, len - sizeof(sr_ethernet_hdr_t), ip_protocol_icmp, dst_ip, src_ip);*/
        set_ip_hdr(copied_iphdr, sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t3_hdr_t), ip_protocol_icmp, dst_ip, src_ip);


        /* Set the ICMP header */
        struct sr_icmp_t3_hdr_t *copied_icmp_hdr = (sr_icmp_t3_hdr_t*)((uint8_t*)(copied_iphdr) + sizeof(sr_ip_hdr_t));
        set_icmp3_hdr(copied_icmp_hdr, 11, 0, len);

        /* if icmp message is Destination unreachable or Time exceeded, should fill data part with original IP(UDP) packet */
        copy_iphdr_and_data_for_icmp(copied_icmp_hdr, packet, len);

        /* Send ICMP message */
        int send_res = sr_send_packet(sr, copied_pkt, len, rcv_sr_if->name);

        free(copied_pkt); /* After handling ip packet, free copied*/
        return;

      } /* end - ICMP message - Time exceeded*/

      /* LPM to find incoming IP packet's sr_rt(dest,gw,mask,iface)*/
      struct sr_rt* rt_matched_entry = find_sr_rt_by_ip(sr, copied_iphdr->ip_dst);

      /* Dest IP addr did not match anything in routing table.
      * ICMP Message: Destination net unreachable (type 3, code 0) Sent if there is a non-existent route to the destination IP 
      * (no matching entry in routing table when forwarding an IP packet).
      */
      if(rt_matched_entry!=NULL){ /* LPM matched. */
        /* Forward the packet.
        * 2. Check ARP cache
        *   2-1. If exists, forward IP packet to the next-hop.
        *   2-2. If not, add ARP request to ARP Queue.
        */
        
        /* Lookup ARP Queue entries to find MAC addr of received IP dest address. */
        struct sr_arpentry *sr_arpentry_copy = sr_arpcache_lookup(&sr->cache, iphdr->ip_dst);
        if(sr_arpentry_copy){/* If exists in arpcache, farward IP packet. */
          struct sr_if *out_fwd_sr_if = sr_get_interface(sr, rt_matched_entry->interface); /* Send through interface which matched in routing table.*/

          /* Edit ethernet header of packet to forward */
          memcpy(copied_e_hdr->ether_dhost, sr_arpentry_copy->mac, ETHER_ADDR_LEN);
          memcpy(copied_e_hdr->ether_shost, out_fwd_sr_if->addr, ETHER_ADDR_LEN);

          /* Send */
          sr_send_packet(sr, copied_pkt, len, out_fwd_sr_if->name);

          /* Free sr_arpentry_copy */
          free(sr_arpentry_copy); /* This should be free after sending. */
      
        }else{/* If does not exist in arp cache, Add a request into ARP Queue */  
          /* Add an ARP request to ARP Queue. returns the newly added *sr_arpreq(=req) */

          struct sr_arpreq *req = 0;
          req = sr_arpcache_queuereq(&(sr->cache), rt_matched_entry->gw.s_addr, copied_pkt, len, rt_matched_entry->interface);
          /*req = sr_arpcache_queuereq(&(sr->cache), copied_iphdr->ip_dst, copied_pkt, len, rt_matched_entry->interface); /* interface=outgoing interface*/

        }
      }
      else{  /* LPM not matched. */
        /* Send ICMP message. -> Destination net unreachable (type 3, code 0). */
        struct sr_if *rcv_sr_if = sr_get_interface(sr, interface);

        /* Set the Ethernet header */
        set_eth_hdr(copied_e_hdr, copied_e_hdr->ether_shost, rcv_sr_if->addr, htons(ethertype_ip)); /* shost = mac of iface of incoming packet */

        /* Set the IP header */
        uint32_t dst_ip = copied_iphdr->ip_src;
        uint32_t src_ip = rcv_sr_if->ip; /* IP addr of incoming interface.*/
        set_ip_hdr(copied_iphdr, len - sizeof(sr_ethernet_hdr_t), ip_protocol_icmp, dst_ip, src_ip);

        /* Set the ICMP header */
        struct sr_icmp_t3_hdr_t *copied_icmp_hdr = (sr_icmp_t3_hdr_t*)((uint8_t*)(copied_iphdr) + sizeof(sr_ip_hdr_t));
        set_icmp3_hdr(copied_icmp_hdr, 3, 0, len);

        /* if icmp message is Destination unreachable or Time exceeded, should fill data part with original IP(UDP) packet */
        copy_iphdr_and_data_for_icmp(copied_icmp_hdr, packet, len);

        /* Send ICMP message */
        int send_res = sr_send_packet(sr, copied_pkt, len, rcv_sr_if->name);
      }
    }

    free(copied_pkt); /* After handling ip packet, free copied packet*/

  }
  else if(ethertype(packet)==ethertype_arp){
    struct sr_arp_hdr*       a_hdr = 0;
    a_hdr = (struct sr_arp_hdr*)(packet + sizeof(struct sr_ethernet_hdr));

    /**
    * Router can send a request of ARP and receive the reply.
    * Router can receive a request of ARP and reply it.
    */

    /* --- Figure out if it is reply or request to me --- */
    if(a_hdr->ar_op == htons(arp_op_request)){
      /* -- If it is a request to me -- */
      /* Construct an ARP reply and send it back */
      /* ARP request is broadcast. operation code == 1
      * target MAC addr = ff-ff-ff-ff-ff-ff
      * target IP addr should be given.
      */

      /* If arp request is for the MAC addr of the current router itself, immediately send a reply with it.*/

      /*sr_print_if(received_sr_if); /* Debug*/
      /*struct in_addr ip_addr; /* Debug*/
      /*ip_addr.s_addr = a_hdr->ar_tip; /* Debug*/
      /*Debug("\ta_hdr->ar_tip: %s\n", inet_ntoa(ip_addr)); /* Debug*/

      if(a_hdr->ar_tip == received_sr_if->ip){
        /* 1. Copy the packet */
        uint8_t *copied_pkt = (uint8_t*)malloc(len);
        memcpy(copied_pkt, packet, len);
        /* 2. Edit the ethernet destination and source MAC addresses, plus whatever fields of the packet are relevant*/
        sr_ethernet_hdr_t *copied_ehdr = (sr_ethernet_hdr_t *)copied_pkt;
        sr_arp_hdr_t *copied_a_hdr = (struct sr_arp_hdr*)(copied_pkt + sizeof(struct sr_ethernet_hdr));
        /* Ethernet header*/
        memcpy(copied_ehdr->ether_dhost, e_hdr->ether_shost, sizeof(uint8_t) * ETHER_ADDR_LEN);
        memcpy(copied_ehdr->ether_shost, received_sr_if->addr, sizeof(uint8_t) * ETHER_ADDR_LEN);

        /* ARP header */
        copied_a_hdr->ar_op = htons(arp_op_reply);
        memcpy(copied_a_hdr->ar_sha, received_sr_if->addr, sizeof(unsigned char) * ETHER_ADDR_LEN);
        copied_a_hdr->ar_sip = received_sr_if->ip;
        memcpy(copied_a_hdr->ar_tha, a_hdr->ar_sha, sizeof(unsigned char) * ETHER_ADDR_LEN);
        copied_a_hdr->ar_tip = a_hdr->ar_sip;

        /* 3. Send it. */
        sr_send_packet(sr, copied_pkt, len, interface);
        free(copied_pkt); /* HACK: free here?*/
      }
      
    }
    else if(a_hdr->ar_op == htons(arp_op_reply)){
      /* -- If it is a reply to me -- */
      /* Cache it, go through my request queue and send outstanding pakcets
      * (=fill out destination MAC addr of the raw Ethernet frame (in packets waiting on that packet)?) 
      */
      /* ARP reply is unicast. operation code == 2*/

      /* Cache it */
      struct sr_arpreq *sr_arpreq_for_reply;
      sr_arpreq_for_reply = sr_arpcache_insert(&sr->cache, a_hdr->ar_sha, a_hdr->ar_sip);

      /* If there are requests wating on this arp reply, send outstanding packets. */
      
      if(sr_arpreq_for_reply){
        struct sr_packet *curr_sr_pkt = sr_arpreq_for_reply->packets;
        while(curr_sr_pkt){
          uint8_t *curr_sr_pkt_buf = curr_sr_pkt->buf; /* current packet. (it includes ethernet header)*/
          
          /* Edit Ethernet header. src=mac addr of out iface. dest=mac addr of source where arp reply came from. */
          struct sr_ethernet_hdr* q_reply_e_hdr = (struct sr_ethernet_hdr*)curr_sr_pkt_buf;
          struct sr_if *out_sr_if = sr_get_interface(sr, interface); /* Send through interface which arp reply received from.*/
          memcpy(q_reply_e_hdr->ether_dhost, a_hdr->ar_sha, ETHER_ADDR_LEN); /* Send to src addr of received arp reply */
          memcpy(q_reply_e_hdr->ether_shost, out_sr_if->addr, sizeof(unsigned char) * ETHER_ADDR_LEN);
          q_reply_e_hdr->ether_type = q_reply_e_hdr->ether_type;

          /* Send outstanding packets (forwarding) */
          sr_send_packet(sr, curr_sr_pkt_buf, curr_sr_pkt->len, out_sr_if->name);

          curr_sr_pkt = curr_sr_pkt->next;
        }
        
        sr_arpreq_destroy(&sr->cache, sr_arpreq_for_reply);
      }
    }
  }
  

}/* end sr_ForwardPacket */

/**
 * Check if checksum in ip header is valid
 */
int is_valid_ip_cksum(sr_ip_hdr_t *iphdr){
  /* Copy iphdr */
  sr_ip_hdr_t *copied_iphdr = (sr_ip_hdr_t*)malloc(sizeof(sr_ip_hdr_t));
  memcpy(copied_iphdr, iphdr, sizeof(sr_ip_hdr_t));
  copied_iphdr->ip_sum=0;
  int is_valid = (iphdr->ip_sum==cksum(copied_iphdr, sizeof(sr_ip_hdr_t)));

  /* free */
  free(copied_iphdr);

  return is_valid;
}

/**
 * Check if checksum in icmp header is valid
 */
int is_valid_icmp_cksum(sr_icmp_hdr_t *icmp_hdr){
  /* Copy iphdr */
  sr_icmp_hdr_t *copied_icmp_hdr = (sr_icmp_hdr_t*)malloc(sizeof(sr_icmp_hdr_t));
  memcpy(copied_icmp_hdr, icmp_hdr, sizeof(sr_icmp_hdr_t));
  copied_icmp_hdr->icmp_sum=0;
  int cs = cksum(copied_icmp_hdr, sizeof(sr_icmp_hdr_t));
  int is_valid = (icmp_hdr->icmp_sum==cs);

  /* free */
  free(copied_icmp_hdr);

  return is_valid;
}

/**
 * Set proper values to ethernet header
 */
void set_eth_hdr(sr_ethernet_hdr_t *e_hdr,
                  uint8_t dhost[ETHER_ADDR_LEN],
                  uint8_t shost[ETHER_ADDR_LEN],
                  uint16_t e_type){
  memcpy(e_hdr->ether_dhost, dhost, ETHER_ADDR_LEN); 
  memcpy(e_hdr->ether_shost, shost, ETHER_ADDR_LEN); 
  e_hdr->ether_type = e_type;

  return;
}

/**
 * Set proper values to ip header 
 */
void set_ip_hdr(sr_ip_hdr_t *iphdr,
                  uint16_t ip_len,
                  uint8_t ip_p,
                  uint32_t ip_dst, 
                  uint32_t ip_src
                  )
{
  iphdr->ip_hl=(int)(sizeof(sr_ip_hdr_t) / sizeof(uint32_t));
  iphdr->ip_v=4;
  iphdr->ip_tos=0;
  iphdr->ip_len=htons(ip_len);
  iphdr->ip_id=0;
  iphdr->ip_off=htons(IP_DF);
  iphdr->ip_ttl=64;
  iphdr->ip_p=ip_p;
  iphdr->ip_dst = ip_dst;
  iphdr->ip_src = ip_src;
  iphdr->ip_sum = 0;
  iphdr->ip_sum = cksum(iphdr, sizeof(sr_ip_hdr_t));

  return;
}

/**
 * Set proper values to ICMP header 
 */
void set_icmp_hdr(sr_icmp_hdr_t *icmp_hdr, uint8_t icmp_type, uint8_t icmp_code){
  icmp_hdr->icmp_type=icmp_type;
  icmp_hdr->icmp_code=icmp_code;
  icmp_hdr->icmp_sum=0;
  uint16_t cs = cksum(icmp_hdr, sizeof(sr_icmp_hdr_t));
  icmp_hdr->icmp_sum = cs;
  return;
}

/**
 * Set proper values to ICMP Type3 header 
 */
void set_icmp3_hdr(sr_icmp_t3_hdr_t *icmp3_hdr, uint8_t icmp_type, uint8_t icmp_code, uint total_len){
  icmp3_hdr->icmp_type=icmp_type;
  icmp3_hdr->icmp_code=icmp_code;
  icmp3_hdr->icmp_sum=0;
/* let fields below remain same as received packet.
  icmp3_hdr->unused=0;
  icmp3_hdr->next_mtu=0; /* 09/07/2023. Does not Fragment yet, so set it to 0 or 1500 which is default.  */
  /*memcpy(&(icmp3_hdr->data), &data, ICMP_DATA_SIZE);*/
  
  /* Update ckecksum */
  /*icmp3_hdr->icmp_sum = cksum(icmp3_hdr, total_len - sizeof(sr_ethernet_hdr_t) - sizeof(sr_ip_hdr_t));*/
  icmp3_hdr->icmp_sum = cksum(icmp3_hdr, sizeof(sr_icmp_t3_hdr_t));

  return;
}

/*
 * Copy original incoming packet's IP header and data(ICMP type, code, cksum)
 * into ICMP message that will be sent.
 * FYI, original incoming packet would be UDP packet(which is 74B) for traceroute.
*/
void copy_iphdr_and_data_for_icmp(sr_icmp_t3_hdr_t* icmp_hdr, uint8_t* incoming_pkt, uint32_t incoming_pkt_len){
  sr_ip_hdr_t *incoming_iphdr = (sr_ip_hdr_t*)(incoming_pkt + sizeof(sr_ethernet_hdr_t));
  uint16_t incoming_iphdr_len = incoming_iphdr->ip_hl * 4;
  
  /* original incoming packet size is too short to copy ip header and 64 bit data */
  assert(incoming_pkt_len >= (sizeof(sr_ethernet_hdr_t) + incoming_iphdr_len + 8/* data(type,code,cksum)*/));

  uint8_t *icmp_data_ptr = ((uint8_t*)(icmp_hdr) + sizeof(sr_icmp_hdr_t)/* 4bytes=type(1B)+code(1B)+cksum(2B)*/ + 4 /*unused*/);
  
  memcpy(icmp_data_ptr, incoming_iphdr, incoming_iphdr_len + 8 /* original data=(type,code,cksum)?? or ICMP echo request/reply's data */);
  /*Debug*/
  /*Debug("Added incoming original ip header and data into icmp message: \n");
  print_hdr_ip(icmp_data_ptr);
  print_hdr_icmp(icmp_data_ptr + incoming_iphdr_len);*/

  
  
}

/*
 * Check if the destionation IP address is one of router's IP addresses(for me).
 * returns 1 if it is, otherwise, 0.
 */
int is_router_ip(struct sr_instance* sr, 
        uint32_t dst_ip)
{
  struct sr_if* iface = sr->if_list;
  for (;iface; iface = iface->next) {
    if (iface->ip == dst_ip)
      return 1;
  }
  return 0;
}

/** Return: struct sr_rt*
 * Find longest prefix matched sr_rt using destinatin of IP addr.
 */
struct sr_rt* find_sr_rt_by_ip(struct sr_instance* sr, uint32_t dst_ip){
  struct sr_rt *rt_walker = sr->routing_table;
  uint32_t lpm_mask = 0; /* longest prefix matched mask */
  struct sr_rt *rt_matched_entry = NULL; /* longest prefix matched entry */
  while(rt_walker)
  {
    /* masking IP dest addr */
    if((dst_ip & rt_walker->mask.s_addr) == rt_walker->dest.s_addr &&
      rt_walker->mask.s_addr >= lpm_mask){
        lpm_mask = rt_walker->mask.s_addr;
        rt_matched_entry = rt_walker;
        /*sr_print_routing_entry(rt_matched_entry);Debug*/
    }
    rt_walker = rt_walker->next;
  }
  return rt_matched_entry;
}