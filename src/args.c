#include "args.h"
#include <argp.h>
#include "midictl.h"

const char *argp_program_version = "midictl v1.0rc1";
const char *argp_program_bug_address = "<mrjjot@gmail.com>";
char argp_doc[] = "midictl - a terminal-based MIDI control panel";
char argp_keydoc[] = "[CONFIG]";
struct argp_option argp_options[] =
{
	{"channel", 'c', "channel", 0, "MIDI channel"},
	{"device",  'd', "device",  0, "Destination MIDI device"},
	{"port",    'p', "port",    0, "Destination MIDI port"},
	{0}
};

error_t args_parser(int key, char *arg, struct argp_state *state)
{
	midictl_args *conf = (midictl_args*)state->input;
	
	switch (key)
	{
		case 'c':
			conf->midi_channel_str = arg;
			break;

		case 'd':
			conf->midi_device_str = arg;
			break;
		
		case 'p':
			conf->midi_port_str = arg;
			break;

		case ARGP_KEY_ARG:
			if (state->arg_num >= 1) argp_usage(state);
			conf->config_path = arg;
			break;

		case ARGP_KEY_END:
			if (state->arg_num < 0) argp_usage(state);
			break;
		
		default:
			return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

int args_config_interpret(midictl_args *conf)
{
	conf->midi_port = 0;
	conf->midi_channel = 0;

	if (conf->midi_channel_str)
	{
		if (!sscanf(conf->midi_channel_str, "%d", &conf->midi_channel))
		{
			fprintf(stderr, "Invalid MIDI channel!\n");
			return 1;
		}
	}

	if (!conf->midi_device_str)
	{
		fprintf(stderr, "MIDI device ID must be specified!\n");
		return 1;
	}

	if (!sscanf(conf->midi_device_str, "%d", &conf->midi_device))
	{
		fprintf(stderr, "Invalid MIDI device ID!\n");
		return 1;
	}
	
	if (conf->midi_port_str)
	{
		if (!sscanf(conf->midi_port_str, "%d", &conf->midi_port))
		{
			fprintf(stderr, "Invalid MIDI port number!\n");
			return 1;
		}
	}

	return 0;
}