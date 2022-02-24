#!/bin/bash

echo -e "Starting midictl\n\nMidi output device list\n"

out_devs=$(aconnect -o)
echo "$out_devs"

echo -e "\nEnter client number"
read client

echo "Enter midi virtual port number"
read port

./midictl --device=$client --port=$port $1
