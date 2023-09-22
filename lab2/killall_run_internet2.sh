#!/bin/bash
# sudo bash
killall ovs_controller # kill ovs controller process
mn -c # clear any existing Mininet network setups and clean up resources
python internet2.py