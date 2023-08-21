#!/bin/sh
set -e
sudo python ../tester/generic_tester.py base.xml
sudo python ../tester/generic_tester.py extended.xml
sudo python ../tester/generic_tester.py memtest.xml
