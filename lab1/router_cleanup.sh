#!/bin/bash
for session in $(screen -ls | grep -o '[0-9]*\.sr'); do screen -S "${session}" -X stuff "^C"; done
