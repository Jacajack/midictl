all:
	gcc --std=gnu99 -D_GNU_SOURCE -o midictl midictl.c args.c -Wall -lcurses -lasound 
