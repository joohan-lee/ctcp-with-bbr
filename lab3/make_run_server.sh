#!/bin/bash
make clean
make
# base
sudo ./ctcp -s -p 8888
# log 
# sudo ./ctcp -s -p 8888 -l