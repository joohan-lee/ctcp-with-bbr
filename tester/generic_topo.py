#!/usr/bin/python

"""
Setting up a mininet topology from an XML description. We don't use this, but it may be useful in the future.
"""

# Import modules
import time
import os
import sys
import logging
import subprocess

from mininet.net import Mininet
from mininet.node import Controller, RemoteController
from mininet.log import setLogLevel
from mininet.cli import CLI
from mininet.topo import Topo

from scapy.all import *

import textwrap
from optparse import OptionParser

try:
    import xml.etree.cElementTree as ET
except ImportError:
    import xml.etree.ElementTree as ET

# Global variables
net = None  # Mininet object
topo = Topo() # Empty topology
tree = None # Parse tree for XML
nodes = {} # Dict of all hosts and switches

# Functions

def setup_mininet(topo, tree):
    "Instantiate mininet"

    for e in tree.iter(tag="controller"):
        cname = e.text.strip()
        if (e.get("type") == "remote"):
            if (e.get("ipbase")):
                ipb = e.get("ipbase")
                logging.debug("*** Setting up mininet with remote controller %s and ipbase %s", cname, ipb)
                net = Mininet(topo=topo, controller=RemoteController, ipBase=ipb)
                # controller = net.addController(cname, controller=RemoteController, port=6633, ipBase=ipb)
            else:
                logging.debug("*** Setting up mininet with remote controller %s", cname)
                net = Mininet(topo=topo, controller=RemoteController)
        else:
            logging.debug("*** Setting up mininet with local controller %s", cname)
            net = Mininet(topo=topo, controller=controller)
    return net

def setup_switches(topo, tree):
    "Setting up the switches"

    for e in tree.iter(tag="switch"):
        sname = e.text.strip()
        logging.debug("*** Adding switch %s", sname)
        nodes[sname] = topo.addSwitch(sname)

def setup_hosts(topo, tree):
    "Set up the specified hosts"

    for e in tree.iter(tag="host"):
        hname = e.text.strip()
        logging.debug("*** Adding host %s", hname)
        if (e.get("ip")):
            if (e.get("inns")):
                nodes[hname] = topo.addHost(hname, ip=e.get("ip"), inNamespace=e.get("inns"))
            else:
                nodes[hname] = topo.addHost(hname, ip=e.get("ip"))
        else:
            if (e.get("inns") == "False"):
                nodes[hname] = topo.addHost(hname, inNamespace=False)
            else:
                nodes[hname] = topo.addHost(hname)

def setup_links(topo, tree):
    "Set up links in the topology"

    for e in tree.iter(tag="link"):
        links = e.text.strip()
        l = links.split()
        logging.debug("*** Adding link between %s and %s", l[0], l[1])
        e1 = nodes[l[0]]
        e2 = nodes[l[1]]
        topo.addLink(e1,e2)

def generic_topology(tree):
    "Parse the topology from the XML and set up various elements"

    global topo
    ## Set up various elements
    setup_switches(topo, tree)
    setup_hosts(topo, tree)
    setup_links(topo, tree)
    net = setup_mininet(topo, tree)
    net.start()
    return net
