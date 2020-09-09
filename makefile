all:
	gcc -o midictl midictl.c -Wall -lcurses -lasound
