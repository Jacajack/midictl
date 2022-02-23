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
	Draws a lame slider
*/
void draw_slider(WINDOW *win, int y, int x, int w, int v, int min, int max)
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
		int blocks = (float)(CLAMP(v, min, max) - min) / (max - min) * sw;
		mvhline(y, x + lw, 0, blocks);
		mvprintw(y, x + lw + sw, "]");
	}
}

/**
	Draws slider without the slider part
*/
void draw_value_label(WINDOW *win, int y, int x, int w, int v, int min, int max)
{
	mvprintw(y, x, "%3d", v);
}

/**
	The main draw function
*/
void draw_menu(WINDOW *win, menu_entry *menu, int count, int offset, int active, float split_pos, int show_lcol)
{
	int win_w, win_h;
	getmaxyx(win, win_h, win_w);
	
	int col[3];   // Column positions
	int colw[3];  // Column widths
	int split[2]; // Splitter positions

	// Optional left column is fixed width
	if (show_lcol)
	{
		col[0] = 0;
		colw[0] = 4;
		split[0] = col[0] + colw[0];
	}
	else
	{
		col[0] = -1;
		colw[0] = 0;
		split[0] = -1;
	}

	// Middle column with variable width	
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
			if (show_lcol)
				mvprintw(y, col[0], "%3d", ent->midi_ctl.cc);

			mvprintw(y, col[1], "%.*s", colw[1], ent->text);
			int len = strlen(ent->text);
			int left = colw[1] - len;
			if (left > 0)
				mvprintw(y, col[1] + len, "%*s", left, "");
		}

		attroff(A_REVERSE);

		if (ent->type == ENTRY_MIDI_CTL)
		{
			if (ent->midi_ctl.slider)
				draw_slider(win, y, col[2], colw[2], ent->midi_ctl.value, ent->midi_ctl.min, ent->midi_ctl.max);
			else
				draw_value_label(win, y, col[2], colw[2], ent->midi_ctl.value, ent->midi_ctl.min, ent->midi_ctl.max);
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
	if (show_lcol)
		mvvline(0, split[0], 0, win_h);
	mvvline(0, split[1], 0, win_h);

	// Draw crosses where rules are
	for (int y = 0; y < win_h; y++)
	{
		int i = y + offset;
		int valid = i < count && i >= 0;
		menu_entry *ent = &menu[i];
		
		if (!valid || ent->type != ENTRY_HRULE) continue;

		if (show_lcol)
		{
			move(y, split[0]);
			addch(ACS_PLUS);
		}

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
	char *buf = calloc(1024, sizeof(char));
	echo();
	curs_set(1);
	scanw("%1023s", buf);
	curs_set(0);
	noecho();
	return buf;
}

/**
	Shows user a search prompt and searches for an item in menu

	\note Call with menu=NULL or win=NULL to free last search string
*/
void menu_search(WINDOW *win, menu_entry *menu, int menu_size, menu_entry_type type, int *index)
{
	static char *menu_search_last = NULL;
	if (menu == NULL || win == NULL)
	{
		free(menu_search_last);
		return;
	}

	char *str = draw_bottom_prompt(win, "Search for: ");
	char *needle = str;
	int repeat = 0;
	
	// Repeat search
	if (isempty(str) && menu_search_last != NULL)
	{
		needle = menu_search_last;
		repeat = 1;
	}

	for (int i = 0; i < menu_size; i++)
	{
		int k = (i + 1 + *index) % menu_size;
		if (menu[k].type == type && menu[k].text && strcasestr(menu[k].text, needle) != NULL)
		{
			*index = k;
			break;
		}
	}

	if (repeat)
	{
		free(str);
	}
	else
	{
		free(menu_search_last);
		menu_search_last = str;
	}
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

/**
	Sets a value for MIDI controller menu entry
*/
void midi_ctl_set(menu_entry *ent, int v)
{
	assert(ent->type == ENTRY_MIDI_CTL);
	ent->midi_ctl.value = CLAMP(v, ent->midi_ctl.min, ent->midi_ctl.max);
	ent->midi_ctl.changed = 1;
}

/**
	Send MIDI CC based on current state of provided MIDI_CTL menu entry
*/
void midi_ctl_send_cc(menu_entry *ent, midictl_alsa_seq *seq, int default_midi_channel)
{
	assert(ent->type == ENTRY_MIDI_CTL);
	int ch = ent->midi_ctl.channel < 0 ? default_midi_channel : ent->midi_ctl.channel;
	alsa_seq_send_midi_cc(seq, ch, ent->midi_ctl.cc, ent->midi_ctl.value);
	ent->midi_ctl.changed = 0;
}

/**
	Resets the value to default
*/
void midi_ctl_reset(menu_entry *ent)
{
	assert(ent->type == ENTRY_MIDI_CTL);
	if (ent->midi_ctl.def < 0)
		midi_ctl_set(ent, (ent->midi_ctl.min + ent->midi_ctl.max) / 2);
	else
		midi_ctl_set(ent, ent->midi_ctl.def);
}

/**
	Mark MIDI controller as changed
*/
void midi_ctl_touch(menu_entry *ent)
{
	assert(ent->type == ENTRY_MIDI_CTL);
	ent->midi_ctl.changed = 1;
}

/**
	Reset all controllers in menu
*/
void midi_ctl_reset_all(menu_entry *menu, int menu_size)
{
	for (int i = 0; i < menu_size; i++)
		if (menu[i].type == ENTRY_MIDI_CTL)
			midi_ctl_reset(&menu[i]);
}

/**
	Mark all controllers in menu as changed
*/
void midi_ctl_touch_all(menu_entry *menu, int menu_size)
{
	for (int i = 0; i < menu_size; i++)
		if (menu[i].type == ENTRY_MIDI_CTL)
			midi_ctl_touch(&menu[i]);
}

/**
	Update (transmit) all controllers marked as changed
*/
void midi_ctl_update_changed(menu_entry *menu, int menu_size, midictl_alsa_seq *seq, int default_midi_channel)
{
	for (int i = 0; i < menu_size; i++)
		if (menu[i].type == ENTRY_MIDI_CTL && menu[i].midi_ctl.changed)
			midi_ctl_send_cc(&menu[i], seq, default_midi_channel);
}

/**
	Shows a prompt asking for a new value for a MIDI controller
*/
void midi_ctl_value_prompt(WINDOW *win, menu_entry *ent)
{
	assert(ent->type == ENTRY_MIDI_CTL);
	char *buf = draw_bottom_prompt(win, "Enter new value: ");
	int value;
	if (!sscanf(buf, "%d", &value) || !INRANGE(value, ent->midi_ctl.min, ent->midi_ctl.max))
	{
		draw_bottom_mesg(win, "Invalid value.");
		wgetch(win);
	}
	else
	{
		midi_ctl_set(ent, value);
	}
	free(buf);
}

/**
	Writes all current controller values to a file

	\note The filename input handles all characters very literally
*/
void midi_ctl_dump_all_to_file(WINDOW *win, menu_entry *menu, int menu_size)
{
	char *filename = draw_bottom_prompt(win, "Dump to file: ");
	if (isempty(filename))
	{
		free(filename);
		return;
	}

	FILE *f = fopen(filename, "wt");
	free(filename);
	
	if (!f)
	{
		draw_bottom_mesg(win, "Could not open file for writing.");
		wgetch(win);
		return;
	}

	for (int i = 0; i < menu_size; i++)
	{
		if (menu[i].type == ENTRY_MIDI_CTL)
			fprintf(f, "%d\t%d\t# %s\n", menu[i].midi_ctl.cc, menu[i].midi_ctl.value, menu[i].text);
	}

	fclose(f);
}

/**
	Load controller values from dump file generated by
	midi_ctl_dump_all_to_file()
*/
void midi_ctl_load_from_dump_file(WINDOW *win, menu_entry *menu, int menu_size)
{
	char *filename = draw_bottom_prompt(win, "Load from file: ");
	if (isempty(filename))
	{
		free(filename);
		return;
	}

	FILE *f = fopen(filename, "rt");
	free(filename);	
	if (!f)
	{
		draw_bottom_mesg(win, "Could not open file for reading.");
		wgetch(win);
		return;
	}

	// Read line by line
	const char *errstr = NULL;
	char *line = NULL;
	size_t line_len = 0;
	while (!errstr && getline(&line, &line_len, f) > 0)
	{
		int cc, value;
		if (sscanf(line, "%d %d", &cc, &value) != 2)
		{
			errstr = "Invalid syntax!";
			break;
		}

		// Search for the controller in the menu
		for (int i = 0; !errstr && i < menu_size; i++)
		{
			if (menu[i].type == ENTRY_MIDI_CTL && menu[i].midi_ctl.cc == cc)
			{
				midi_ctl_set(&menu[i], value);
				break;
			}
		}
	}
	free(line);
	fclose(f);

	if (errstr)
	{
		draw_bottom_mesg(win, "%s", errstr);
		wgetch(win);
	}
}

int main(int argc, char *argv[])
{
	// Parse command line args
	midictl_args config = {0};
	struct argp argp = {argp_options, args_parser, argp_keydoc, argp_doc};
	argp_parse(&argp, argc, argv, 0, 0, &config);
	if (args_config_interpret(&config))
		exit(EXIT_FAILURE);

	int default_midi_channel = config.midi_channel;

	// Init ALSA seq
	midictl_alsa_seq midi_seq;
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

	bool save_config_path = 1;
	// check if config file path is given
	//if not open the last given path.
	if (config.config_path == NULL)
	{
		FILE *prev_config_path = fopen(".prevconfpath", "rt");
		if (prev_config_path == NULL)
		{
			//prev file doesnt exist
			fprintf(stderr, "Previously used config path is non existant\n");
			exit(EXIT_FAILURE);
		}
		
		//get the file size
		fseek(prev_config_path, 0L, SEEK_END);
    	uint32_t sz = ftell(prev_config_path) + 1;
    	fseek(prev_config_path, 0L, SEEK_SET);

		//get the path
		char* f_path = malloc(sz);
		fgets(f_path, sz, prev_config_path);

		//close the file
		fclose(prev_config_path);

		config.config_path = f_path;
		
		save_config_path = 0;
	}

	// Open config file
	FILE *config_file = fopen(config.config_path, "rt");
	if (config_file == NULL)
	{
		fprintf(stderr, "Could not open config file: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (save_config_path)
	{
		FILE *conf_path = fopen(".prevconfpath", "wt");
		fputs(config.config_path, conf_path);
		fclose(conf_path);
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
	if (win == NULL)
	{
		fprintf(stderr, "ncurses init failed!\n");
		exit(EXIT_FAILURE);
	}

	keypad(win, TRUE);
	curs_set(0);
	noecho();

	// UI setup - make sure we start on the top
	int menu_cursor = 1;
	int menu_viewport = 0;
	int menu_show_lcol = 1;
	float menu_split = 0.5;
	menu_move_cursor(menu, menu_size, &menu_cursor, -1);

	// Update changed controllers
	// At this point only controllers with default value have 'changed' flag set
	// see: config_parser.c
	midi_ctl_update_changed(menu, menu_size, &midi_seq, default_midi_channel);

	// The main loop
	int active = 1;
	while (active)
	{
		// Get terminal size
		int win_w, win_h;
		getmaxyx(win, win_h, win_w);
		(void) win_w;
		
		// Make sure cursor is in the viewport
		if (menu_cursor - menu_viewport < 0)
			menu_viewport = menu_cursor;
		if (menu_cursor - menu_viewport >= win_h)
			menu_viewport = menu_cursor - win_h + 1;
		
		// Currently selected menu entry
		menu_entry *active_entry = &menu[menu_cursor];
		
		// Draw
		clear();
		menu_split = CLAMP(menu_split, 0.2f, 0.8f);
		draw_menu(win, menu, menu_size, menu_viewport, menu_cursor, menu_split, menu_show_lcol);
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
			case 'i':
				midi_ctl_value_prompt(win, active_entry);
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

			// Jump to the end of the list
			case KEY_END:
				menu_move_cursor(menu, menu_size, &menu_cursor, menu_size);
				break;

			// Jump to the beginning of the list
			case KEY_HOME:
				menu_move_cursor(menu, menu_size, &menu_cursor, -menu_size);
				break;

			// Increment value
			case KEY_RIGHT:
			case 'l':
				midi_ctl_set(active_entry, active_entry->midi_ctl.value + 1);
				break;

			// Increment value (big step)
			case 'L':
			case ';':
			case 'C':
				midi_ctl_set(active_entry, active_entry->midi_ctl.value + 10);
				break;

			// Decrement value
			case 'h':
			case KEY_LEFT:
				midi_ctl_set(active_entry, active_entry->midi_ctl.value - 1);
				break;

			// Decrement value (big step)
			case 'H':
			case 'g':
			case 'Z':
				midi_ctl_set(active_entry, active_entry->midi_ctl.value - 10);
				break;

			// Set to min
			case 'z':
				midi_ctl_set(active_entry, active_entry->midi_ctl.min);
				break;

			// Set to center
			case 'x':
				midi_ctl_set(active_entry, (active_entry->midi_ctl.max + active_entry->midi_ctl.min) / 2);
				break;

			// Set to max
			case 'c':
				midi_ctl_set(active_entry, active_entry->midi_ctl.max);
				break;

			// Set to default (reset)
			case 'r':
				midi_ctl_reset(active_entry);
				break;

			// Retransmit
			case 't':
				midi_ctl_touch(active_entry);
				break;

			// Reset all
			case 'R':
				midi_ctl_reset_all(menu, menu_size);
				break;

			// Retransmit
			case 'T':
				midi_ctl_touch_all(menu, menu_size);
				break;

			// Dump all to file
			case 'D':
				midi_ctl_dump_all_to_file(win, menu, menu_size);
				break;

			// Load dump file
			case 'O':
				midi_ctl_load_from_dump_file(win, menu, menu_size);
				break;

			// Search
			case '/':
				menu_search(win, menu, menu_size, ENTRY_MIDI_CTL, &menu_cursor);
				break;

			// Toggle left column visibility
			case '=':
				menu_show_lcol = !menu_show_lcol;
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

		// Update all changed controllers
		midi_ctl_update_changed(menu, menu_size, &midi_seq, default_midi_channel);
	}

	// Destroy the menu
	for (int i = 0; i < menu_size; i++)
		free(menu[i].text);
	free(menu);
	
	// Free search cache
	menu_search(NULL, NULL, 0, 0, NULL);

	endwin();
	alsa_seq_destroy(&midi_seq);
	config_parser_destroy();
	return 0;
}
