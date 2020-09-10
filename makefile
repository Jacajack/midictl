CC = cc
LIBS = -lcurses -lasound 
CFLAGS = -Wall --std=gnu99 -D_GNU_SOURCE -O2 $(LIBS)

SOURCES = src/midictl.c src/config_parser.c src/alsa.c src/args.c src/utils.c
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))
DEPENDS = $(patsubst %.c,%.d,$(SOURCES))

.PHONY: all clean

all: midictl

clean:
	rm -f $(DEPENDS) $(OBJECTS) midictl

midictl: $(OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@

-include $(DEPENDS)

%.o: %.c makefile
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@
