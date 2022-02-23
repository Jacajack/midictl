#!/bin/bash

if [ $# -eq 0 ]
  then
    echo "Please enter config file"
    exit 0
fi


echo -e "Starting midictl\n\nMidi output device list\n"

out_devs=$(aconnect -o)
echo "$out_devs"

echo -e "\nEnter client number"
read client

echo "Enter midi virtual port number"
read port

./midictl --device=$client --port=$port $1
