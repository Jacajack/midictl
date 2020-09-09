all:
	gcc -o midictl midictl.c args.c -Wall -lcurses -lasound
