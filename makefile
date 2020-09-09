all:
	gcc --std=gnu99 -o midictl midictl.c args.c -Wall -lcurses -lasound 
