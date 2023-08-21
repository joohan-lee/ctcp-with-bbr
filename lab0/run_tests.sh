#!/bin/sh
set -e
sudo python ../tester/generic_tester.py bus.xml
sudo python ../tester/generic_tester.py star.xml
sudo python ../tester/generic_tester.py circle.xml
