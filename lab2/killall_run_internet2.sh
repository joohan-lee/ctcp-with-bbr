#!/bin/bash
# This is to start internet2 topology which is used for lab2-a
# sudo bash
killall ovs_controller # kill ovs controller process
mn -c # clear any existing Mininet network setups and clean up resources
python internet2.py