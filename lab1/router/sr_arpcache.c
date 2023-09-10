#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include "sr_arpcache.h"
#include "sr_router.h"
#include "sr_if.h"
#include "sr_protocol.h"

static volatile int keep_running_arpcache = 1;

/* 
  This function gets called every second. For each request sent out, we keep
  checking whether we should resend an request or destroy the arp request.
  See the comments in the header file for an idea of what it should look like.
*/
void sr_arpcache_sweepreqs(struct sr_instance *sr) { 
    /* Fill this in */
    Debug("\nsr_arpcache_sweepreqs called.\n");

    struct sr_arpreq *arpreq_pt = (struct sr_arpreq*)(sr->cache.requests);
    struct sr_arpreq *prev = 0;
    
    while(arpreq_pt){
        time_t curtime = time(NULL);
        /*
        printf("arpreq_pt->ip: \n");
        print_addr_ip_int(arpreq_pt->ip);
        printf("arpreq_pt->times_sent: %d\n",arpreq_pt->times_sent);
        printf("curtime: %ld\n",curtime);
        printf("arpreq_pt->sent: %ld\n",arpreq_pt->sent);
        printf("difftime(curtime, arpreq_pt->sent): %f\n",difftime(curtime, arpreq_pt->sent)); Debug */
        if(difftime(curtime, arpreq_pt->sent) < 1.0){
            arpreq_pt = arpreq_pt->next;
            continue;
        }
        
        if(arpreq_pt->times_sent >=5){
            /* Already sent request 5 times.
             * Destination host unreachable should go back to all the sender of packets
             * that were waiting on a reply to this ARP request.
             * Destination host unreachable (type 3, code 1) Sent after five ARP requests were sent to the next-hop IP 
             * without a response.
             */
            /* Copy the packet from sr_arpreq->packets to send ICMP message back to all senders of packets that were waiting on */
            uint32_t new_pkt_for_icmp_len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t3_hdr_t);
            uint8_t *org_pkt = (uint8_t*)(arpreq_pt->packets->buf);
            uint org_pkt_len = arpreq_pt->packets->len;
            uint8_t *new_pkt_for_icmp = (uint8_t*)malloc(new_pkt_for_icmp_len);
            memcpy(new_pkt_for_icmp, org_pkt, new_pkt_for_icmp_len);

            /* Ethernet header of new_pkt_for_icmp */
            sr_ethernet_hdr_t *new_pkt_e_hdr = (sr_ethernet_hdr_t*)new_pkt_for_icmp;
            sr_ethernet_hdr_t *org_pkt_e_hdr = (sr_ethernet_hdr_t*)org_pkt;

            /* Find the interface to send ICMP message using MAC addr of incoming iface 
             * This interface will be used to send ICMP message through.
             */
            struct sr_if *out_sr_if = NULL;
            struct sr_if *if_walker = sr->if_list;
            while(if_walker){
                if(memcmp(if_walker->addr, org_pkt_e_hdr->ether_dhost,ETHER_ADDR_LEN)==0){
                    out_sr_if = if_walker;
                    break;
                }
                if_walker = if_walker->next;
            }

            memcpy(new_pkt_e_hdr->ether_dhost, org_pkt_e_hdr->ether_shost, ETHER_ADDR_LEN); /* MAC addr of sender of packets that were waiting */
            memcpy(new_pkt_e_hdr->ether_shost, org_pkt_e_hdr->ether_dhost, ETHER_ADDR_LEN); 
            new_pkt_e_hdr->ether_type = htons(ethertype_ip);

            /* IP header of new_pkt_for_icmp */
            sr_ip_hdr_t *new_pkt_ip_hdr = (sr_ip_hdr_t*)(new_pkt_for_icmp + sizeof(sr_ethernet_hdr_t));
            sr_ip_hdr_t *org_pkt_ip_hdr = (sr_ip_hdr_t*)(org_pkt + sizeof(sr_ethernet_hdr_t));
            /*new_pkt_ip_hdr->ip_hl=(int)(sizeof(sr_ip_hdr_t) / 4);
            new_pkt_ip_hdr->ip_v=4;
            new_pkt_ip_hdr->ip_tos=0;*/
            new_pkt_ip_hdr->ip_len=htons(new_pkt_for_icmp_len - sizeof(sr_ethernet_hdr_t));
            new_pkt_ip_hdr->ip_id=0;
            new_pkt_ip_hdr->ip_off=0;
            new_pkt_ip_hdr->ip_ttl=64;
            new_pkt_ip_hdr->ip_p=ip_protocol_icmp;
            new_pkt_ip_hdr->ip_dst = org_pkt_ip_hdr->ip_src; /* dest = src of origin*/
            new_pkt_ip_hdr->ip_src = out_sr_if->ip; /* src = ip addr of current interface*/
            new_pkt_ip_hdr->ip_sum = 0;
            new_pkt_ip_hdr->ip_sum = cksum(new_pkt_ip_hdr, (new_pkt_ip_hdr->ip_hl)*4);

            /* ICMP header of new_pkt_for_icmp.
             * Destination host unreachable (type 3, code 1) */
            sr_icmp_t3_hdr_t *new_pkt_icmp_hdr = (sr_icmp_t3_hdr_t*)(new_pkt_for_icmp + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
            sr_icmp_t3_hdr_t *org_pkt_icmp_hdr = (sr_icmp_t3_hdr_t*)(org_pkt + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
            new_pkt_icmp_hdr->icmp_type=3; /* This must be 3 since it is ICMP type3(=Destination Unreachable). */
            new_pkt_icmp_hdr->icmp_code=1; /* Host Unreachable. */
            new_pkt_icmp_hdr->icmp_sum=0;
            new_pkt_icmp_hdr->icmp_sum = cksum(new_pkt_icmp_hdr, sizeof(sr_icmp_t3_hdr_t));

            /* if icmp message is Destination unreachable or Time exceeded, should fill data part with original IP(UDP) packet */
            copy_iphdr_and_data_for_icmp(new_pkt_icmp_hdr, org_pkt, org_pkt_len);
            

            printf("---HOST UNREACHABLE ICMP FRAME---\n");
            print_hdrs(new_pkt_for_icmp); /* Debug */

            
            /* Send ICMP Destination Host Unreachable */
            sr_send_packet(sr, new_pkt_for_icmp, new_pkt_for_icmp_len, out_sr_if->name);
            printf("ICMP Host unreachable packet sent!\n");

            /* Free req */
            free(new_pkt_for_icmp);
            sr_arpreq_destroy(&sr->cache, arpreq_pt);
            
        }else{
            /* Create ARP request packet */
            uint32_t arp_req_pkt_len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_arp_hdr_t);
            uint8_t *arp_req_pkt = (uint8_t*)malloc(arp_req_pkt_len);

            /* Ethernet header */
            struct sr_ethernet_hdr* arp_req_e_hdr = (struct sr_ethernet_hdr*)arp_req_pkt;
            struct sr_if *out_sr_if = sr_get_interface(sr, arpreq_pt->packets->iface);
            memset(arp_req_e_hdr->ether_dhost, 0xFF, ETHER_ADDR_LEN); /* Should broadcast*/
            memcpy(arp_req_e_hdr->ether_shost, out_sr_if->addr, sizeof(unsigned char) * ETHER_ADDR_LEN);
            arp_req_e_hdr->ether_type = htons(ethertype_arp);

            /*Debug("--- Created ethernet header in arp req packet \n");
            print_hdr_eth(arp_req_e_hdr); /* Debug*/

            /* ARP header */
            struct sr_arp_hdr* arp_req_a_hdr = (struct sr_arp_hdr*)(arp_req_pkt + sizeof(struct sr_ethernet_hdr));
            arp_req_a_hdr->ar_hrd=htons(arp_hrd_ethernet);
            arp_req_a_hdr->ar_pro=htons(ethertype_ip); /* IPv4 */
            arp_req_a_hdr->ar_hln = 6;
            arp_req_a_hdr->ar_pln = 4;
            arp_req_a_hdr->ar_op=htons(arp_op_request);
            memcpy(arp_req_a_hdr->ar_sha, out_sr_if->addr, sizeof(unsigned char) * ETHER_ADDR_LEN);
            arp_req_a_hdr->ar_sip=out_sr_if->ip;
            memset(arp_req_a_hdr->ar_tha, 0x00, ETHER_ADDR_LEN);
            arp_req_a_hdr->ar_tip=arpreq_pt->ip;

            /* Debug("--- Created arp header in arp req packet \n");
            print_hdr_arp(arp_req_a_hdr); /* Debug */

            /* Send ARP request */
            int send_res = sr_send_packet(sr, arp_req_pkt, arp_req_pkt_len, out_sr_if->name);
            free(arp_req_pkt);
            /* Update sent time in sr_arpreq */
            arpreq_pt->sent = curtime;
            
            /* Update times_sent in sr_arpreq */
            arpreq_pt->times_sent++;

        }

        /* Next node */
        arpreq_pt = arpreq_pt->next;
        
    }
    
}

