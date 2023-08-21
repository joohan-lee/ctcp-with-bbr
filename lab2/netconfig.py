#!/usr/bin/python

import sys
import os
import atexit
import argparse

import csv
import networkx as nx

ROUTERS = 'netinfo/routers.csv'
HOSTS = 'netinfo/hosts.csv'
LINKS = 'netinfo/links.csv'
ASNS = 'netinfo/asns.csv'

netGraph = None

def nc_loopback(router):
    "Get loopback address for router"

    with open(ROUTERS) as csvf:
        reader = csv.DictReader(csvf)
        for row in reader:
            if (row['Name'] == router):
                return row['Loopback']

def nc_routers(net):
    "Get all routers in the specified net"

    with open(ROUTERS) as csvf:
        reader = csv.DictReader(csvf)
        routers = []
        for row in reader:
            if (row['Net'] == net):
                routers.append(row['Name'])
    return routers

def nc_hosts(net):
    "Get all hosts in the specified net"

    with open(HOSTS) as csvf:
        reader = csv.DictReader(csvf)
        hosts = []
        for row in reader:
            if (row['Net'] == net):
                hosts.append(row['Name'])
    return hosts

def nc_links(net):
    "Get all links in the specified net. Returns a list of row dicts"

    with open(LINKS) as csvf:
        reader = csv.DictReader(csvf)
        links = []
        for row in reader:
            if (row['Net'] == net):
                links.append(row)
    return links

def nc_interfaces(net):
    "Get all interfaces in the specified net. Returns a list of link dicts"

    with open(LINKS) as csvf:
        reader = csv.DictReader(csvf)
        intfs = []
        for row in reader:
            if (row['Net'] == net):
                link1 = {'node': row['Node1'], 'interface': row['Interface1']}
                link2 = {'node': row['Node2'], 'interface': row['Interface2']}
                intfs.append(link1)
                intfs.append(link2)
    return intfs

def nc_interface(node, ifname):
    "Get the IP address of the specified interface on the specified node"

    with open(LINKS) as csvf:
        reader = csv.DictReader(csvf)
        for row in reader:
            if (row['Node1'] == node and row['Interface1'] == ifname):
                return row['Address1']
            if (row['Node2'] == node and row['Interface2'] == ifname):
                return row['Address2']
    return None

def nc_any_interface(node):
    """
    Get the IP address of any one interface on the specified node.
    """

    with open(LINKS) as csvf:
        reader = csv.DictReader(csvf)
        for row in reader:
            if (row['Node1'] == node):
                return row['Address1']
            if (row['Node2'] == node):
                return row['Address2']
    return None

def nc_reverse_lookup(addr):
    """
    Given an IP address, return the node and interface to which it belongs.
    """

    with open(LINKS) as csvf:
        reader = csv.DictReader(csvf)
        for row in reader:
            if (row['Address1'] == addr):
                return (row['Node1'], row['Interface1'])
            if (row['Address2'] == addr):
                return (row['Node2'], row['Interface2'])
    return None

def nc_reverse_path_lookup(path):
    """
    Given a path as a list of IP addresses, return a space-delimited string
    containing the corresponding sequence of node names.
    """

    lookup_path = list()
    for addr in path:
        (node, ifname) = nc_reverse_lookup(addr)
        lookup_path.append(node)
    return ' '.join(lookup_path)


def nc_interface_cost(node, ifname):
    "Get the interface cost of the specified interface on the specified node"

    with open(LINKS) as csvf:
        reader = csv.DictReader(csvf)
        for row in reader:
            if (row['Node1'] == node and row['Interface1'] == ifname):
                return int(row['Cost'])
            if (row['Node2'] == node and row['Interface2'] == ifname):
                return int(row['Cost'])
    return None

def nc_whichnet(router):
    "Get the net corresponding to the specified router"

    with open(ROUTERS) as csvf:
        reader = csv.DictReader(csvf)
        for row in reader:
            if (row['Name'] == router):
                return row['Net']

    return None

def nc_asnum(router):
    "Get the AS number for the given router"

    net = nc_whichnet(router)
    with open(ASNS) as csvf:
        reader = csv.DictReader(csvf)
        for row in reader:
            if (row['Net'] == net):
                return row['ASN']

    return None

def nc_init_netgraph():
    """
    Initialize the networkx graph structure to permit shortest path computations.
    """

    global netGraph
    netGraph = nx.Graph()
    with open(LINKS) as csvf:
        reader = csv.DictReader(csvf)
        for row in reader:
            if (row['Cost'] == ''):
                cost = 1
            else:
                cost = int(row['Cost'])
            netGraph.add_edge(row['Node1'], row['Node2'], weight=cost)
    return

def nc_path(source, dest):
    """
    Compute the path from the source node to the dest node.
    Return the result as a space delimited string
    """

    if (netGraph is None):
        nc_init_netgraph()

    ## Dijkstra returns a tuple, the first element is the cost, the second the path
    (cost, path) = nx.bidirectional_dijkstra(netGraph, source, dest, "weight")
    del path[0] # Remove the source
    return ' '.join(path)
