#include "alsa.h"
#include <stdio.h>
#include <alsa/asoundlib.h>

/**
	Sends MIDI CC with provided ALSA sequencer
*/
void alsa_seq_send_midi_cc(snd_seq_t *seq, int channel, int cc, int value)
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
	Initializes ALSA sequencer and connects to destination MIDI client
*/
int alsa_seq_init(snd_seq_t **seq, int dest_client, int dest_port)
{
	int err = snd_seq_open(seq, "default", SND_SEQ_OPEN_DUPLEX, 0);
	if (err < 0)
	{
		perror("snd_seq_open() failed");
		return 1;
	}
	
	// Open MIDI port
	int midi_src_port = snd_seq_create_simple_port(*seq, "midictl port",
		SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
		SND_SEQ_PORT_TYPE_APPLICATION);
	
	// Connect to the destination MIDI client
	err = snd_seq_connect_to(*seq, midi_src_port, dest_client, dest_port);
	if (err < 0)
	{
		perror("Failed connecting to the MIDI device");
		return 1;
	}

	return 0;
}