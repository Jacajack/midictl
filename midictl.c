#include "midictl.h"
#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <argp.h>
#include <alsa/asoundlib.h>
#include "args.h"

#define MIN(a, b) ((a) <= (b) ? (a) : (b))
#define MAX(a, b) ((a) >= (b) ? (a) : (b))
#define CLAMP(x, min, max) (MAX(MIN(x, (max)), (min)))

/**
	Menu entry (controller) type
*/
typedef enum controller_type
{
	CTL_VALUE,
	CTL_RULE
} controller_type;

/**
	\brief MIDI contoller - an entry in the main menu
*/
typedef struct midi_ctl
{
	controller_type type;
	char *name;
	int id;
	int value;
} midi_ctl;

/**
	Sets a value for MIDI controller
*/
void midi_ctl_set(midi_ctl *ctl, int v)
{
	ctl->value = CLAMP(v, 0, 127);
}

/**
	Draws a lame slider
*/
void draw_slider_ctl(WINDOW *win, int y, int x, int w, int v, int max)
{
	// There's no space for the value...
	if (w < 7)
	{
		if (w > 0)
			mvprintw(y, x, "%*d", w, v);
	}
	else
	{
		int lw; // Label width
		mvprintw(y, x, "%3d [%n", v, &lw);

		int sw = w - lw - 1; // Slider width
		int blocks = (float)v / max * sw;
		mvhline(y, x + lw, 0, blocks);
		mvprintw(y, x + lw + sw, "]");
	}
}

/**
	The main draw function
*/
void draw_ctls(WINDOW *win, midi_ctl *ctls, int count, int offset, int active, float split_pos)
{
	int win_w, win_h;
	getmaxyx(win, win_h, win_w);
	
	int col[3];   // Column positions
	int colw[3];  // Column widths
	int split[2]; // Splitter positions

	// Left column is fixed width
	col[0] = 0;
	colw[0] = 4;
	split[0] = col[0] + colw[0];

	// Middle column takes 0.5 of the space left	
	col[1] = split[0] + 2;
	colw[1] = (win_w - colw[0] + 1) * CLAMP(split_pos, 0.1f, 0.9f);
	split[1] = col[1] + colw[1];

	// The right column takes the rest
	col[2] = split[1] + 1;
	colw[2] = win_w - col[2];

	for (int y = 0; y < win_h; y++)
	{
		int i = y + offset;
		int valid = i < count && i >= 0;
		midi_ctl *ctl = &ctls[i];

		if (!valid) continue;

		// Invert if selected and draw
		// background for the left and middle column
		if (i == active)
		{
			attron(A_REVERSE);
			mvprintw(y, col[0], "%*s", split[1], "");
		}

		// Left and middle columns
		if (ctl->type == CTL_VALUE)
		{
			mvprintw(y, col[0], "%3d", ctl->id);

			mvprintw(y, col[1], "%.*s", colw[1], ctl->name);
			int len = strlen(ctl->name);
			int left = colw[1] - len;
			if (left > 0)
				mvprintw(y, col[1] + len, "%*s", left, "");
		}

		attroff(A_REVERSE);

		if (ctl->type == CTL_VALUE)
		{
			draw_slider_ctl(win, y, col[2], colw[2], ctl->value, 127);
		}
		else if (ctl->type == CTL_RULE)
		{
			mvhline(y, 0, 0, win_w);

			if (ctl->name)
			{
				int len = strlen(ctl->name);
				int x = split[1] - len - 3;
				
				if (x > split[0] && len + 2 < colw[1])
				{
					move(y, x);
					addch(ACS_RTEE);
					attron(A_REVERSE);
					printw("%s", ctl->name);
					attroff(A_REVERSE);
					addch(ACS_LTEE);
				}
			}
		}
	}

	// Draw splits
	mvvline(0, split[0], 0, win_h);
	mvvline(0, split[1], 0, win_h);

	// Draw crosses where rules are
	for (int y = 0; y < win_h; y++)
	{
		int i = y + offset;
		int valid = i < count && i >= 0;
		midi_ctl *ctl = &ctls[i];
		
		if (!valid || ctl->type != CTL_RULE) continue;

		move(y, split[0]);
		addch(ACS_PLUS);
		move(y, split[1]);
		addch(ACS_PLUS);
	}
}

