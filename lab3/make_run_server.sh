#!/bin/bash
make clean
make
# base
sudo ./ctcp -s -p 8888

# log 
# sudo ./ctcp -s -p 8888 -l

### Below examples are from README. ###

# Running Server with Application and Multiple Clients (Part 1b)
# - To run a server that also runs an application, run the following command.
# - Each client that connects to the server will be associated with one instance
#   of the application (refer to the diagram for Lab 2).
# sudo ./ctcp -s -p 9999 -- sh