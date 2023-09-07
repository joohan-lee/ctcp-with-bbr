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

  /* don't waste my time ... */
  if ( len < sizeof(struct sr_ethernet_hdr) ){
      fprintf(stderr , "** Error: packet is wayy to short \n");
      return -1;
  }
  
  struct sr_if *received_sr_if = sr_get_interface(sr, interface);
  struct sr_ethernet_hdr* e_hdr = 0;
  e_hdr = (struct sr_ethernet_hdr*)packet;

  /* Determine if the packet is IP Packet or ARP Packet */
  if(ethertype(packet)==ethertype_ip){
    printf("-- Received packet is ip packet--\n");
    
    sr_ip_hdr_t *iphdr = (sr_ip_hdr_t *)(packet + sizeof(struct sr_ethernet_hdr));
    sr_ip_hdr_t *copied_iphdr = (sr_ip_hdr_t*)malloc(sizeof(sr_ip_hdr_t));
    memcpy(copied_iphdr, iphdr, sizeof(sr_ip_hdr_t)); /* copy ip_hdr to edit checksum value before validating it */
    
    print_hdrs(packet, len);/*Debug*/

    /* -- 1. Check if the IP packet is valid -- */
    /* Check if IP packet is large enough to hold an IP header */
    if((iphdr->ip_hl) * 4 < 20){
      /* Header Length field is a 4-bit and represents the number of 32-bit words */
      /* HACK: Should check size of header is lower than 60 bytes? */
      fprintf(stderr , "ERR: Packet header size of %d is lower than 20.\n", iphdr->ip_hl);
      return -1;
    }

    /* Check if header has a correct checksum */
    copied_iphdr->ip_sum = 0; /* When calculating checksum, header checksum value should be zero. */
    uint16_t calculated_cksum= cksum(copied_iphdr, (iphdr->ip_hl)*4);
    if(calculated_cksum != iphdr->ip_sum){
      printf("ERR: Invalid checksum. %d != %d\n",calculated_cksum, iphdr->ip_sum);
      return;
    }else{
      printf("Valid Checksum. %d = %d\n", calculated_cksum, iphdr->ip_sum);
    }
    /* -- end- 1. Check if the IP packet is valid -- */

    /* 
     * 2.  Check if it is sent to one of the router’s IP addresses.(one of interfaces' IP addresses)  
     */
    if(ntohs(iphdr->ip_dst) == received_sr_if->ip){
      /*
       *  If yes, take one of three actions below.
       *  · If the packet is an ICMP echo request and its checksum is valid, send an ICMP echo reply to the sending host.
       *  · If the packet contains a TCP or UDP payload, send an ICMP port unreachable to the sending host. 
       *  · Otherwise, ignore the packet.
       */
      Debug("IP dest addr is one of router's IP addr!\n");
    }
    else{
      /*
       * If no, normal forwarding logic.
       */
      Debug("IP dest addr is NOT one of router's IP addr! Forwarding starts.\n");
      /* 1. LPM */
      sr_print_routing_table(sr);

      struct sr_rt *rt_walker = sr->routing_table;
      uint32_t lpm_mask = 0; /* longest prefix matched mask */
      struct sr_rt *rt_matched_entry = NULL; /* longest prefix matched entry */
      while(rt_walker->next)
      {
        /* masking received IP dest addr */
        if((iphdr->ip_dst & rt_walker->mask.s_addr) == rt_walker->dest.s_addr &&
          rt_walker->mask.s_addr >= lpm_mask){
            printf("--- LPM matched. --- \n");
            lpm_mask = rt_walker->mask.s_addr;
            rt_matched_entry = rt_walker;
            sr_print_routing_entry(rt_matched_entry);
        }
        rt_walker = rt_walker->next;
      }

      /* Dest IP addr did not match anything in routing table.
       * ICMP Message: Destination net unreachable (type 3, code 0) Sent if there is a non-existent route to the destination IP 
       * (no matching entry in routing table when forwarding an IP packet).
       */
      if(!rt_matched_entry){
        /* TODO: Send ICMP message */
        Debug("LPM NOT matched!!\n");
      }
      else{
        /* 
         * 2. Check ARP cache
         *   2-1. If exists, forward IP packet to the next-hop.
         *   2-2. If not, add ARP request to ARP Queue.
         */
         /* Lookup ARP Queue entries to find MAC addr of received IP dest address. */
        struct sr_arpentry *sr_arpentry_copy = sr_arpcache_lookup(&sr->cache, iphdr->ip_dst);
        if(sr_arpentry_copy){
          /* If exists, check if it is valid or not.*/
          if(sr_arpentry_copy->valid){
            /* If it is valid, farward IP packet. */
            Debug("IP dest addr exists in arp cache.\n Dest IP addr: ");
            print_addr_ip_int(iphdr->ip_dst); /*XXX*/
            Debug("\n its MAC addr: ");
            int i;
            for (i = 0; i < ETHER_ADDR_LEN; i++) {
                printf("%02x:", sr_arpentry_copy->mac[i]);
            }
            printf("\n"); /*Debug*/

            /* TODO: after sending free sr_arpentry_copy */
          }
          else{
            /* This case can exist??*/
            /* If it is not valid, add a request into ARP Queue. */
            printf("IP dest addr, %d, exists in arp cache. Its MAC addr is %s. But it is not valid(%d). \n", 
              iphdr->ip_dst, sr_arpentry_copy->mac, sr_arpentry_copy->valid); /*XXX*/
            
          }
      
        }else{
          /* If does not exist, Add a request into ARP Queue */
          Debug("IP dest addr, %d, does NOT exist in arp cache.\n", iphdr->ip_dst); /*XXX*/
          
          /* Add an ARP request to ARP Queue. returns the newly added *sr_arpreq(=req) */
          uint8_t *copy_pkt_arpq = (uint8_t*)malloc(len); /* copy packet to pass it into sr_arpcache_queuereq */
          memcpy(copy_pkt_arpq, packet, len);
          struct sr_arpreq *req = 0;
          req = sr_arpcache_queuereq(&(sr->cache), rt_matched_entry->gw.s_addr, copy_pkt_arpq, len, rt_matched_entry->interface);
          free(copy_pkt_arpq); /* free passed *packet */

        }
        

      }


    }

    /* -- end- 2.  Check if it is sent to one of the router’s IP addresses.(one of interfaces' IP addresses) -- */
    
    
    
    

  }
  else if(ethertype(packet)==ethertype_arp){
    printf("-- Received packet is arp packet--\n");
    
    struct sr_arp_hdr*       a_hdr = 0;
    a_hdr = (struct sr_arp_hdr*)(packet + sizeof(struct sr_ethernet_hdr));

    /*sr_arp_hdr_t *arp_hdr = (sr_arp_hdr_t *)(packet); XXX*/
    print_hdr_arp(a_hdr); /* XXX: print all fields of header in packet*/

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
      printf("---ARP REQUST!---\n"); /*XXX*/
      /* If arp request is for the MAC addr of the current router itself, immediately send a reply with it.*/
      
      if(a_hdr->ar_tip == received_sr_if->ip){
        
        sr_print_if(received_sr_if); /* Debug*/
        struct in_addr ip_addr; /* Debug*/
        ip_addr.s_addr = a_hdr->ar_tip; /* Debug*/
        Debug("\ta_hdr->ar_tip: %s\n", inet_ntoa(ip_addr)); /* Debug*/

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

        /*Debug("--- copied_pkt --- \n");
        print_hdrs(copied_pkt, len);/* Debug */

        /* 3. Send it. */
        sr_send_packet(sr, copied_pkt, len, interface);
        free(copied_pkt); /* HACK: free here?*/

      }
      else{
        /* If arp request is for the MAC addr of the current router itself, immediately send a reply with it.*/
        printf("ARP request for other IP address not in the current router.\n");
        
      }
      
    }
    else if(a_hdr->ar_op == htons(arp_op_reply)){
      /* -- If it is a reply to me -- */
      /* Cache it, go through my request queue and send outstanding pakcets
      * (=fill out destination MAC addr of the raw Ethernet frame (in packets waiting on that packet)?) 
      */
      /* ARP reply is unicast. operation code == 2*/
      Debug("---ARP REPLY!---\n");

      /* Cache it */
      struct sr_arpreq *sr_arpreq_for_reply;
      sr_arpreq_for_reply = sr_arpcache_insert(&sr->cache, a_hdr->ar_sha, a_hdr->ar_sip);

      /* If there are requests wating on this arp reply, send outstanding packets. */
      
      if(sr_arpreq_for_reply){
        Debug("req dest ip:\n "); /*Debug*/
        print_addr_ip_int(sr_arpreq_for_reply->ip); /*Debug*/

        struct sr_packet *curr_sr_pkt = sr_arpreq_for_reply->packets;
        while(curr_sr_pkt){
          uint8_t *curr_sr_pkt_buf = curr_sr_pkt->buf; /* current packet. (it includes ethernet header)*/
          
          /* Edit Ethernet header. src=mac addr of out iface. dest=mac addr of source where arp reply came from. */
          struct sr_ethernet_hdr* q_reply_e_hdr = (struct sr_ethernet_hdr*)curr_sr_pkt_buf;
          struct sr_if *out_sr_if = sr_get_interface(sr, interface); /* Send through interface which arp reply received from.*/
          memcpy(q_reply_e_hdr->ether_dhost, a_hdr->ar_sha, ETHER_ADDR_LEN); /* Send to src addr of received arp reply */
          memcpy(q_reply_e_hdr->ether_shost, out_sr_if->addr, sizeof(unsigned char) * ETHER_ADDR_LEN);
          q_reply_e_hdr->ether_type = q_reply_e_hdr->ether_type;
          
          
          Debug("--- Created ethernet for sending outstanding packets\n");
          /*print_hdr_eth(q_reply_e_hdr); /* Debug*/
          print_hdrs(curr_sr_pkt_buf, curr_sr_pkt->len);/*Debug*/

          /* Send outstanding packets (forwarding) */
          sr_send_packet(sr, curr_sr_pkt_buf, curr_sr_pkt->len, out_sr_if->name);

          curr_sr_pkt = curr_sr_pkt->next;
        }
        
        sr_arpreq_destroy(&sr->cache, sr_arpreq_for_reply); /* HACK: Destroy here? */
      }
    }else{
      printf("---arp opcode ERROR---\n");
    }
  }
  

  

}/* end sr_ForwardPacket */

