#!/usr/bin/python

import sys
import os
import atexit
import argparse
import textwrap
import time
import subprocess

from mininet.log import setLogLevel, info

sys.path.append(os.getcwd())

from mxanal import *

total_time=240
increment=15
network=sys.argv[1]

if __name__ == '__main__':
    setLogLevel('info')
    ctime = 0

    if (network=="multiAS"):
        while (ctime < total_time):
            time.sleep(increment)
            ctime += increment
            res_west = mx_check_ebgp_converged("west")
            res_east = mx_check_ebgp_converged("east")
            if (res_east and res_west):
                info("*** Routing has converged!\n")
                sys.exit(0)
            info("*** Time %d: no convergence yet\n" % (ctime))
        info("*** BGP didn't converge in %d seconds\n" % (total_time))
        sys.exit(1)

    if (network=="i2"):
        while (ctime < total_time):
            time.sleep(increment)
            ctime += increment
            res_west = (mx_count_ospf_routes("SEAT") == 22)
            res_east = (mx_count_ospf_routes("NEWY") == 22)
            if (res_east and res_west):
                info("*** Routing has converged!\n")
                sys.exit(0)
            info("*** Time %d: no convergence yet\n" % (ctime))
        info("*** OSPF didn't converge in %d seconds\n" % (total_time))
        sys.exit(1)
