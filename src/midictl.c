#include "midictl.h"
#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <argp.h>
#include <assert.h>
#include <errno.h>
#include "args.h"
#include "config_parser.h"
#include "alsa.h"
#include "utils.h"

/**
	Sets a value for MIDI controller menu entry
*/
void menu_entry_midi_ctl_set(menu_entry *ctl, int v)
{
	ctl->value = CLAMP(v, 0, 127);
}

/**
	Draws a lame slider
*/
void draw_slider(WINDOW *win, int y, int x, int w, int v, int max)
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
void draw_menu(WINDOW *win, menu_entry *menu, int count, int offset, int active, float split_pos)
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
		menu_entry *ent = &menu[i];

		if (!valid) continue;

		// Invert if selected and draw
		// background for the left and middle column
		if (i == active)
		{
			attron(A_REVERSE);
			mvprintw(y, col[0], "%*s", split[1], "");
		}

		// Left and middle columns
		if (ent->type == ENTRY_MIDI_CTL)
		{
			mvprintw(y, col[0], "%3d", ent->cc);

			mvprintw(y, col[1], "%.*s", colw[1], ent->text);
			int len = strlen(ent->text);
			int left = colw[1] - len;
			if (left > 0)
				mvprintw(y, col[1] + len, "%*s", left, "");
		}

		attroff(A_REVERSE);

		if (ent->type == ENTRY_MIDI_CTL)
		{
			draw_slider(win, y, col[2], colw[2], ent->value, 127);
		}
		else if (ent->type == ENTRY_HRULE)
		{
			mvhline(y, 0, 0, win_w);

			if (ent->text)
			{
				int len = strlen(ent->text);
				int x = split[1] - len - 3;
				
				if (x > split[0] && len + 2 < colw[1])
				{
					move(y, x);
					addch(ACS_RTEE);
					attron(A_REVERSE);
					printw("%s", ent->text);
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
		menu_entry *ent = &menu[i];
		
		if (!valid || ent->type != ENTRY_HRULE) continue;

		move(y, split[0]);
		addch(ACS_PLUS);
		move(y, split[1]);
		addch(ACS_PLUS);
	}
}

/**
	Shows a message/prompt at the bottom of the window
*/
void draw_bottom_mesg(WINDOW *win, const char *format, ...)
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
char *draw_bottom_prompt(WINDOW *win, const char *prompt, ...)
{
	va_list ap;
	va_start(ap, prompt);
	draw_bottom_mesg(win, prompt);
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
void midi_ctl_data_entry(WINDOW *win, menu_entry *ctl)
{
	char *buf = draw_bottom_prompt(win, "Enter new value: ");
	int value;
	if (!sscanf(buf, "%d", &value) || value < 0 || value > 127)
	{
		draw_bottom_mesg(win, "Invalid value.");
		wgetch(win);
	}
	else
	{
		menu_entry_midi_ctl_set(ctl, value);
	}
	free(buf);
}

/**
	Shows user a search prompt and searches for an item in menu
*/
void menu_midi_ctl_search(WINDOW *win, menu_entry *menu, int menu_size, menu_entry_type type, int *index)
{
	char *str = draw_bottom_prompt(win, "Search for: ");

	for (int i = 0; i < menu_size; i++)
	{
		int k = (i + 1 + *index) % menu_size;
		if (menu[k].type == type && menu[k].text && strcasestr(menu[k].text, str) != NULL)
		{
			*index = k;
			break;
		}
	}

	free(str);
}

/**
	Moves menu
*/
void menu_move_cursor(menu_entry *menu, int entry_count, int *cursor, int delta)
{
	int c = *cursor + delta;
	int d = delta > 0 ? 1 : -1;

	// If not on valid field, continue moving
	while (c >= 0 && c < entry_count && menu[c].type != ENTRY_MIDI_CTL)
		c += d;

	// If reached end of the list, search from the end in the opposite direction
	if (c < 0 || c >= entry_count)
	{
		c = d > 0 ? entry_count - 1 : 0;
		while (c >= 0 && c < entry_count && menu[c].type != ENTRY_MIDI_CTL)
			c -= d;
	}

	// If reached the end again, do not update
	if (c >= 0 && c < entry_count)
		*cursor = c;
}

int main(int argc, char *argv[])
{
	// Parse command line args
	midictl_args config = {0};
	struct argp argp = {argp_options, args_parser, argp_keydoc, argp_doc};
	argp_parse(&argp, argc, argv, 0, 0, &config);
	if (args_config_interpret(&config))
		exit(EXIT_FAILURE);

	int midi_channel = config.midi_channel;

	// Init ALSA seq
	snd_seq_t *midi_seq;
	if (alsa_seq_init(&midi_seq, config.midi_device, config.midi_port))
	{
		fprintf(stderr, "ALSA sequencer init failed!\n");
		exit(EXIT_FAILURE);
	}

	// Parser init
	if (config_parser_init())
	{
		fprintf(stderr, "Config parser init failed!\n");
		exit(EXIT_FAILURE);
	}

	// Open config file
	FILE *config_file = fopen(config.config_path, "rt");
	if (config_file == NULL)
	{
		fprintf(stderr, "Could not open config file: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	
	// Build menu
	int menu_size = 0;
	menu_entry *menu = build_menu_from_config_file(config_file, &menu_size);
	if (menu == NULL)
		exit(EXIT_FAILURE);
	
	// Close config file
	fclose(config_file);
	
	// Ncurses init
	WINDOW *win = initscr();
	keypad(win, TRUE);
	curs_set(0);
	noecho();

	// UI setup - make sure we start on the top
	int menu_cursor = 1;
	int menu_viewport = 0;
	float menu_split = 0.7;
	menu_move_cursor(menu, menu_size, &menu_cursor, -1);

	// The main loop
	int active = 1;
	while (active)
	{
		// Get terminal size
		int win_w, win_h;
		getmaxyx(win, win_h, win_w);
		(void) win_w;
		
		// If set to true, MIDI CC will be sent
		// for the active menu entry
		int midi_cc_change = 0;

		// Make sure cursor is in the viewport
		if (menu_cursor - menu_viewport < 0)
			menu_viewport = menu_cursor;
		if (menu_cursor - menu_viewport >= win_h)
			menu_viewport = menu_cursor - win_h + 1;
		
		// Pointer to the currently selected controller
		menu_entry *active_entry = &menu[menu_cursor];
		
		// Draw
		clear();
		menu_split = CLAMP(menu_split, 0.2f, 0.8f);
		draw_menu(win, menu, menu_size, menu_viewport, menu_cursor, menu_split);
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
				midi_ctl_data_entry(win, active_entry);
				midi_cc_change = 1;
				break;

			// Previous controller
			case KEY_UP:
			case 'k':
				menu_move_cursor(menu, menu_size, &menu_cursor, -1);
				break;

			// Previous page
			case KEY_PPAGE:
				menu_move_cursor(menu, menu_size, &menu_cursor, -win_h);
				break;

			// Next controller
			case KEY_DOWN:
			case 'j':
				menu_move_cursor(menu, menu_size, &menu_cursor, 1);
				break;

			// Next page
			case KEY_NPAGE:
				menu_move_cursor(menu, menu_size, &menu_cursor, win_h);
				break;

			// Increment value
			case KEY_RIGHT:
			case 'l':
				menu_entry_midi_ctl_set(active_entry, active_entry->value + 1);
				midi_cc_change = 1;
				break;

			// Increment value (big step)
			case 'L':
				menu_entry_midi_ctl_set(active_entry, active_entry->value + 10);
				midi_cc_change = 1;
				break;

			// Decrement value
			case 'h':
			case KEY_LEFT:
				menu_entry_midi_ctl_set(active_entry, active_entry->value - 1);
				midi_cc_change = 1;
				break;

			// Decrement value (big step)
			case 'H':
				menu_entry_midi_ctl_set(active_entry, active_entry->value - 10);
				midi_cc_change = 1;
				break;

			// Set to min
			case 'y':
				menu_entry_midi_ctl_set(active_entry, 0);
				midi_cc_change = 1;
				break;

			// Set to center
			case 'u':
				menu_entry_midi_ctl_set(active_entry, 64);
				midi_cc_change = 1;
				break;

			// Set to max
			case 'i':
				menu_entry_midi_ctl_set(active_entry, 127);
				midi_cc_change = 1;
				break;

			// Search
			case '/':
				menu_midi_ctl_search(win, menu, menu_size, ENTRY_MIDI_CTL, &menu_cursor);
				break;

			// Move split to the left
			case '[':
				menu_split -= 0.05f;
				break;

			// Move split to the right
			case ']':
				menu_split += 0.05f;
				break;
		}

		// Send MIDI message if controller value has changed
		if (midi_cc_change)
			alsa_seq_send_midi_cc(midi_seq, midi_channel, active_entry->cc, active_entry->value);
	}

	// Destroy the menu
	for (int i = 0; i < menu_size; i++)
		free(menu[i].text);
	free(menu);
	
	endwin();
	snd_seq_close(midi_seq);
	config_parser_destroy();
	return 0;
}