/**
	\returns whether the string is only whitespace
*/
int isempty(const char *s)
{
	while (*s)
		if (!isspace(*s++))
			return 0;
	return 1;
}

/**
	\todo Fix memory leaks when handling errors
*/
midi_ctl *load_ctls_from_file(FILE *f, int *count)
{
	// Allocate memory for some controllers
	const int max_ctls = 512;
	midi_ctl *ctls = calloc(max_ctls, sizeof(midi_ctl));
	int ctl_count = 0;
	
	// Parse controller data
	char *line = NULL;
	size_t len = 0;
	for (int line_number = 1; ctl_count < max_ctls && getline(&line, &len, f) > 0; line_number++)
	{
		// Remove the newline character
		line[--len] = 0;

		// Ignore comments and empty lines
		if (isempty(line) || line[0] == '#') continue;

		midi_ctl *ctl = &ctls[ctl_count];

		// Lines starting with --- are horizontal rules/headers
		if (strstr(line, "---") == line)
		{
			ctl->type = CTL_RULE;
			ctl->name = NULL;
			sscanf(line, "---%*[ \t]%ms", &ctl->name);
			ctl_count++;
			continue;
		}

		// If the line starts with a number, it's a controller
		if (isdigit(line[0]))
		{
			ctl->type = CTL_VALUE;
			ctl->name = NULL;
			ctl->value = 64;

			// Check if match is valid
			if (sscanf(line, "%d%*[ \t]%ms", &ctl->id, &ctl->name) != 2)
			{
				fprintf(stderr, "Line %d doest not contain a valid controller description\n", line_number);
				goto error;
			}

			// Check CC range
			if (ctl->id < 0 || ctl->id > 127)
			{
				fprintf(stderr, "Invalid MIDI CC %d\n", ctl->id);
				goto error;
			}

			ctl_count++;
			continue;
		}

		// If we got here, something is wrong
		fprintf(stderr, "Bad syntax on line %d\n", line_number);
		goto error;
	}

	// Check for duplicates
	char found[128] = {0};
	for (int i = 0; i < ctl_count; i++)
	{
		if (ctls[i].type == CTL_VALUE)
		{
			int id = ctls[i].id;
			if (found[id])
			{
				fprintf(stderr, "Duplicate %d controller found!\n", id);
				goto error;
			}
			
			found[id] = 1;
		}
	}
	
	// Success
	free(line);
	*count = ctl_count;
	return ctls;

	// Exit with error
	error:
	for (int i = 0; i < ctl_count; i++)
		free(ctls[i].name);
	free(ctls);
	free(line);
	*count = 0;
	return NULL;
}

/**
	Sends MIDI CC with provided ALSA sequencer
*/
void send_midi_cc(snd_seq_t *seq, int channel, int cc, int value)
{
	snd_seq_event_t ev;
	snd_seq_ev_clear(&ev);
	snd_seq_ev_set_direct(&ev);
	snd_seq_ev_set_subs(&ev);
	snd_seq_ev_set_controller(&ev, channel, cc, value);
	snd_seq_event_output(seq, &ev);
	snd_seq_drain_output(seq);
}

/**
	Shows a message/prompt at the bottom of the window
*/
void show_bottom_mesg(WINDOW *win, const char *format, ...)
{
	// Get terminal size
	int win_w, win_h;
	getmaxyx(win, win_h, win_w);
	(void) win_w;

	// Clear the bottom line
	move(win_h - 1, 0);
	clrtoeol();

	// Show the line
	move(win_h - 1, 0);
	va_list ap;
	va_start(ap, format);
	vw_printw(win, format, ap);
	va_end(ap);
}

