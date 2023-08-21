#!/usr/bin/python

import sys
import os
import atexit
import argparse
import textwrap

# patch isShellBuiltin
import mininet.util
import mininext.util
mininet.util.isShellBuiltin = mininext.util.isShellBuiltin
sys.modules['mininet.util'] = mininet.util

from mininet.util import dumpNodeConnections
from mininet.node import OVSController, RemoteController
from mininet.log import setLogLevel, info

from mininext.cli import CLI
from mininext.net import MiniNExT
from mininet.link import Link, TCLink

from subprocess import check_call

sys.path.append(os.getcwd())
from topology import *
from netconfig import *

from sets import Set

# Globals
net = None

def starthttp( host ):
    "Start simple Python web server on hosts"
    info( '*** Starting SimpleHTTPServer on host', host, '\n' )
    host.cmd( 'cd ./http_%s/; nohup python2.7 ./webserver.py &' % (host.name) )


def stophttp():
    "Stop simple Python web servers"
    info( '*** Shutting down stale SimpleHTTPServers',
          quietRun( "pkill -9 -f SimpleHTTPServer" ), '\n' )
    info( '*** Shutting down stale webservers',
          quietRun( "pkill -9 -f webserver.py" ), '\n' )

def startNetwork(full=False):
    "instantiates a topo, then starts the network and prints debug information"

    info('** Creating Quagga network topology\n')
    topo = I2()

    if (full):
        info("** Adding WEST and EAST\n")
        add_west(topo)
        add_east(topo)

    global net
    if (not full):
        net = MiniNExT(topo, controller=OVSController)
    else:
        net = MiniNExT(topo, controller=RemoteController)

    info('** Starting the network\n')
    net.start()

    config_i2(net)
    if (full):
        info("** Configuring WEST and EAST\n")
        config_west(net)
        config_east(net)

        info("** Starting HTTP servers on EAST\n")
        starthttp(net.getNodeByName('server1'))
        starthttp(net.getNodeByName('server2'))

    # info('** Dumping host connections\n')
    # dumpNodeConnections(net.hosts)
    #
    # info('** Dumping host processes\n')
    # for host in net.hosts:
    #     host.cmdPrint("ps aux")

    return net

def stopNetwork():
    "stops a network (only called on a forced cleanup)"

    if net is not None:
        info('** Tearing down Quagga network\n')
        net.stop()

def write_quagga_configs(dir_name, full=False):
    "write the quagga configs for all the routers in the topology, based on the skeleton"

    names = nc_routers('i2')
    if full:
        names = names + nc_routers('west') + nc_routers('east')

    for name in names:
        info("\tGenerating configs for %s\n" % (name))
        if not os.path.exists(dir_name+'/'+name):
            os.makedirs(dir_name+'/'+name)

        for f in ['daemons', 'debian.conf', 'vtysh.conf', 'zebra.conf', 'ospfd.conf', 'bgpd.conf']:
            with open(dir_name+'/EXAMPLE/'+f) as fd:
                skelet = fd.read()
                skelet = skelet.replace('NAME', 'G'+nc_asnum(name)+'_'+name)
                skelet = skelet.replace('127.0.0.1', nc_loopback(name))
            with open(dir_name+'/'+name+'/'+f, "w") as outfile:
                outfile.write(skelet)

def lab2_topology(full=False):
    "Return the topology for the lab. If full is set, add the west and east ASes."

    setLogLevel('info')
    write_quagga_configs('/root/configs', full)
    return startNetwork(full)
