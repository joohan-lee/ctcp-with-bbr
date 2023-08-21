#!/usr/bin/python

"""
Various utility functions for the tests
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
import hashlib

import textwrap
from optparse import OptionParser

try:
    import xml.etree.cElementTree as ET
except ImportError:
    import xml.etree.ElementTree as ET

def print_test_description(tree):
    "Print the test description specified in the XML"

    for e in tree.iter(tag="description"):
        print e.text

def which_lab(tree):
    "Extract the lab tag for the test (used to obtain the lab topology)"

    for e in tree.iter(tag="test"):
        return e.get("lab")

def compare_files(f):
    "Compare the hash digests of files to ensure correct download"

    digests = []
    for filename in f:
        hasher = hashlib.md5()
        with open(filename, 'rb') as f:
            buf = f.read()
            hasher.update(buf)
            a = hasher.hexdigest()
            info("*** Hex digest for %s is %s\n" % (filename, a))
            digests.append(a)

    return (digests[0] == digests[1])
