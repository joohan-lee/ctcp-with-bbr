#!/usr/bin/python

"""
Functions to run commands on mininext nodes, and parse and analyze their
outputs. These are used for lab2 and lab3 tests.
"""

# Import modules
import time
import os
import sys
import logging
import subprocess
import re

from mininet.net import Mininet
from mininet.node import Controller, RemoteController
from mininet.log import setLogLevel, info
from mininet.cli import CLI
from mininet.topo import Topo

import textwrap
from optparse import OptionParser

def mx_run_command(node, cmd):
    """
    Run a cmd on on a Mininext node's shell and return the output
    """

    info("*** Running %s on node %s\n" % (cmd, node))
    res = subprocess.check_output(['./go_to.sh', node, '-c', cmd])
    return res

def mx_run_command_background(node, cmd):
    """
    Run a cmd on on a Mininext node's shell and return the output
    """

    info("*** Running %s on node %s in the background\n" % (cmd, node))
    subprocess.call(['./go_to.sh', node, '-c', cmd])
    return

def mx_run_vty_command(node, cmd):
    """
    Run a cmd on a Quagga shell on a Mininext node and return the output
    """

    cmd = 'vtysh -c \"%s\"' % (cmd)
    info("*** Running %s on node %s\n" % (cmd, node))
    res = subprocess.check_output(['./go_to.sh', node, '-c', cmd])
    return res

def mx_check_ebgp_converged(node):
    """
    Check if E-BGP has converged at a node
    This test is specific to our topology, where we expect 3 prefixes.
    """

    res = mx_run_vty_command(node, "show ip bgp")
    for line in res.split('\n'):
        if (line == 'Total number of prefixes 3'):
            return True
    return False

def mx_count_ospf_routes(node):
    """
    Count the number of OSPF routes at a node
    """

    res = mx_run_vty_command(node, "show ip ospf route")
    count = 0
    for line in res.split('\n'):
        if (line != "" and line[0] == 'N'):
            count += 1
    return count

def mx_hostifaddr(host, ifname):
    """
    Return the IP address of the interface ifname at nodes
    """

    regex = re.compile(r"inet addr:(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})")
    cmd = "ifconfig " + ifname
    res = mx_run_command(host, cmd)
    for line in res.splitlines():
        m = regex.search(line)
        if (m):
            ipaddr = m.group(1)
            return ipaddr
    return None

def mx_routerifaddr(router, ifname):
    """
    Return the IP address of the interface ifname at router
    """

    regex = re.compile(r"Internet Address (\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})")
    cmd = "show ip ospf interface " + ifname
    res = mx_run_vty_command(router, cmd)
    for line in res.splitlines():
        m = regex.search(line)
        if (m):
            ipaddr = m.group(1)
            return ipaddr
    return None

def mx_ifcost(router, ifname):
    """
    Return the link cost of the interface ifname at router
    """

    regex = re.compile(r"Cost: (\d+)")
    cmd = "show ip ospf interface " + ifname
    res = mx_run_vty_command(router, cmd)
    for line in res.splitlines():
        m = regex.search(line)
        if (m):
            cost = m.group(1)
            return int(cost)
    return None

def mx_path(source, dest):
    """
    Run a traceroute at source to dest, where dest is expected to be an IP addr.
    Parse the traceroute and return a list of IP addresses.
    """

    trace_path = list()
    regex = re.compile(r"\d+\s+(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})")
    cmd = "traceroute " + dest
    res = mx_run_command(source, cmd)
    for line in res.splitlines():
        ## info("*** mx_anal line: %s\n" % (line))
        m = regex.search(line)
        if (m):
            ## info("*** Found entry %s\n" % (m.group(1)))
            trace_path.append(m.group(1))
    return trace_path

def mx_ibgp_peer_count(router):
    """
    Return a count of the number of iBGP peers
    """

    count = 0
    regex = re.compile(r"internal link")
    cmd = "show ip bgp neighbors"
    res = mx_run_vty_command(router, cmd)
    for line in res.splitlines():
        m = regex.search(line)
        if (m):
            count += 1
    return count
