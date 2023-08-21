#!/usr/bin/python

"""
Script to run XML-defined tests automatically and output pass/fail.
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

## Our modules
script_path = os.path.abspath(os.path.dirname(__file__))
sys.path.append(script_path)
#sys.path.append(os.path.join(script_path, "../lab2/scripts"))
#sys.path.append(['../tester', '../../tester', '/../miniNExT'])
sys.path.append(os.getcwd())

from run_tests import run_tests
from run_commands import run_commands
from tester_utils import *
from traceanal import *

## Optparsing
import textwrap
from optparse import OptionParser

try:
    import xml.etree.cElementTree as ET
except ImportError:
    import xml.etree.ElementTree as ET

# Usage check
description = """Given the <xmlfile> file, run the tests
specified on the specified topology and out pass/fail
decisions"""

usage = "%prog [options] <xmlfile>\n" + '\n' + textwrap.fill(description)

parser = OptionParser(usage=usage)
parser.add_option("-q", "--quiet", default=False, action="store_true", dest="quiet", help="do not print info messages")
parser.add_option("-c", "--cli", default=False, action="store_true", dest="cli", help="run the CLI after starting network")
parser.add_option("-f", "--failstop", default=False, action="store_true", dest="failstop", help="stop if a test fails")
(options, args) = parser.parse_args();

if (len(args) < 1):
    parser.print_help()
    sys.exit()

if __name__ == '__main__':
    if (not options.quiet):
        setLogLevel('info')
        logging.basicConfig(stream=sys.stderr, level=logging.INFO)

    tree = ET.ElementTree(file=args[0])
    print_test_description(tree)

    run_commands(None, tree, "init")

    lab = which_lab(tree)
    if (lab is None):
        from generic_topo import generic_topology
        net = generic_topology(tree)
    if (lab == "lab0"):
        from lab0_topo import lab0_topology
        net = lab0_topology()
    if (lab == "lab1"):
        from lab1 import lab1_topology
        net = lab1_topology()
    if (lab == "lab1_ext"):
        from lab1 import lab1_extended_topology
        net = lab1_extended_topology()
    if (lab == "lab2_parta"):
        from network import lab2_topology
        net = lab2_topology(full=False)
    if (lab == "lab2_partb"):
        from network import lab2_topology
        net = lab2_topology(full=True)
    if (lab == "lab3_part1"):
        from lab3_topology import dumbbell_topology
        net = dumbbell_topology()

    run_commands(net, tree, "startup")

    # Check if we need to run CLI
    if (options.cli):
        CLI(net)
    else:
        (max, accrued) = run_tests(net, tree, options.failstop)

    info("\n****************** Stopping the network\n")
    net.stop()
    run_commands(None, tree, "cleanup")

    if 'max' in locals():
        info("\n****************** YOUR SCORE: (%d/%d)\n" % (accrued, max))
