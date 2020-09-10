DEBUG ?= 0
RELEASE ?= 0
CC = cc
LIBS = -lcurses -lasound 
CFLAGS = -Wall --std=gnu99 -D_GNU_SOURCE

ifneq ($(DEBUG),0)
$(warning Compiling in debug mode!)
CC = clang
CFLAGS += -fsanitize=address -fsanitize=undefined -DDEBUG -O0 -g -fno-builtin
endif

ifneq ($(RELEASE),0)
CFLAGS += -DNDEBUG -O2 -s
endif

SOURCES = src/midictl.c src/config_parser.c src/alsa.c src/args.c src/utils.c
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))
DEPENDS = $(patsubst %.c,%.d,$(SOURCES))

.PHONY: all clean

all: midictl

clean:
	rm -f $(DEPENDS) $(OBJECTS) midictl

midictl: $(OBJECTS)
	$(CC) $(CFLAGS) $(LIBS) $^ -o $@

-include $(DEPENDS)

%.o: %.c makefile
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@
