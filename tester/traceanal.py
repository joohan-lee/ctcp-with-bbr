#!/usr/bin/python

"""
Packet tracing related functions
"""

# Import modules
import time
import os
import sys
import logging
import subprocess

from scapy.all import *

def start_tracing(net, athost):
    "Start tcpdump at specified host"

    filename = "/tmp/tcplog_" + athost
    node = net.get(athost)
    cmd = "tcpdump -i %s-eth0 -U -w %s &" % (node.name, filename)
    logging.info("*** Starting %s", cmd)
    node.cmdPrint(cmd)
    time.sleep(5) # Time for network to quiesce
    return filename

def stop_tracing(net, athost):
    "Kill tcpdump at specified host"

    logging.info("*** Stopping tcpdump at %s", athost)
    node = net.get(athost)
    node.cmdPrint("pkill -9 -f tcpdump")
    time.sleep(5) # Time for network to quiesce

def check_traceroute(fn, path):
    "Check if dump file fn contains the specified path"

    packets = rdpcap(fn)
    logging.info("*** Checking if traceroute matches %s", ' '.join(path))
    seen = {}
    seen_path = []
    # packets.summary()
    for packet in packets:
        if (packet.haslayer(ICMP)):
            # Look for time exceeded or port unreachable
            if ((packet[ICMP].type == 3 and packet[ICMP].code == 3) or packet[ICMP].type == 11):
                if (packet[IP].src not in seen.keys()):
                    seen_path.append(packet[IP].src)
                    logging.info("*** Found traceroute hop %s", packet[IP].src)
                    seen[packet[IP].src] = 1
    logging.info("*** Found path %s", ' '.join(seen_path))
    return True if (seen_path == path) else False

def check_host_unreach(fn):
    "Check if host unreachable found in dump file fn"

    packets = rdpcap(fn)
    logging.info("*** Checking host unreachable")
    count = 0
    for packet in packets:
        if (packet.haslayer(ICMP)):
            if (packet[ICMP].type == 3 and packet[ICMP].code == 1):
                count += 1
    logging.info("*** Found %d host unreachables", count)
    return True if (count > 0) else False

def check_net_unreach(fn):
    "Check if network unreachable found in dump file fn"

    packets = rdpcap(fn)
    logging.info("*** Checking network unreachable")
    count = 0
    for packet in packets:
        if (packet.haslayer(ICMP)):
            if (packet[ICMP].type == 3 and packet[ICMP].code == 0):
                count += 1
    logging.info("*** Found %d network unreachables", count)
    return True if (count > 0) else False

def check_ping(fn, f, t, c):
    "Check if file fn contains c pings from f to t"

    packets = rdpcap(fn)
    seen_request = False
    count = 0
    logging.info("*** Testing %d ping(s) from %s to %s", c, f, t)
    # packets.summary()
    for packet in packets:
        if (packet.haslayer(ICMP)):
            if (not seen_request):
                if (packet[IP].src == f and packet[IP].dst == t and packet[ICMP].type == 8):
                    seen_request = True
            else:
                if (packet[IP].dst == f and packet[IP].src == t and packet[ICMP].type == 0):
                    count += 1
                    seen_request = False
    logging.info("*** Saw %d ping(s) from %s to %s", count, f, t)
    return True if (count == c) else False
