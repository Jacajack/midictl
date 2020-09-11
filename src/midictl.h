#ifndef MIDICTL_H
#define MIDICTL_H

/**
	Configuration from command line arguments
*/
typedef struct midictl_args
{
	int midi_device;
	int midi_port;
	int midi_channel;

	const char *config_path;
	const char *midi_device_str;
	const char *midi_port_str;
	const char *midi_channel_str;
} midictl_args;

/**
	Menu entry (controller) type
*/
typedef enum menu_entry_type
{
	ENTRY_MIDI_CTL,
	ENTRY_HRULE
} menu_entry_type;

/**
	A position in the main menu
*/
typedef struct menu_entry
{
	menu_entry_type type;
	char *text;

	// ENTRY_MIDI_CTL
	struct
	{
		int cc;
		int value;
		int min;
		int max;
		int def;     //!< Default value (-1 to ignore)
		int channel; //!< MIDI channel (-1 to use default)
		int slider;  //!< Should slider be displayed
		int changed; //!< Non-zero if the value needs retransmitting to the device
	} midi_ctl;
} menu_entry;

#endif