/* You should not need to touch the rest of this code. */

/* Checks if an IP->MAC mapping is in the cache. IP is in network byte order.
   You must free the returned structure if it is not NULL. */
struct sr_arpentry *sr_arpcache_lookup(struct sr_arpcache *cache, uint32_t ip) {
    pthread_mutex_lock(&(cache->lock));
    
    struct sr_arpentry *entry = NULL, *copy = NULL;
    
    int i;
    for (i = 0; i < SR_ARPCACHE_SZ; i++) {
        if ((cache->entries[i].valid) && (cache->entries[i].ip == ip)) {
            entry = &(cache->entries[i]);
        }
    }
    
    /* Must return a copy b/c another thread could jump in and modify
       table after we return. */
    if (entry) {
        copy = (struct sr_arpentry *) malloc(sizeof(struct sr_arpentry));
        memcpy(copy, entry, sizeof(struct sr_arpentry));
    }
        
    pthread_mutex_unlock(&(cache->lock));
    
    return copy;
}

/* Adds an ARP request to the ARP request queue. If the request is already on
   the queue, adds the packet to the linked list of packets for this sr_arpreq
   that corresponds to this ARP request. You should free the passed *packet.
   
   A pointer to the ARP request is returned; it should not be freed. The caller
   can remove the ARP request from the queue by calling sr_arpreq_destroy. */
