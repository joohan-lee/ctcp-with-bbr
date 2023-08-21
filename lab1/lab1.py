#!/usr/bin/python

"""
Start up a Simple topology for CS551
"""

from mininet.net import Mininet
from mininet.node import Controller, RemoteController
from mininet.log import setLogLevel, info
from mininet.cli import CLI
from mininet.topo import Topo
from mininet.util import quietRun
from mininet.moduledeps import pathCheck

from sys import exit
import os.path
from subprocess import Popen, STDOUT, PIPE, check_call

IPBASE = '10.3.0.0/16'
ROOTIP = '10.3.0.100/16'
IPCONFIG_FILE = './IP_CONFIG'
IP_SETTING={}

class CS551Topo( Topo ):
    "CS 551 Lab 1 Topology"

    def __init__(self, ext):
        Topo.__init__(self)
        server1 = self.addHost( 'server1' )
        server2 = self.addHost( 'server2' )
        router = self.addSwitch( 'sw0' )
        client = self.addHost('client')
        for h in server1, server2, client:
            self.addLink( h, router )

        if ext:
            server3 = self.addHost( 'server3' )
            server4 = self.addHost( 'server4' )
            for h in server3, server4:
                self.addLink( h, server2 )

def startsshd( host ):
    "Start sshd on host"
    stopsshd()
    info( '*** Starting sshd\n' )
    name, intf, ip = host.name, host.defaultIntf(), host.IP()
    banner = '/tmp/%s.banner' % name
    host.cmd( 'echo "Welcome to %s at %s" >  %s' % ( name, ip, banner ) )
    host.cmd( '/usr/sbin/sshd -o "Banner %s"' % banner, '-o "UseDNS no"' )
    info( '***', host.name, 'is running sshd on', intf, 'at', ip, '\n' )


def stopsshd():
    "Stop *all* sshd processes with a custom banner"
    info( '*** Shutting down stale sshd/Banner processes ',
          quietRun( "pkill -9 -f Banner" ), '\n' )


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

def set_default_route(host):
    info('*** setting default gateway of host %s\n' % host.name)
    if(host.name == 'server1'):
        routerip = IP_SETTING['sw0-eth1']
    elif(host.name == 'server2'):
        routerip = IP_SETTING['sw0-eth2']
    elif(host.name == 'client'):
        routerip = IP_SETTING['sw0-eth3']
    print host.name, routerip
    host.cmd('route add %s/32 dev %s-eth0' % (routerip, host.name))
    host.cmd('route add default gw %s dev %s-eth0' % (routerip, host.name))
    ips = IP_SETTING[host.name].split(".")
    host.cmd('route del -net %s.0.0.0/8 dev %s-eth0' % (ips[0], host.name))

# Configure server2/3/4 interfaces and static routes
def configure_extended_topo(net):
    server2, server3, server4 = net.get('server2', 'server3', 'server4')

    # Server2 settings
    info('*** configuring the extended topology: server2\n')
    server2.cmd("ifconfig server2-eth1 0")
    server2.cmd("ifconfig server2-eth1 hw addr 00:00:00:00:01:01")
    server2.cmd("ip addr add %s/28 brd + dev server2-eth1" % (IP_SETTING['server2-eth1']))
    server2.cmd("route add %s/32 dev server2-eth1" % (IP_SETTING['server3-eth0']))
    server2.cmd("ifconfig server2-eth2 0")
    server2.cmd("ifconfig server2-eth2 hw addr 00:00:00:00:01:02")
    server2.cmd("ip addr add %s/28 brd + dev server2-eth2" % (IP_SETTING['server2-eth2']))
    server2.cmd("route add %s/32 dev server2-eth2" % (IP_SETTING['server4-eth0']))
    server2.cmd("echo 1 > /proc/sys/net/ipv4/ip_forward")

    # Server3 settings
    info('*** configuring the extended topology: server3\n')
    server3.cmd("ifconfig server3-eth0 0")
    server3.cmd("ifconfig server3-eth0 hw addr 00:00:00:00:02:01")
    intf = server3.defaultIntf()
    intf.setIP("%s/28" % (IP_SETTING['server3-eth0']))
    server3.cmd("route add default gw %s dev server3-eth0" % (IP_SETTING['server2-eth1']))

    # Server4 settings
    info('*** configuring the extended topology: server4\n')
    server4.cmd("ifconfig server4-eth0 0")
    server4.cmd("ifconfig server4-eth0 hw addr 00:00:00:00:02:02")
    intf = server4.defaultIntf()
    intf.setIP("%s/28" % (IP_SETTING['server4-eth0']))
    server4.cmd("route add default gw %s dev server4-eth0" % (IP_SETTING['server2-eth2']))

def get_ip_setting():
    try:
        with open(IPCONFIG_FILE, 'r') as f:
            for line in f:
                if( len(line.split()) == 0):
                  break
                name, ip = line.split()
                print name, ip
                IP_SETTING[name] = ip
            info( '*** Successfully loaded ip settings for hosts\n %s\n' % IP_SETTING)
    except EnvironmentError:
        exit("Couldn't load config file for ip addresses, check whether %s exists" % IPCONFIG_FILE)

def cs551net(ext=False):

    stophttp()
    "Create a simple network for cs551"
    get_ip_setting()
    topo = CS551Topo(ext)
    info( '*** Creating network\n' )
    net = Mininet( topo=topo, controller=RemoteController, ipBase=IPBASE )
    net.start()
    server1, server2, client, router = net.get( 'server1', 'server2', 'client', 'sw0')
    s1intf = server1.defaultIntf()
    s1intf.setIP('%s/8' % IP_SETTING['server1'])
    s2intf = server2.defaultIntf()
    s2intf.setIP('%s/8' % IP_SETTING['server2'])
    clintf = client.defaultIntf()
    clintf.setIP('%s/8' % IP_SETTING['client'])

    for host in server1, server2, client:
        set_default_route(host)
    if ext:
        configure_extended_topo(net)

    starthttp( server1 )
    starthttp( server2 )
    if ext:
        starthttp( net.get('server3') )
        starthttp( net.get('server4') )
    return net

def lab1_topology():
    return cs551net()

def lab1_extended_topology():
    return cs551net(ext=True)

if __name__ == '__main__':
    setLogLevel( 'info' )
    net = cs551net()
    CLI(net)
    stophttp()
    net.stop()
