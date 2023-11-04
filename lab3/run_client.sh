#!/bin/bash
# base
sudo ./ctcp -p 9999 -c localhost:8888

# log version
# sudo ./ctcp -p 9999 -c localhost:8888 -l

### Below examples are from README. ###

# To run a client with a window size multiple of 2
# (2 * MAX_SEG_DATA_SIZE = 2880 bytes), do the following:
# sudo ./ctcp -p 9999 -c localhost:8888 -w 2

# Connecting to a Web Server (Part 1a)
# sudo ./ctcp -p 9999 -c www.google.com:80

# Running Server with Application and Multiple Clients (Part 1b)
# - The server will start a new instance of the application each time a new client
#   connects. For example, to start two clients, which will start two instances
#   of an application on the server, do:
# sudo ./ctcp -c localhost:9999 -p 10000
# sudo ./ctcp -c localhost:9999 -p 10001

# Unreliability
# sudo ./ctcp -c localhost:8888 -p 12345 --drop 80
# sudo ./ctcp -c localhost:8888 -p 12345 --corrupt 100
# sudo ./ctcp -c localhost:8888 -p 12345 --delay 100
# sudo ./ctcp -c localhost:8888 -p 12345 --duplicate 100