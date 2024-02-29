#!/bin/bash

########################################################
# Name: bbr_mininet.sh
# Function: 
#   Starts four tmux sessions for clients and servers, and send files over mininet 
#   dumbbell topology then calculates the average throughput. 
# How to use:
#   1. Make sure you have tmux. otherwise run "sudo apt-get install tmux"
#   2. In one terminal, go to lab3 folder and run "sudo python lab3_topology.py"
#   3. In another terminal, go to lab3 folder and copy "file.txt" here 
#   4. In the same folder, run the script with "sudo ./bbr_mininet.sh [551_HOME_PATH]",
#      [551_HOME_PATH] is your 551 repo folder's path. 
########################################################


HOME_FOLDER=${1%/}

SERVER1_PORT=9096
SERVER2_PORT=9097

SERVER1_IP="12.0.1.1"
SERVER2_IP="12.0.2.1"

CLIENT1_PORT=9098
CLIENT2_PORT=9099

SRC_FILE1="file.txt"
SRC_FILE2="file.txt"
RAND_SUFFIX=$RANDOM
DST_FILE1="dst1_$RAND_SUFFIX.txt" 
DST_FILE2="dst2_$RAND_SUFFIX.txt"

echo "551 home folder: $HOME_FOLDER"
cd $HOME_FOLDER/lab3

echo "Starting mininet"
tmux new -s mnet -d 
tmux send -t mnet "sudo python lab3_topology.py" ENTER
sleep 20

echo "Starting server1"
tmux new -s server1 -d 
tmux send -t server1 "sudo ./go_to.sh server1" ENTER
tmux send -t server1 "cd $HOME_FOLDER/lab3" ENTER 
tmux send -t server1 "sudo ./ctcp -m -s -w 40 -p $SERVER1_PORT > $DST_FILE1" ENTER

echo "Starting server2"
tmux new -s server2 -d 
tmux send -t server2 "sudo ./go_to.sh server2" ENTER
tmux send -t server2 "cd $HOME_FOLDER/lab3" ENTER 
tmux send -t server2 "sudo ./ctcp -m -s -w 40 -p $SERVER2_PORT > $DST_FILE2" ENTER

sleep 5 

echo "Starting client1"
tmux new -s client1 -d 
tmux send -t client1 "sudo ./go_to.sh client1" ENTER
tmux send -t client1 "cd $HOME_FOLDER/lab3" ENTER
tmux send -t client1 "sudo ./ctcp -m -c $SERVER1_IP:$SERVER1_PORT -p $CLIENT1_PORT < $SRC_FILE1" ENTER

echo "Starting client2"
tmux new -s client2 -d 
tmux send -t client2 "sudo ./go_to.sh client2" ENTER
tmux send -t client2 "cd $HOME_FOLDER/lab3" ENTER
tmux send -t client2 "sudo ./ctcp -m -c $SERVER2_IP:$SERVER2_PORT -p $CLIENT2_PORT < $SRC_FILE2" ENTER

START_TIME=$(($(date +%s)))
FILE_SIZE=$(($(stat --printf="%s" $SRC_FILE1)))
sleep 30

echo "Ending all nodes"
tmux kill-session -t client1
tmux kill-session -t client2
tmux kill-session -t server1
tmux kill-session -t server2

dos2unix $DST_FILE1
dos2unix $DST_FILE2

# # Verify files are the same
# GOOD_FILE_1=$(($(cmp -s $SRC_FILE1 $DST_FILE1))) 
# GOOD_FILE_2=$(($(cmp -s $SRC_FILE2 $DST_FILE2))) 

# if [ $((GOOD_FILE_1+GOOD_FILE_2)) -gt 0 ] 
# then 
#     echo "FAILED: Files are different!"
#     exit 1;
# fi

echo "Running the first comp"
cmp $SRC_FILE1 $DST_FILE1
echo "Running the second comp"
cmp $SRC_FILE2 $DST_FILE2

# Calculate throughput 
SEND_TIME1=$(($(stat -c %Y $DST_FILE1)-START_TIME)) 
SEND_TIME2=$(($(stat -c %Y $DST_FILE2)-START_TIME)) 

echo "Dumbbell Throughput: $(($FILE_SIZE * 16 / ($SEND_TIME1 + $SEND_TIME2))) bps"