/**
	Prompts user for response
	\returns malloc() allocated buffer with user's response
*/
char *show_bottom_prompt(WINDOW *win, const char *prompt, ...)
{
	va_list ap;
	va_start(ap, prompt);
	show_bottom_mesg(win, prompt);
	va_end(ap);
	char *buf = malloc(1024);
	echo();
	curs_set(1);
	scanw("%1023s", buf);
	curs_set(0);
	noecho();
	return buf;
}

/**
	Shows a prompt asking for a new value for a MIDI controller
*/
void midi_ctl_data_entry(WINDOW *win, midi_ctl *ctl)
{
	char *buf = show_bottom_prompt(win, "Enter new value: ");
	int value;
	if (!sscanf(buf, "%d", &value) || value < 0 || value > 127)
	{
		show_bottom_mesg(win, "Invalid value.");
		wgetch(win);
	}
	else
	{
		midi_ctl_set(ctl, value);
	}
	free(buf);
}

void midi_ctl_search(WINDOW *win, midi_ctl *ctls, int ctl_count, int *index)
{
	char *str = show_bottom_prompt(win, "Search for: ");

	for (int i = 0; i < ctl_count; i++)
	{
		int k = (i + 1 + *index) % ctl_count;
		if (ctls[k].type == CTL_VALUE && strcasestr(ctls[k].name, str) != NULL)
		{
			*index = k;
			break;
		}
	}

	free(str);
}

void midi_ctl_move_cursor(midi_ctl *ctls, int ctl_count, int *cursor, int delta)
{
	int c = *cursor + delta;
	int d = delta > 0 ? 1 : -1;

	// If not on valid field, continue moving
	while (c >= 0 && c < ctl_count && ctls[c].type != CTL_VALUE)
		c += d;

	// If reached end of the list, search from the end in the opposite direction
	if (c < 0 || c >= ctl_count)
	{
		c = d > 0 ? ctl_count - 1 : 0;
		while (c >= 0 && c < ctl_count && ctls[c].type != CTL_VALUE)
			c -= d;
	}

	// If reached the end again, do not update
	if (c >= 0 && c < ctl_count)
		*cursor = c;
}

