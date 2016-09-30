#!/bin/bash


PID=$(ps aux | grep "0.00 ./cli" | grep -v "grep" | awk -F " " '{print $2}')

kill -9 $PID
