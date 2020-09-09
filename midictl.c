#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>
#include <stdbool.h>
#include <stdarg.h>
#include <alsa/asoundlib.h>

#define MIN(a, b) ((a) <= (b) ? (a) : (b))
#define MAX(a, b) ((a) >= (b) ? (a) : (b))
#define CLAMP(x, min, max) (MAX(MIN(x, (max)), (min)))

/**
	Menu entry (controller) type
*/
typedef enum controller_type
{
	CTL_SLIDER,
	CTL_HEADER
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
	int lw; // Label width
	mvprintw(y, x, "%3d [%n", v, &lw);
	
	int sw = w - lw - 1; // Slider width
	int blocks = (float)v / max * sw;

	mvhline(y, x + lw, 0, blocks);
	mvprintw(y, x + lw + sw, "]");
}

/**
	The main draw function
*/
void draw_ctls(WINDOW *win, midi_ctl *ctls, int count, int offset, int active)
{
	int max_x, max_y;
	getmaxyx(win, max_y, max_x);
	
	int split_x = max_x / 2;
	int lcol = 0;
	int lcol_w = split_x - 1;
	int rcol = split_x + 2;
	int rcol_w = max_x - rcol - 2;

	for (int y = 0; y < max_y; y++)
	{
		int i = y + offset;
		int valid = i < count && i >= 0;
		midi_ctl *ctl = &ctls[i];

		if (i == active) attron(A_REVERSE);

		if (valid)
			mvprintw(y, lcol, "%4d %*s", ctl->id, lcol_w - 6, ctl->name);
		else
			mvprintw(y, lcol, "%*s", lcol, "");
		
		attroff(A_REVERSE);

		if (valid)
		{
			switch (ctl->type)
			{
				case CTL_SLIDER:
					draw_slider_ctl(win, y, rcol, rcol_w, ctl->value, 127);
					break;

				default:
					break;
			}
		}
	}

	mvvline(0, split_x, 0, max_y);
}

/**
	\todo Fix memory leaks when handling errors
*/
midi_ctl *load_ctls_from_file(FILE *f, int *count)
{
	// Allocate memory for some controllers
	const int max_lines = 512;
	midi_ctl *ctls = calloc(max_lines, sizeof(midi_ctl));
	
	// Parse controller data
	char *line = NULL;
	size_t len = 0;
	int i = 0;
	while (i < max_lines && getline(&line, &len, f) > 0)
	{
		int name_offset;
		
		// Handle comments (headers)
		if (line[0] == '#')
		{
			sscanf(line, "#%n", &name_offset);
			ctls[i].type = CTL_HEADER;
			ctls[i].name = strdup(line + name_offset);
		}
		else // Everything else is a slider
		{
			int cc;
			sscanf(line, "%d %n", &cc, &name_offset);
		
			// Check CC range
			if (cc < 0 || cc > 127)
			{
				fprintf(stderr, "Invalid MIDI CC - %d\n", cc);
				return NULL;
			}

			ctls[i].type = CTL_SLIDER;
			ctls[i].name = strdup(line + name_offset);
			ctls[i].id = cc;
			ctls[i].value = 64;
		}
		
		i++;
	}
	free(line);

	// Controller count
	*count = i;
	
	// Check for duplicates
	char found[128] = {0};
	for (int i = 0; i < *count; i++)
	{
		int id = ctls[i].id;
		if (found[id])
		{
			free(ctls);
			fprintf(stderr, "duplicate %d controller found!\n", id);
			return NULL;
		}
		
		found[id] = 1;
	}
	
	return ctls;
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
	int max_x, max_y;
	getmaxyx(win, max_y, max_x);

	// Clear the bottom line
	move(max_y - 1, 0);
	clrtoeol();

	// Show the line
	move(max_y - 1, 0);
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
	scanw("%1023s", buf);
	return buf;
}

/**
	Shows a prompt asking for a new value for a MIDI controller
*/
void midi_ctl_data_entry(WINDOW *win, midi_ctl *ctl)
{
	char *buf = show_bottom_prompt(win, "Enter new value: ");
	int value;
	echo();
	if (!sscanf(buf, "%d", &value) || value < 0 || value > 127)
	{
		show_bottom_mesg(win, "Invalid value.");
		wgetch(win);
	}
	else
	{
		midi_ctl_set(ctl, value);
	}
	noecho();
	free(buf);
}

int main(int argc, char *argv[])
{
	int err;

	int midi_dest_client = 28;
	int midi_dest_port = 0;
	int midi_channel = 0;
	
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
		perror("snd_seq_connect_to() failed");
		exit(EXIT_FAILURE);
	}

	// Load controller list
	FILE *ctls_file = fopen("usynth-ctls.txt", "rt");
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

	int list_cursor = 0;
	int active = 1;
	while (active)
	{
		// Get terminal size
		int max_x, max_y;
		getmaxyx(win, max_y, max_x);
		
		// Manage list display
		bool active_ctl_change = 0;
		if (list_cursor < 0) list_cursor = 0;
		else if (list_cursor >= ctl_count) list_cursor = ctl_count - 1;
		
		// Pointer to the currently selected controller
		midi_ctl *active_ctl = &ctls[list_cursor];
		
		// Draw
		clear();
		draw_ctls(win, ctls, ctl_count, 0, list_cursor);
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
			case 'x':
				midi_ctl_data_entry(win, active_ctl);
				active_ctl_change = 1;
				break;

			// Up arrow
			case KEY_UP:
			case 'k':
				list_cursor--;
				break;

			// Down arrow
			case KEY_DOWN:
			case 'j':
				list_cursor++;
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
			case 'i':
				midi_ctl_set(active_ctl, 0);
				active_ctl_change = 1;
				break;

			// Set to center
			case 'o':
				midi_ctl_set(active_ctl, 64);
				active_ctl_change = 1;
				break;

			// Set to max
			case 'i':
				midi_ctl_set(active_ctl, 127);
				active_ctl_change = 1;
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
