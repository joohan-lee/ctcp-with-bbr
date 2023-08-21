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
from network import lab2_topology, stopNetwork

if __name__ == '__main__':
    atexit.register(stopNetwork)

    net = lab2_topology(full=True)
    CLI(net)
