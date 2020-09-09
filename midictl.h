#ifndef MIDICTL_H
#define MIDICTL_H

/**
	Configuration from command line arguments
*/
typedef struct args_config
{
	int midi_device;
	int midi_port;
	int midi_channel;

	const char *ctls_filename;
	const char *midi_device_str;
	const char *midi_port_str;
	const char *midi_channel_str;
} args_config;

#endif