#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>
#include <stdbool.h>
#include <alsa/asoundlib.h>

typedef enum controller_type
{
	CTL_SLIDER,
	CTL_ENABLE,
	CTL_NUMBER,
	CTL_HEADER
} controller_type;

typedef struct midi_ctl
{
	controller_type type;
	char *name;
	int id;
	int value;
} midi_ctl;

void midi_ctl_set(midi_ctl *ctl, int v)
{
	if (v < 0) v = 0;
	else if (v > 127) v = 127;
	ctl->value = v;
}

midi_ctl controllers[4] = {
	{CTL_SLIDER, "EG0 Attack", 77, 88},
	{CTL_SLIDER, "Wavetable", 56, 15},
	{CTL_ENABLE, "knefel", 123, 0},
	{CTL_NUMBER, "zabobon", 45, 11},
};

void draw_slider_ctl(WINDOW *win, int y, int x, int w, int v, int max)
{
	int lw; // Label width
	mvprintw(y, x, "(%3d) [%n", v, &lw);
	
	int sw = w - lw - 1; // Slider width
	int blocks = (float)v / max * sw;

	mvhline(y, x + lw, 0, blocks);
	mvprintw(y, x + lw + sw, "]");
}

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

	// ncurses init
	WINDOW *win = initscr();
	keypad(win, TRUE);
	curs_set(0);

	int list_cursor = 0;
	midi_ctl *ctls = controllers;
	int ctl_count = 4;

	while (1)
	{
		// Draw
		clear();
		draw_ctls(win, ctls, ctl_count, 0, list_cursor);
		refresh();

		bool active_ctl_change = 0;
		if (list_cursor < 0) list_cursor = 0;
		else if (list_cursor >= ctl_count) list_cursor = ctl_count - 1;
		
		// Pointer to the currently selected controller
		midi_ctl *active_ctl = &ctls[list_cursor];

		// Handle user input
		int c = getchar();
		if (c == 'q') break;
		else if (c == '\x1b')
		{
			getchar(); // Ignore [
			switch (getchar())
			{
				// Up arrow
				case 'A':
					list_cursor--;
					break;

				// Down arrow
				case 'B':
					list_cursor++;
					break;

				// Right arrow
				case 'C':
					midi_ctl_set(active_ctl, active_ctl->value + 1);
					active_ctl_change = 1;
					break;

				case 'D':
					midi_ctl_set(active_ctl, active_ctl->value - 1);
					active_ctl_change = 1;
					break;
			}
		}

		// Send MIDI message if controller value has changed
		if (active_ctl_change)
			send_midi_cc(midi_seq, midi_channel, active_ctl->id, active_ctl->value);
	}

	endwin();
	snd_seq_close(midi_seq);
}