int main(int argc, char *argv[])
{
	int err;

	// Parse command line args
	args_config config = {0};
	struct argp argp = {argp_options, args_parser, argp_keydoc, argp_doc};
	argp_parse(&argp, argc, argv, 0, 0, &config);
	if (args_config_interpret(&config))
		exit(EXIT_FAILURE);

	int midi_dest_client = config.midi_device;
	int midi_dest_port = config.midi_port;
	int midi_channel = config.midi_channel;

	// Create MIDI sequencer client
	snd_seq_t *midi_seq;
	err = snd_seq_open(&midi_seq, "default", SND_SEQ_OPEN_DUPLEX, 0);
	if (err < 0)
	{
		perror("snd_seq_open() failed");
		exit(EXIT_FAILURE);
	}
	
	// Open MIDI port
	int midi_src_port = snd_seq_create_simple_port(midi_seq, "midictl port",
		SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
		SND_SEQ_PORT_TYPE_APPLICATION);
	
	// Connect to the destination MIDI client
	err = snd_seq_connect_to(midi_seq, midi_src_port, midi_dest_client, midi_dest_port);
	if (err < 0)
	{
		perror("Failed connecting to the MIDI device");
		exit(EXIT_FAILURE);
	}

	// Load controller list
	FILE *ctls_file = fopen(config.ctls_filename, "rt");
	if (ctls_file == NULL)
	{
		perror("Could not open controllers file");
		exit(EXIT_FAILURE);
	}
	
	int ctl_count = 0;
	midi_ctl *ctls = load_ctls_from_file(ctls_file, &ctl_count);
	if (ctls == NULL)
		exit(EXIT_FAILURE);
	
	// Close the controllers file
	fclose(ctls_file);
	
	// Ncurses init
	WINDOW *win = initscr();
	keypad(win, TRUE);
	curs_set(0);
	noecho();

	// Make sure we start at the top
	int list_cursor = 1;
	int list_viewport = 0;
	float list_split = 0.7;
	midi_ctl_move_cursor(ctls, ctl_count, &list_cursor, -1);

	int active = 1;
	while (active)
	{
		// Get terminal size
		int win_w, win_h;
		getmaxyx(win, win_h, win_w);
		(void) win_w;
		
		// Manage list display
		bool active_ctl_change = 0;

		// Make sure cursor is in the viewport
		if (list_cursor - list_viewport < 0)
			list_viewport = list_cursor;
		if (list_cursor - list_viewport >= win_h)
			list_viewport = list_cursor - win_h + 1;
		
		// Pointer to the currently selected controller
		midi_ctl *active_ctl = &ctls[list_cursor];
		
		// Draw
		clear();
		list_split = CLAMP(list_split, 0.2f, 0.8f);
		draw_ctls(win, ctls, ctl_count, list_viewport, list_cursor, list_split);
		refresh();

		// Handle user input
		int c = wgetch(win);
		switch (c)
		{
			// Quit
			case 'q':
				active = 0;
				break;
			
			// Data entry
			case KEY_ENTER:
			case '\n':
			case '\r':
			case 'x':
				midi_ctl_data_entry(win, active_ctl);
				active_ctl_change = 1;
				break;

			// Previous controller
			case KEY_UP:
			case 'k':
				midi_ctl_move_cursor(ctls, ctl_count, &list_cursor, -1);
				break;

			// Previous page
			case KEY_PPAGE:
				midi_ctl_move_cursor(ctls, ctl_count, &list_cursor, -win_h);
				break;

			// Next controller
			case KEY_DOWN:
			case 'j':
				midi_ctl_move_cursor(ctls, ctl_count, &list_cursor, 1);
				break;

			// Next page
			case KEY_NPAGE:
				midi_ctl_move_cursor(ctls, ctl_count, &list_cursor, win_h);
				break;

			// Increment value
			case KEY_RIGHT:
			case 'l':
				midi_ctl_set(active_ctl, active_ctl->value + 1);
				active_ctl_change = 1;
				break;

			// Increment value (big step)
			case 'L':
				midi_ctl_set(active_ctl, active_ctl->value + 10);
				active_ctl_change = 1;
				break;

			// Decrement value
			case 'h':
			case KEY_LEFT:
				midi_ctl_set(active_ctl, active_ctl->value - 1);
				active_ctl_change = 1;
				break;

			// Decrement value (big step)
			case 'H':
				midi_ctl_set(active_ctl, active_ctl->value - 10);
				active_ctl_change = 1;
				break;

			// Set to min
			case 'y':
				midi_ctl_set(active_ctl, 0);
				active_ctl_change = 1;
				break;

			// Set to center
			case 'u':
				midi_ctl_set(active_ctl, 64);
				active_ctl_change = 1;
				break;

			// Set to max
			case 'i':
				midi_ctl_set(active_ctl, 127);
				active_ctl_change = 1;
				break;

			// Search
			case '/':
				midi_ctl_search(win, ctls, ctl_count, &list_cursor);
				break;

			// Move split to the left
			case '[':
				list_split -= 0.05f;
				break;

			// Move split to the right
			case ']':
				list_split += 0.05f;
				break;
		}

		// Send MIDI message if controller value has changed
		if (active_ctl_change)
			send_midi_cc(midi_seq, midi_channel, active_ctl->id, active_ctl->value);
	}

	// Free controller list
	for (int i = 0; i < ctl_count; i++)
		free(ctls[i].name);
	free(ctls);
	
	endwin();
	snd_seq_close(midi_seq);
	return 0;
}
