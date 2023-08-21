#!/usr/bin/python

"""
Functions to run scripted commands
"""

# Import modules
import time
import os
import sys
import logging
import subprocess

from mininet.net import Mininet
from mininet.node import Controller, RemoteController
from mininet.log import setLogLevel, info
from mininet.cli import CLI
from mininet.topo import Topo

from scapy.all import *

import textwrap
from optparse import OptionParser

try:
    import xml.etree.cElementTree as ET
except ImportError:
    import xml.etree.ElementTree as ET

def run_commands(net, tree, t):
    "Run commands specified in the XML file. Tag t can have 3 values: init, startup, cleanup"

    for elem in tree.iter(tag=t):
        for e in elem.iter():
            if (e.tag == "run_command"):
                if (e.get("at")):
                    athost = e.get("at")
                    node = net.get(athost)
                    cmd = e.text.strip()
                    info("*** Running, at %s, cmd: %s\n" % (athost, cmd))
                    node.cmd(cmd)
                    if (e.get("sleep")):
                        info("....... Sleeping for %s seconds\n" % (e.get("sleep")))
                        time.sleep(int(e.get("sleep")))
                else:
                    cmd = e.text.strip()
                    info("*** Running cmd: %s\n" % (cmd))
                    subprocess.call(cmd, shell=True)
                    if (e.get("sleep")):
                        info("....... Sleeping for %s seconds\n" % (e.get("sleep")))
                        time.sleep(int(e.get("sleep")))
            if (e.tag == "set_default_intf"):
                node = net.get(e.get("at"))
                intf = node.defaultIntf()
                ip = e.text.strip()
                info("*** Reconfiguring default interface at %s with %s\n", node.name, ip)
                intf.setIP(ip)
