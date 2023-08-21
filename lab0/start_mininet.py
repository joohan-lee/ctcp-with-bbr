import sys
import os
import atexit

from mininet.topo import Topo
from mininet.net import Mininet
from mininet.node import Controller, RemoteController, OVSSwitch
from mininet.log import setLogLevel, info
from mininet.link import TCLink
from mininet.cli import CLI

from topology import load_topology
import networkx as nx

def createMininetNetwork(G):
    """Create a local network with switches, one host connected to each switch, and one remote controller"""

    switches_tab = []

    net = Mininet(topo=None, build=None)

    print "*** Creating switches and hosts"
    #Creating the switches and the hosts
    for switch_id in G:
        dpid_tmp = hex(switch_id)[2:]
        dpid_tmp = '0' * (16 - len(dpid_tmp)) + dpid_tmp

        switches_tab.append(net.addSwitch(name='s'+str(switch_id), listenPort=6634, \
            dpid=dpid_tmp, failMode='secure'))
        net.addHost(name='h'+str(switch_id))

    # Creating links between switches, using the correct ports
    for from_switch, to_switch in G.edges():

        # Compute the port to use on each of the two switches
        port_from = G.node[from_switch]['ports'][str(to_switch)]
        port_to = G.node[to_switch]['ports'][str(from_switch)]

        # Create the links
        print 'Add Link '+str(from_switch)+'('+str(port_from)+')->('+str(port_to)+')'+str(to_switch)
        net.addLink(net.get('s'+str(from_switch)), net.get('s'+str(to_switch)), port1=port_from, port2=port_to)

    # Creating links between switches and hosts_tab
    for switch_id in G:
        port_from = G.node[switch_id]['ports']['host']

        print 'Add Link s'+str(switch_id)+'('+str(port_from)+')->(?)'+'h'+str(switch_id)
        net.addLink(net.get('s'+str(switch_id)), net.get('h'+str(switch_id)), port1=port_from)

    print "*** Creating remote controller"
    c1 = net.addController('c1', controller=RemoteController, port=6633)

    print "*** Starting network"
    net.build()
    c1.start()
    for s in switches_tab:
        s.start([c1])

    return net

if __name__ == '__main__':

    G = load_topology('topology.txt')

    net = createMininetNetwork(G)

    print "*** Running CLI"
    CLI(net)
