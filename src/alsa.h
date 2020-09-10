#ifndef MIDICTL_ALSA_H
#define MIDICTL_ALSA_H

#include <alsa/asoundlib.h>

extern void alsa_seq_send_midi_cc(snd_seq_t *seq, int channel, int cc, int value);
extern int alsa_seq_init(snd_seq_t **seq, int dest_client, int dest_port);

#endif