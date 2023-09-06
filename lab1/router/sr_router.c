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
  /* don't waste my time ... */
  if ( len < sizeof(struct sr_ethernet_hdr) ){
      fprintf(stderr , "** Error: packet is wayy to short \n");
      return -1;
  }

  /************* region - Debugging ****************/
  printf("-- All headers --\n");
  print_hdrs(packet, len);
  printf("-- End All headers --\n");
  /************* endregion - Debugging ************/
  
  struct sr_if *received_sr_if = sr_get_interface(sr, interface);
  struct sr_ethernet_hdr* e_hdr = 0;
  e_hdr = (struct sr_ethernet_hdr*)packet;

  /* Determine if the packet is IP Packet or ARP Packet */
  if(ethertype(packet)==ethertype_ip){
    printf("-- Received packet is ip packet--\n");
    sr_ip_hdr_t *iphdr = (sr_ip_hdr_t *)(packet + sizeof(struct sr_ethernet_hdr));
    sr_ip_hdr_t *copied_iphdr = (sr_ip_hdr_t*)malloc(sizeof(sr_ip_hdr_t));
    memcpy(copied_iphdr, iphdr, sizeof(sr_ip_hdr_t)); /* copy ip_hdr to edit checksum value before validating it */
    /*Debug("--copied_iphdr--\n"); */
    /* print_hdr_ip(copied_iphdr); /* Debug: print ip header*/

    /* -- 1. Check if the IP packet is valid -- */
    /* Check if IP packet is large enough to hold an IP header */
    if((iphdr->ip_hl) * 4 < 20){
      /* Header Length field is a 4-bit and represents the number of 32-bit words */
      /* HACK: Should check size of header is lower than 60 bytes? */
      fprintf(stderr , "ERR: Packet header size of %d is lower than 20.\n", iphdr->ip_hl);
      return -1;
    }

    /* Check if header has a correct checksum */
    printf("--------------------------\n");
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
     * 2.  If it is sent to one of the router’s IP addresses.(one of interfaces' IP addresses)  
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
      /* TODO: 1. LPM 2. Send_packet to next_hop */
    }

    /* -- end- 2.  If it is sent to one of the router’s IP addresses.(one of interfaces' IP addresses) -- */
    
    
    /* Lookup ARP Queue entries to find MAC addr of requested IP address. */
    struct sr_arpentry *sr_arpentry_copy = sr_arpcache_lookup(&sr->cache, a_hdr->ar_tip);
    if(sr_arpentry_copy){
      /* If exists, check if it is valid or not.*/
      if(sr_arpentry_copy->valid){
        /* If it is valid, send reply. */
        printf("Target IP address in ARP request, %d, exists in arp cache. Its MAC addr is %s \n", 
          a_hdr->ar_tip, sr_arpentry_copy->mac); /*XXX*/
      }
      else{
        /* If it is not valid, add a request into ARP Queue. */
        printf("Target IP address in ARP request, %d, exists in arp cache. Its MAC addr is %s. But it is not valid(%d). \n", 
          a_hdr->ar_tip, sr_arpentry_copy->mac, sr_arpentry_copy->valid); /*XXX*/
      }
  
    }else{
      /* If does not exist, Add a request into ARP Queue */
      printf("Target IP address in ARP request, %d, does not exist in arp cache.\n", a_hdr->ar_tip); /*XXX*/
    }
    

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
    /* -- If it is a request to me -- */
    /* Construct an ARP reply and send it back */
    /* ARP request is broadcast. operation code == 1
    * target MAC addr = ff-ff-ff-ff-ff-ff
    * target IP addr should be given.
    */
    if(a_hdr->ar_op == htons(arp_op_request)){
      printf("---ARP REQUST!---\n"); /*XXX*/
      /* If arp request is for the MAC addr of the current router itself, immediately send a reply with it.*/
      
      if(a_hdr->ar_tip == received_sr_if->ip){
        
        sr_print_if(received_sr_if); /* Debug*/
        struct in_addr ip_addr; /* Debug*/
        ip_addr.s_addr = a_hdr->ar_tip; /* Debug*/
        Debug("\ta_hdr->ar_tip: %s\n", inet_ntoa(ip_addr)); /* Debug*/

        /* 1. Copy the packet */
        uint8_t *copied_pkt = (uint8_t*)malloc(len);
        /*memset(copied_pkt, 0, len);*/
        memcpy(copied_pkt, packet, len);
        /* 2. Edit the ethernet destination and source MAC addresses, plus whatever fields of the packet are relevant*/
        sr_ethernet_hdr_t *copied_ehdr = (sr_ethernet_hdr_t *)copied_pkt;
        sr_arp_hdr_t *copied_a_hdr = (struct sr_arp_hdr*)(copied_pkt + sizeof(struct sr_ethernet_hdr));
        /* Ethernet header*/
        memcpy(copied_ehdr->ether_dhost, e_hdr->ether_shost, sizeof(uint8_t) * ETHER_ADDR_LEN);
        memcpy(copied_ehdr->ether_shost, received_sr_if->addr, sizeof(uint8_t) * ETHER_ADDR_LEN);
        Debug("Before ethertype: %d\n", ntohs(copied_ehdr->ether_type));
        /*copied_ehdr->ether_type = htons(ethertype_arp);*/
        Debug("After ethertype: %d\n", ntohs(copied_ehdr->ether_type));
        /* ARP header */
        copied_a_hdr->ar_op = htons(arp_op_reply);
        memcpy(copied_a_hdr->ar_sha, received_sr_if->addr, sizeof(unsigned char) * ETHER_ADDR_LEN);
        copied_a_hdr->ar_sip = received_sr_if->ip;
        memcpy(copied_a_hdr->ar_tha, a_hdr->ar_sha, sizeof(unsigned char) * ETHER_ADDR_LEN);
        copied_a_hdr->ar_tip = a_hdr->ar_sip;

        Debug("--- copied_pkt --- \n");
        print_hdrs(copied_pkt, len);/* Debug */

        /* 3. Send it. */
        sr_send_packet(sr, copied_pkt, len, interface);

        return;
      }
      else{
        /* If arp request is for the MAC addr of the current router itself, immediately send a reply with it.*/
        printf("ARP request for other IP address not in the current router.\n");
        return ;
      }
      
    }


    /* -- If it is a reply to me -- */
    /* Cache it, go through my request queue and send outstanding pakcets
     * (=fill out destination MAC addr of the raw Ethernet frame (in packets waiting on that packet)?) 
    */
    /* ARP reply is unicast. operation code == 2*/
    if(a_hdr->ar_op == htons(arp_op_reply)){
      printf("---ARP REPLY!---\n");
    }
  }
  

  

}/* end sr_ForwardPacket */

