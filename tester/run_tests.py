#!/usr/bin/python

"""
Python module to run various test cases
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

from scapy.all import *

sys.path.append('.')
sys.path.append('../tester/')
sys.path.append('../lab2/')
from traceanal import *
from tester_utils import *
from netconfig import *
from mxanal import *
from random import randint

import textwrap
from optparse import OptionParser

try:
    import xml.etree.cElementTree as ET
except ImportError:
    import xml.etree.ElementTree as ET

## Global variable
stop_after_failure = False
tests_max_points = 0
tests_accrued_points = 0

def test_begin(e):
    "Print the test banner"
    # info("\n*********** TEST BEGIN: %s\n" % (ET.tostring(e).strip()))
    info("\n*********** TEST BEGIN: %s\n" % (e.tag))

## Print test result
def test_result(e, succ):
    "Print the test result"

    global stop_after_failure, tests_max_points, tests_accrued_points
    if (e.get("points")):
        points = int(e.get("points"))
    else:
        points = 10 	# Default points
    tests_max_points += points
    if (succ):
        tests_accrued_points += points
        info("*********** TEST END: %s: PASSED (%d points)!\n" % (e.tag, points))
    else:
        info("*********** TEST END: %s: FAILED!\n" % (e.tag))
        if (stop_after_failure):
            sys.exit(1)

# Get Whether the test should be graded or not
def get_grade_or_not(e):
    grade = e.get("grade")
    if grade is None:
	grade = 1
    grade = int(grade)
    return grade

## Run the tests
def run_tests(net, tree, stopf):
    "Parse and run the specified tests"

    global stop_after_failure
    stop_after_failure = stopf
    for elem in tree.iter("test_cases"):
        for e in elem.iter():
            if (e.tag == "ping"):
                test_begin(e)
                links = e.text.strip()
                l = links.split()
                info("*** Running ping between %s and %s\n" % (l[0], l[1]))
                h1 = net.get(l[0])
                h2 = net.get(l[1])
                result = net.ping([h1, h2])
		grade = get_grade_or_not(e)
		if grade:
                    test_result(e, (result == 0))

	    if (e.tag == "pingtoip"):
                test_begin(e)
                links = e.text.strip()
                (f, tip, n) = links.split()
                fn = start_tracing(net, f)
                hf = net.get(f)
                info("*** Running ping from %s to %s\n" % (hf.IP(), tip))
                hf.cmdPrint("ping -c %s %s" % (n, tip))
                time.sleep(5)
                stop_tracing(net, f)
		grade = get_grade_or_not(e)
		if grade:
                    result = check_ping(fn, hf.IP(), tip, int(n))
                    test_result(e, result)

            if (e.tag == "iperf"):
                test_begin(e)
                links = e.text.strip()
                l = links.split()
                h1 = net.get(l[0])
                h2 = net.get(l[1])
                info("*** Running iperf between %s and %s\n" % (h1.IP(), h2.IP()))
                bw1, bw2 = net.iperf([h1, h2])
                bw = float((bw1.split())[0])
		grade = get_grade_or_not(e)
		if grade:
                    expected_bw = float(e.get("expect"))
                    succ = (bw >= 0.8 * expected_bw)
                    test_result(e, succ)

            if (e.tag == "pingall"):
                test_begin(e)
                info("*** Running pingall")
                result = net.pingAll()
		grade = get_grade_or_not(e)
		if grade:
                    test_result(e, (result == 0))

            if (e.tag == "traceroute"):
                test_begin(e)
                links = e.text.strip()
                (f, t) = links.split()
                fn = start_tracing(net, f)
                hf = net.get(f)
                ht = net.get(t)
                info("*** Running traceroute from %s to %s\n" % (hf.IP(), ht.IP()))
                hf.cmdPrint("traceroute -z 1 %s" % (ht.IP()))
                time.sleep(5)
                stop_tracing(net, f)
		grade = get_grade_or_not(e)
		if grade:
                    expected_path = e.get("expect")
                    result = check_traceroute(fn, expected_path.split())
                    test_result(e, result)

            if (e.tag == "traceroutetoip"):
                test_begin(e)
                links = e.text.strip()
                (f, tip) = links.split()
                fn = start_tracing(net, f)
                hf = net.get(f)
                info("*** Running traceroute from %s to %s\n" % (hf.IP(), tip))
                hf.cmdPrint("traceroute -z 1 %s" % (tip))
                time.sleep(5)
                stop_tracing(net, f)
		grade = get_grade_or_not(e)
		if grade:
                    expected_path = e.get("expect")
                    result = check_traceroute(fn, expected_path.split())
                    test_result(e, result)

            if (e.tag == "pingdestunreach"):
                test_begin(e)
                links = e.text.strip()
                (f, t) = links.split()
                fn = start_tracing(net, f)
                hf = net.get(f)
                ht = net.get(t)
                info("*** Running ping from %s to %s\n" % (hf.IP(), ht.IP()))
                hf.cmdPrint("ping -c 3 %s" % (ht.IP()))
                time.sleep(5)
		grade = get_grade_or_not(e)
		if grade:
                    stop_tracing(net, f)
                    result = check_host_unreach(fn)
                    test_result(e, result)

            if (e.tag == "pingnetunreach"):
                test_begin(e)
                links = e.text.strip()
                (f, tip) = links.split()
                fn = start_tracing(net, f)
                hf = net.get(f)
                info("*** Running ping from %s to %s\n" % (hf.IP(), tip))
                hf.cmdPrint("ping -c 3 %s" % (tip))
                time.sleep(5)
		grade = get_grade_or_not(e)
		if grade:
                    stop_tracing(net, f)
                    result = check_net_unreach(fn)
                    test_result(e, result)

            if (e.tag == "wget"):
                test_begin(e)
                hat = net.get(e.get("at"))
                hf = net.get(e.get("from"))
                file = e.text.strip()
                info("*** Downloading %s from %s at %s\n" % (file, hf.IP(), hat.IP()))
                hat.cmdPrint("wget -O /tmp/foo http://%s/%s" % (hf.IP(), file))
                ofile = "./http_%s/%s" % (hf.name, file)
		grade = get_grade_or_not(e)
		if grade:
                    info("*** Comparing md5 sum of %s and %s\n" % ('/tmp/foo', ofile))
                    test_result(e, compare_files(['/tmp/foo', ofile]))

            if (e.tag == "valgrind"):
                test_begin(e)
                fname = e.text.strip()
                info("*** Checking for memory leaks\n")
                not_found = True
                for line in open(fname, 'r'):
                    if (re.search('definitely lost', line)):
                        not_found = False
		grade = get_grade_or_not(e)
		if grade:
                    test_result(e, not_found)

	    if (e.tag == "linkdown"):		
                hat = net.get(e.get("at"))
		link = e.get("link")
                info("*** tearing down a link\n")
                hat.cmdPrint("ifconfig %s down" %link)
		time.sleep(5)

	    if (e.tag == "routercleanup"):
		info("***running command: ./router_cleanup.sh \n")
                subprocess.call(["sh", "router_cleanup.sh"])
		time.sleep(5)
		

            #### Below this line are tests for MiniNeXT
            if (e.tag == "hostif"):
                test_begin(e)
                node = e.text.strip()
                host = node + "-host"
                ifname = node.lower()
                info("*** Checking if interface %s configured on host %s\n" % (ifname, host))
                expected_if = nc_interface(host, ifname)
                actual_if = mx_hostifaddr(host, ifname)
                info("*** Expected ifaddr %s, got %s\n" % (expected_if, actual_if))
                test_result(e, (expected_if == actual_if))

            if (e.tag == "routerif"):
                test_begin(e)
                line = e.text.strip()
                (router, ifname) = line.split(' ')
                info("*** Check if interface %s is configured on router %s\n" % (router, ifname))
                expected_if = nc_interface(router, ifname)
                actual_if = mx_routerifaddr(router, ifname)
                info("*** Expected ifaddr %s, got %s\n" % (expected_if, actual_if))
                test_result(e, (expected_if == actual_if))

            if (e.tag == "linkcost"):
                test_begin(e)
                line = e.text.strip()
                (router, ifname) = line.split(' ')
                info("*** Check if link cost on interface %s at router %s is correctly configured\n" % (router, ifname))
                expected_ifcost = nc_interface_cost(router, ifname)
                actual_ifcost = mx_ifcost(router, ifname)
                info("*** Expected interface cost %s, got %s\n" % (expected_ifcost, actual_ifcost))
                test_result(e, (expected_ifcost == actual_ifcost))

            if (e.tag == "path"):
                test_begin(e)
                line = e.text.strip()
                (source, dest) = line.split(' ')
                time.sleep(1) ## This helps avoid some test failures
                info("*** Check path from %s to %s\n" % (source, dest))
                expected_path = nc_path(source, dest)
                actual_path = nc_reverse_path_lookup(mx_path(source, nc_any_interface(dest)))
                info("*** Expected path %s, got %s\n" % (expected_path, actual_path))
                test_result(e, (expected_path == actual_path))

            if (e.tag == "ibgp"):
                test_begin(e)
                router = e.text.strip()
                info("*** Check if iBGP at Internet2 router %s is correctly configured\n" % (router))
                count = mx_ibgp_peer_count(router)
                info("*** Expected %d iBGP neighbors, got %d\n" % (8, count))
                test_result(e, (count == 8))

            if (e.tag == "wgetmx"):
                test_begin(e)
                hat = e.get("at")
                hf = e.get("from")
                hfIP = nc_any_interface(hf)
                file = e.text.strip()
                info("*** Downloading %s from %s at %s\n" % (file, hf, hat))
                res = mx_run_command(hat, "wget -O /tmp/foo http://%s/%s" % (hfIP, file))
                ofile = "./http_%s/%s" % (hf, file)
                info("*** Comparing md5 sum of %s and %s\n" % ('/tmp/foo', ofile))
                test_result(e, compare_files(['/tmp/foo', ofile]))

            ### Below this line are CTCP tests
            if (e.tag == "cget"):

                # Parse options
                if (e.get("file")):
                    filename = e.get("file")
                else:
                    filename = "reference"
                info("*** Copying file %s to /tmp\n" % (filename))
                subprocess.call(["cp", filename, "/tmp/"])
                filename = "/tmp/" + filename

                if (e.get("loss")):
                    (rate, link) = e.get("loss").split(' ')
                    (router, intf) = link.split(':') # Assumption on interface naming
                    node = net.getNodeByName(router)
                    info("*** Setting loss rate of %f at %s interface %s\n" % (float(rate), router, intf))
                    node.cmd("tc qdisc add dev %s root netem %f%s" % (intf, float(rate), '%'))
                links = e.text.strip().split(' ')

                if (e.get("window")):
                    info("*** Setting window size %s\n" % (e.get("window")))
                    ctcp_opt = "-w " + e.get("window")
                else:
                    ctcp_opt = ""

                val_prefix_client = ""
                val_prefix_server = ""
                if (e.get("memcheck")):
                    info("*** Checking for memory leaks\n")
                    val_file_client = e.get("memcheck") + "_client"
                    val_file_server = e.get("memcheck") + "_server"
                    val_prefix_client = "valgrind --leak-check=full --log-file=%s " % (val_file_client)
                    val_prefix_server = "valgrind --leak-check=full --log-file=%s " % (val_file_server)

                # Parse node pairs
                clients = list()
                servers = list()
                serverIPports = list()
                for link in links:
                    (node1, node2) = link.split(':')
                    clients.append(node1)
                    servers.append(node2)

                # Start servers
                for s in servers:
                    (node, port) = s.split('!')
                    server = net.getNodeByName(node)
                    sp = mx_hostifaddr(node, str(server.defaultIntf())) + ':' + port
                    serverIPports.append(sp)
                    info("*** Starting ctcp server on node %s IP:port %s\n" % (node, sp))
                    cmd = os.getcwd() + "/" + "ctcp -m -s -p %s -- cat %s 2> /dev/null &" % (port, filename)
                    info("*** Cmd: %s\n" % (cmd))
                    # server.cmd(cmd)
                    mx_run_command_background(node, val_prefix_server + cmd)

                time.sleep(5)   # Wait for servers to start

                for c in clients:
                    (node, port) = c.split('!')
                    received_filename = filename + "_" + node
                    client = net.getNodeByName(node)
                    servport = serverIPports.pop(0)
                    info("*** Starting ctcp client on node %s port %s to server %s\n" % (node, port, servport))
                    cmd = os.getcwd() + "/" + "ctcp -m -c %s -p %s %s > %s 2> /dev/null &" % (servport, port, ctcp_opt, received_filename)
                    info("*** Cmd: %s\n" % (cmd))
                    # client.cmd(cmd)
                    mx_run_command_background(node, val_prefix_client + cmd)

                time.sleep(15)   # Wait for clients to finish

                if (val_prefix_client == ""): # We are not checking for memleaks
                    result = True
                    info("*** Checking result\n")
                    for c in clients:
                        (node, port) = c.split('!')
                        received_filename = filename + "_" + node
                        result = result and compare_files([filename, received_filename])
                        test_result(e, result)
                else:
                    not_found = True
                    for line in open(val_file_client, 'r'):
                        if (re.search('definitely lost', line)):
                            info("*** Memory leak found in ctcp client code, see %s" % (val_file_client))
                            not_found = False
                    for line in open(val_file_server, 'r'):
                        if (re.search('definitely lost', line)):
                            info("*** Memory leak found in ctcp server code, see %s" % (val_file_server))
                            not_found = False
                    test_result(e, not_found)

                ## Cleanup after tests
                info("*** Cleaning up after test\n")
                subprocess.call(['killall', 'ctcp'])

        return (tests_max_points, tests_accrued_points)
