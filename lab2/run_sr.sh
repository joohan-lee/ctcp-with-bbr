#!/bin/bash
screen -S pox -d -m ~/pox/pox.py cs144.ofhandler cs144.srhandler
expect pox_expect
screen -S sr -d -m ./sr