struct sr_arpreq *sr_arpcache_queuereq(struct sr_arpcache *cache,
                                       uint32_t ip,
                                       uint8_t *packet,           /* borrowed */
                                       unsigned int packet_len,
                                       char *iface)
{
    pthread_mutex_lock(&(cache->lock));
    
    struct sr_arpreq *req;
    for (req = cache->requests; req != NULL; req = req->next) {
        if (req->ip == ip) {
            break;
        }
    }
    
    /* If the IP wasn't found, add it */
    if (!req) {
        req = (struct sr_arpreq *) calloc(1, sizeof(struct sr_arpreq));
        req->ip = ip;
        req->next = cache->requests;
        cache->requests = req;
    }
    
    /* Add the packet to the list of packets for this request */
    if (packet && packet_len && iface) {
        struct sr_packet *new_pkt = (struct sr_packet *)malloc(sizeof(struct sr_packet));
        
        new_pkt->buf = (uint8_t *)malloc(packet_len);
        memcpy(new_pkt->buf, packet, packet_len);
        new_pkt->len = packet_len;
		new_pkt->iface = (char *)malloc(sr_IFACE_NAMELEN);
        strncpy(new_pkt->iface, iface, sr_IFACE_NAMELEN);
        new_pkt->next = req->packets;
        req->packets = new_pkt;
    }
    
    pthread_mutex_unlock(&(cache->lock));
    
    return req;
}

/* This method performs two functions:
   1) Looks up this IP in the request queue. If it is found, returns a pointer
      to the sr_arpreq with this IP. Otherwise, returns NULL.
   2) Inserts this IP to MAC mapping in the cache, and marks it valid. */
