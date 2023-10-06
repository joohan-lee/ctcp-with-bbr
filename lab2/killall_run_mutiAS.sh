#!/bin/bash
# This is to start multiAS(WEST-internet2-EAST) topology which is used for lab2-b
# sudo bash
killall ovs_controller # kill ovs controller process
mn -c # clear any existing Mininet network setups and clean up resources
sh config.sh
python multiAS.py