"""
Create topologies for i2, EAST and WEST, and configure them.
"""

import inspect
import os
import sys

from mininext.topo import Topo
from mininext.services.quagga import QuaggaService

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

from netconfig import *

net = None

def ip_to_net(ip):
    "Convert IP address to /24 net"

    octets = ip.split('.')
    octets[3] = "0"
    return '.'.join(octets) + "/24"

class I2(Topo):
    "Creates a topology of Quagga routers, based on the Internet2 topology"

    def __init__(self):
        """Initialize a Quagga topology based on the topology information"""
        Topo.__init__(self)

        # Initialize a service helper for Quagga with default options
        self.quaggaSvc = QuaggaService(autoStop=False)

        # Path configurations for mounts
        self.quaggaBaseConfigPath = '/root/configs/'

        # Setup each Quagga router
        for router_name in nc_routers('i2'):
            info("\tAdding router %s\n" % (router_name))
            self.addHost(name=router_name,
                        hostname=router_name,
                        privateLogDir=True,
                        privateRunDir=True,
                        inMountNamespace=True,
                        inPIDNamespace=True,
                        inUTSNamespace=True)

            # Add a loopback interface with an IP in router's announced range
            self.addNodeLoopbackIntf(router_name, ip=nc_loopback(router_name))

            # Configure and setup the Quagga service for this node
            quaggaSvcConfig = \
                {'quaggaConfigPath': self.quaggaBaseConfigPath + router_name}
            self.addNodeService(node=router_name, service=self.quaggaSvc,
                                nodeConfig=quaggaSvcConfig)

            # print self.quaggaBaseConfigPath + router_name

        # Create the normal host connected to the routers
        for host_name in nc_hosts('i2'):
            info("\tAdding hosts %s\n" % (host_name))
            self.addHost(name=host_name,
                        hostname=host_name,
                        privateLogDir=True,
                        privateRunDir=True,
                        inMountNamespace=True,
                        inPIDNamespace=True,
                        inUTSNamespace=True)

def config_i2(net):
    "Configures interfaces and links on the I2 network"

    info('** Adding links to i2\n')
    for link in nc_links('i2'):
        info('\tAdding link from %s to %s\n' % (link['Node1'], link['Node2']))
        net.addLink(net.getNodeByName(link['Node1']), net.getNodeByName(link['Node2']), intfName1=link['Interface1'], intfName2=link['Interface2'])

    info('** Configuring interfaces on i2\n')
    for intfs in nc_interfaces('i2'):
        info('\tConfiguring interface %s on %s\n' % (intfs['interface'], intfs['node']))
        node = net.getNodeByName(intfs['node'])
        ifname = intfs['interface']
        node.cmd('ifconfig %s 0.0.0.0/0 up' % (ifname))

def add_west(topo):
    "Adds the WEST AS with a BR, a simple router and a client"

    info('** Adding WEST nodes\n')
    br = topo.addHost(name='west',
                    hostname='west',
                    privateLogDir=True,
                    privateRunDir=True,
                    inMountNamespace=True,
                    inPIDNamespace=True,
                    inUTSNamespace=True)
    sr = topo.addSwitch('sr0') # Switch name needs digit!
    client = topo.addHost('client')
    topo.addLink(client, sr, intfName2='sr-eth1', intfName1='client-eth0')
    topo.addLink(br, sr, intfName2='sr', intfName1='sr-eth2')

    # topo.addController('csr0', controller=RemoteController)

    info('** Adding WEST br quagga config\n')
    topo.addNodeLoopbackIntf('west', nc_loopback('west'))
    quaggaSvcConfig = \
        {'quaggaConfigPath': topo.quaggaBaseConfigPath + 'west'}
    topo.addNodeService(node='west', service=topo.quaggaSvc,
                        nodeConfig=quaggaSvcConfig)

def config_west(net):
    "Configures all nodes and interfaces in WEST"

    sr = net.getNodeByName('sr0')
    client = net.getNodeByName('client')
    br = net.getNodeByName('west')
    sea = net.getNodeByName('SEAT')
    net.addLink(br, sea, intfName1='seat', intfName2='west')

    ## Config interfaces on as5br and SEAT
    sea.cmd('ifconfig west 0.0.0.0/0 up')
    for intf in ['sr', 'seat']:
        br.cmd('ifconfig ' + intf + ' 0.0.0.0/0 up')

    ## Configure the client default route
    clintf = client.defaultIntf()
    clintf.setIP('%s/24' % (nc_interface('client', 'client-eth0')))

    client.cmd('route add %s/32 dev client-eth0' % (nc_interface('sr', 'sr-eth1')))
    client.cmd('route add default gw %s dev client-eth0' % (nc_interface('sr', 'sr-eth1')))
    client.cmd("iptables -A OUTPUT -p tcp --tcp-flags RST RST -j DROP") # For CTCP

    # Configure route on As5br towards client subnet
    br.cmd('route add -net %s gw %s dev sr' % (ip_to_net(nc_interface('client', 'client-eth0')), nc_interface('sr', 'sr-eth2')))

def add_east(topo):
    "Configure the EAST AS with 2 servers and a border router"

    info('** Adding AS6 hosts\n')
    s1 = topo.addHost(name='server1',
                    hostname='server1',
                    privateLogDir=True,
                    privateRunDir=True,
                    inMountNamespace=True,
                    inPIDNamespace=True,
                    inUTSNamespace=True)
    s2 = topo.addHost(name='server2',
                    hostname='server2',
                    privateLogDir=True,
                    privateRunDir=True,
                    inMountNamespace=True,
                    inPIDNamespace=True,
                    inUTSNamespace=True)
    br = topo.addHost(name='east',
                    hostname='east',
                    privateLogDir=True,
                    privateRunDir=True,
                    inMountNamespace=True,
                    inPIDNamespace=True,
                    inUTSNamespace=True)

    info('** Adding as6-br config\n')
    topo.addNodeLoopbackIntf('east', nc_loopback('east'))
    quaggaSvcConfig = \
        {'quaggaConfigPath': topo.quaggaBaseConfigPath + 'east'}
    topo.addNodeService(node='east', service=topo.quaggaSvc,
                        nodeConfig=quaggaSvcConfig)

def config_east(net):
    "Set up links and interfaces on border router and servers. Start HTTP servers elsewhere"

    s1 = net.getNodeByName('server1')
    s2 = net.getNodeByName('server2')
    br = net.getNodeByName('east')
    ny = net.getNodeByName('NEWY')
    net.addLink(s1, br, intfName1='east', intfName2="server1")
    net.addLink(s2, br, intfName1='east', intfName2="server2")
    net.addLink(br, ny, intfName1='newy', intfName2='east')
    # net.addController('csr0', controller=RemoteController)

    # Set up interfaces on border routers
    ny.cmd('ifconfig east 0.0.0.0/0 up')
    for intf in ['newy', 'server1', 'server2']:
        br.cmd('ifconfig ' + intf + ' 0.0.0.0/0 up')

    # Set up interfaces and default routes on hosts
    s1.cmd('ifconfig east %s/24 up' % (nc_interface('server1', 'east')))
    s1.cmd('route add default gw %s east' % (nc_interface('east', 'server1')))
    s1.cmd("iptables -A OUTPUT -p tcp --tcp-flags RST RST -j DROP") # For CTCP
    s2.cmd('ifconfig east %s/24 up' % (nc_interface('server2', 'east')))
    s2.cmd('route add default gw %s east' % (nc_interface('east', 'server2')))
    s2.cmd("iptables -A OUTPUT -p tcp --tcp-flags RST RST -j DROP") # For CTCP
