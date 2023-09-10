/*-----------------------------------------------------------------------------
 * File: sr_router.h
 * Date: ?
 * Authors: Guido Apenzeller, Martin Casado, Virkam V.
 * Contact: casado@stanford.edu
 *
 *---------------------------------------------------------------------------*/

#ifndef SR_ROUTER_H
#define SR_ROUTER_H

#include <netinet/in.h>
#include <sys/time.h>
#include <stdio.h>

#include "sr_protocol.h"
#include "sr_arpcache.h"

/* we dont like this debug , but what to do for varargs ? */
#ifdef _DEBUG_
#define Debug(x, args...) printf(x, ## args)
#define DebugMAC(x) \
  do { int ivyl; for(ivyl=0; ivyl<5; ivyl++) printf("%02x:", \
  (unsigned char)(x[ivyl])); printf("%02x",(unsigned char)(x[5])); } while (0)
#else
#define Debug(x, args...) do{}while(0)
#define DebugMAC(x) do{}while(0)
#endif

#define INIT_TTL 255
#define PACKET_DUMP_SIZE 1024

/* forward declare */
struct sr_if;
struct sr_rt;

/* ----------------------------------------------------------------------------
 * struct sr_instance
 *
 * Encapsulation of the state for a single virtual router.
 *
 * -------------------------------------------------------------------------- */

struct sr_instance
{
    int  sockfd;   /* socket to server */
    char user[32]; /* user name */
    char host[32]; /* host name */ 
    char template[30]; /* template name if any */
    unsigned short topo_id;
    struct sockaddr_in sr_addr; /* address to server */
    struct sr_if* if_list; /* list of interfaces */
    struct sr_rt* routing_table; /* routing table */
    struct sr_arpcache cache;   /* ARP cache */
    pthread_attr_t attr;
    FILE* logfile;
};

/* -- sr_main.c -- */
int sr_verify_routing_table(struct sr_instance* sr);

/* -- sr_vns_comm.c -- */
int sr_send_packet(struct sr_instance* , uint8_t* , unsigned int , const char*);
int sr_connect_to_server(struct sr_instance* ,unsigned short , char* );
int sr_read_from_server(struct sr_instance* );

/* -- sr_router.c -- */
void sr_init(struct sr_instance* );
void sr_handlepacket(struct sr_instance* , uint8_t * , unsigned int , char* );
int is_valid_ip_cksum(struct sr_ip_hdr*);
int is_valid_icmp_cksum(sr_icmp_hdr_t*);
void set_eth_hdr(sr_ethernet_hdr_t* ,
                  uint8_t dhost[ETHER_ADDR_LEN],
                  uint8_t shost[ETHER_ADDR_LEN],
                  uint16_t);
void set_ip_hdr(sr_ip_hdr_t *iphdr,
                  uint16_t ip_len,
                  uint8_t ip_p,
                  uint32_t ip_dst,
                  uint32_t ip_src 
                  );
void set_icmp_hdr(sr_icmp_hdr_t*, uint8_t, uint8_t);
void set_icmp3_hdr(sr_icmp_t3_hdr_t*, uint8_t, uint8_t, unsigned int);
void copy_iphdr_and_data_for_icmp(sr_icmp_t3_hdr_t*, uint8_t*, uint32_t);
int is_router_ip(struct sr_instance* sr, uint32_t dst_ip);
struct sr_rt* find_sr_rt_by_ip(struct sr_instance*, uint32_t);

/* -- sr_if.c -- */
void sr_add_interface(struct sr_instance* , const char* );
void sr_set_ether_ip(struct sr_instance* , uint32_t );
void sr_set_ether_addr(struct sr_instance* , const unsigned char* );
void sr_print_if_list(struct sr_instance* );

#endif /* SR_ROUTER_H */



/* DEFINE HEADER LENGTH */
#define ALL_HEADER_SIZE (sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_hdr_t))
#define ALL_HEADER_ICMP3_SIZE (sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t3_hdr_t))
#define ICMP_HDR_PTR(buf) ((sr_icmp_hdr_t*)(((uint8_t*)(buf)) + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t)))
#define ICMP3_HDR_PTR(buf) ((sr_icmp_t3_hdr_t*)(((uint8_t*)(buf)) + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t)))
#define ICMP_DATA_PTR(buf) ((uint8_t*)(((uint8_t*)(buf)) + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_hdr_t)))
#define ICMP3_DATA_PTR(buf) ((uint8_t*)(((uint8_t*)(buf)) + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t3_hdr_t)))

#define ICMP_UDP_LEN (ALL_HEADER_SIZE + 4 /* unused */ + (sizeof(sr_ip_hdr_t) + 8 /* Datagram's data size=64bit*/)) /* size of IP packet with UDP */
#define ICMP3_UDP_LEN (ALL_HEADER_ICMP3_SIZE + (sizeof(sr_ip_hdr_t) + 8 /* Datagram's data size=64bit*/) /* UDP packet */) /* size of IP packet with UDP */