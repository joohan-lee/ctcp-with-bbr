#!/bin/bash
if [ $1 == "-valgrind" ];
then
	echo "==================== Memory Test ======================"
	echo "cmd: screen -S sr -d -m valgrind --leak-check=yes --log-file=\"/tmp/sr_valgrind\" ./router/sr"
	screen -S sr -d -m valgrind --leak-check=yes --show-reachable=yes --log-file="/tmp/sr_valgrind" ./router/sr
elif [ $1 == "-normal" ];
then
	echo "==================== Normal Test ======================"
	echo "cmd: screen -S sr -d -m ./router/sr"
	screen -S sr -d -m ./router/sr
fi

./sr_expect >/dev/null

echo "router ready"

echo
echo "*********************************"