struct sr_arpreq *sr_arpcache_insert(struct sr_arpcache *cache,
                                     unsigned char *mac,
                                     uint32_t ip)
{
    pthread_mutex_lock(&(cache->lock));
    
    struct sr_arpreq *req, *prev = NULL, *next = NULL; 
    for (req = cache->requests; req != NULL; req = req->next) {
        if (req->ip == ip) {            
            if (prev) {
                next = req->next;
                prev->next = next;
            } 
            else {
                next = req->next;
                cache->requests = next;
            }
            
            break;
        }
        prev = req;
    }
    
    int i;
    for (i = 0; i < SR_ARPCACHE_SZ; i++) {
        if (!(cache->entries[i].valid))
            break;
    }
    
    if (i != SR_ARPCACHE_SZ) {
        memcpy(cache->entries[i].mac, mac, 6);
        cache->entries[i].ip = ip;
        cache->entries[i].added = time(NULL);
        cache->entries[i].valid = 1;
    }
    
    pthread_mutex_unlock(&(cache->lock));
    
    return req;
}

/* Frees all memory associated with this arp request entry. If this arp request
   entry is on the arp request queue, it is removed from the queue. */
void sr_arpreq_destroy(struct sr_arpcache *cache, struct sr_arpreq *entry) {
    pthread_mutex_lock(&(cache->lock));
    
    if (entry) {
        struct sr_arpreq *req, *prev = NULL, *next = NULL; 
        for (req = cache->requests; req != NULL; req = req->next) {
            if (req == entry) {                
                if (prev) {
                    next = req->next;
                    prev->next = next;
                } 
                else {
                    next = req->next;
                    cache->requests = next;
                }
                
                break;
            }
            prev = req;
        }
        
        struct sr_packet *pkt, *nxt;
        
        for (pkt = entry->packets; pkt; pkt = nxt) {
            nxt = pkt->next;
            if (pkt->buf)
                free(pkt->buf);
            if (pkt->iface)
                free(pkt->iface);
            free(pkt);
        }
        
        free(entry);
    }
    
    pthread_mutex_unlock(&(cache->lock));
}

/* Prints out the ARP table. */
void sr_arpcache_dump(struct sr_arpcache *cache) {
    fprintf(stderr, "\nMAC            IP         ADDED                      VALID\n");
    fprintf(stderr, "-----------------------------------------------------------\n");
    
    int i;
    for (i = 0; i < SR_ARPCACHE_SZ; i++) {
        struct sr_arpentry *cur = &(cache->entries[i]);
        unsigned char *mac = cur->mac;
        fprintf(stderr, "%.1x%.1x%.1x%.1x%.1x%.1x   %.8x   %.24s   %d\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], ntohl(cur->ip), ctime(&(cur->added)), cur->valid);
    }
    
    fprintf(stderr, "\n");
}

/* Initialize table + table lock. Returns 0 on success. */
int sr_arpcache_init(struct sr_arpcache *cache) {  
    /* Seed RNG to kick out a random entry if all entries full. */
    srand(time(NULL));
    
    /* Invalidate all entries */
    memset(cache->entries, 0, sizeof(cache->entries));
    cache->requests = NULL;
    
    /* Acquire mutex lock */
    pthread_mutexattr_init(&(cache->attr));
    pthread_mutexattr_settype(&(cache->attr), PTHREAD_MUTEX_RECURSIVE);
    int success = pthread_mutex_init(&(cache->lock), &(cache->attr));
    
    return success;
}

/* Destroys table + table lock. Returns 0 on success. */
int sr_arpcache_destroy(struct sr_arpcache *cache) {
    keep_running_arpcache = 0;
    return pthread_mutex_destroy(&(cache->lock)) && pthread_mutexattr_destroy(&(cache->attr));
}

/* Thread which sweeps through the cache and invalidates entries that were added
   more than SR_ARPCACHE_TO seconds ago. */
void *sr_arpcache_timeout(void *sr_ptr) {
    struct sr_instance *sr = sr_ptr;
    struct sr_arpcache *cache = &(sr->cache);
    
    while (keep_running_arpcache) {
        sleep(1.0);
        
        pthread_mutex_lock(&(cache->lock));
    
        time_t curtime = time(NULL);
        
        int i;    
        for (i = 0; i < SR_ARPCACHE_SZ; i++) {
            if ((cache->entries[i].valid) && (difftime(curtime,cache->entries[i].added) > SR_ARPCACHE_TO)) {
                cache->entries[i].valid = 0;
            }
        }
        
        sr_arpcache_sweepreqs(sr);

        pthread_mutex_unlock(&(cache->lock));
    }
    
    return NULL;
}

