#ifndef MIDICTL_ALSA_H
#define MIDICTL_ALSA_H

#include <alsa/asoundlib.h>

typedef struct midictl_alsa_seq
{
	snd_seq_t *seq;
	int port;
	int queue;
	int tick_cnt;
} midictl_alsa_seq;

extern void alsa_seq_send_midi_cc(midictl_alsa_seq *seq, int channel, int cc, int value);
extern int alsa_seq_init(midictl_alsa_seq *seq, int dest_client, int dest_port);
extern void alsa_seq_destroy(midictl_alsa_seq *seq);
#endif