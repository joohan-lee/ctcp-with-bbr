#!/bin/bash

########################################################
# Name: bbr_i2.sh
# Function: 
#   Starts four tmux sessions for clients and servers, and send files over I2 
#   then calculates the average throughput. 
# How to use:
#   1. Make sure you have tmux. otherwise run "sudo apt-get install tmux"
#   2. Go to lab2 folder and copy "file.txt" here 
#   3. In the same folder, run the script with "sudo ./bbr_i2.sh [551_HOME_PATH]",
#      [551_HOME_PATH] is your 551 repo folder's path, should be FULL path 
########################################################


HOME_FOLDER=${1%/}
echo "551 home folder: $HOME_FOLDER"

SERVER1_PORT=19997
SERVER2_PORT=19996

SERVER1_IP="6.0.2.2"
SERVER2_IP="6.0.3.2"

CLIENT1_PORT=8887
CLIENT2_PORT=8886

SRC_FILE1="file.txt"
SRC_FILE2="file.txt"
RAND_SUFFIX=$RANDOM
DST_FILE1="dst1_$RAND_SUFFIX.txt" 
DST_FILE2="dst2_$RAND_SUFFIX.txt"

echo "Starting I2 topology"
killall ovs-controller
killall screen
killall ctcp
mn -c
sh config.sh
tmux new -s i2 -d 
tmux send -t i2 "sudo python multiAS.py" ENTER
sleep 20

echo "Copy ctcp and sr to lab2 folder"
cp ../lab3/ctcp .
cp ../lab1/sr .

echo "Configuring I2..."
python load_configs_multiAS.py configs_multiAS
python config_i2_hosts.py
python ../tester/converged.py multiAS

echo "Starting pox and sr"
screen -S pox -d -m ~/pox/pox.py cs144.ofhandler cs144.srhandler
expect pox_expect
sleep 5
screen -S sr -d -m ./sr

echo "Starting server1"
tmux new -s server1 -d 
tmux send -t server1 "sudo ./go_to.sh server1" ENTER
tmux send -t server1 "cd $HOME_FOLDER/lab2" ENTER 
tmux send -t server1 "sudo ./ctcp -m -s -w 40 -p $SERVER1_PORT > $DST_FILE1" ENTER

echo "Starting server2"
tmux new -s server2 -d 
tmux send -t server2 "sudo ./go_to.sh server2" ENTER
tmux send -t server2 "cd $HOME_FOLDER/lab2" ENTER 
tmux send -t server2 "sudo ./ctcp -m -s -w 40 -p $SERVER2_PORT > $DST_FILE2" ENTER

sleep 10

echo "Starting client1"
tmux new -s client1 -d 
tmux send -t client1 "sudo ./go_to.sh client" ENTER
tmux send -t client1 "cd $HOME_FOLDER/lab2" ENTER
tmux send -t client1 "sudo ./ctcp -m -c $SERVER1_IP:$SERVER1_PORT -p $CLIENT1_PORT < $SRC_FILE1" ENTER

echo "Starting client2"
tmux new -s client2 -d 
tmux send -t client2 "sudo ./go_to.sh client" ENTER
tmux send -t client2 "cd $HOME_FOLDER/lab2" ENTER
tmux send -t client2 "sudo ./ctcp -m -c $SERVER2_IP:$SERVER2_PORT -p $CLIENT2_PORT < $SRC_FILE2" ENTER

START_TIME=$(($(date +%s)))
FILE_SIZE=$(($(stat --printf="%s" $SRC_FILE1)))
sleep 30

echo "Ending all nodes"
tmux kill-session -t client1
tmux kill-session -t client2
tmux kill-session -t server1
tmux kill-session -t server2
killall screen
killall ovs-controller
killall ctcp
tmux kill-session -t i2

# Verify files are the same
GOOD_FILE_1=$(($(cmp -s $SRC_FILE1 $DST_FILE1))) 
GOOD_FILE_2=$(($(cmp -s $SRC_FILE2 $DST_FILE2))) 

if [ $((GOOD_FILE_1+GOOD_FILE_2)) -gt 0 ] 
then 
    echo "FAILED: Files are different!"
    exit 1;
fi

# Calculate throughput 
SEND_TIME1=$(($(stat -c %Y $DST_FILE1)-START_TIME)) 
SEND_TIME2=$(($(stat -c %Y $DST_FILE2)-START_TIME)) 

echo "I2 Throughput: $(($FILE_SIZE * 16 / ($SEND_TIME1 + $SEND_TIME2))) bps"
