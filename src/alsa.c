#include "alsa.h"
#include <stdio.h>
#include <alsa/asoundlib.h>

/**
	Sends MIDI CC with provided ALSA sequencer
*/
void alsa_seq_send_midi_cc(midictl_alsa_seq *seq, int channel, int cc, int value)
{
	snd_seq_event_t ev;
	snd_seq_ev_clear(&ev);
	snd_seq_ev_set_subs(&ev);
	snd_seq_ev_set_controller(&ev, channel, cc, value);
	snd_seq_ev_schedule_tick(&ev, seq->queue, 0, seq->tick_cnt++); // I hate it, but it works.

	snd_seq_event_output(seq->seq, &ev);
	snd_seq_drain_output(seq->seq);
	snd_seq_sync_output_queue(seq->seq);
}

/**
	Initializes ALSA sequencer and connects to destination MIDI client
*/
int alsa_seq_init(midictl_alsa_seq *seq, int dest_client, int dest_port)
{
	int err = snd_seq_open(&seq->seq, "default", SND_SEQ_OPEN_DUPLEX, 0);
	if (err < 0)
	{
		perror("snd_seq_open() failed");
		return 1;
	}
	
	// Open MIDI port
	seq->port = snd_seq_create_simple_port(seq->seq, "midictl port",
		SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
		SND_SEQ_PORT_TYPE_APPLICATION);

	// Allocate queue and set tempo
	seq->queue = snd_seq_alloc_named_queue(seq->seq, "midictl queue");
	assert(seq->queue >= 0);
	snd_seq_start_queue(seq->seq, seq->queue, NULL);

	// Connect to the destination MIDI client
	err = snd_seq_connect_to(seq->seq, seq->port, dest_client, dest_port);
	if (err < 0)
	{
		perror("Failed connecting to the MIDI device");
		return 1;
	}

	seq->tick_cnt = 0;
	return 0;
}

void alsa_seq_destroy(midictl_alsa_seq *seq)
{
	snd_seq_close(seq->seq);
}