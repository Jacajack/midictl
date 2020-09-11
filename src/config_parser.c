#include "midictl.h"
#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <string.h>
#include <assert.h>
#include "utils.h"

/**
	Regex for matching controller lines in the config
*/
static const char *midi_ctl_regex_str = "^\\s*([0-9]*)?\\s*(\\[(.*)\\])?\\s*(.*)$";
static regex_t midi_ctl_regex;

/**
	Regex for parsing metadata
*/
static const char *metadata_regex_str = "\\s*([a-zA-z]+)\\s*=\\s*(-?[0-9]+)\\s*,?";
static regex_t metadata_regex;

/**
	Parses metadata string and properly configures
	MIDI_CTL menu entry
*/
static int parse_metadata(menu_entry *ent, const char *metadata)
{
	int max_matches = 3;
	regmatch_t matches[max_matches];
	int offset = 0;
	int fail = 0;
	int cnt = 0;

	while (!fail)
	{
		const char *str = metadata + offset;
		int err = regexec(&metadata_regex, str, max_matches, matches, 0);
		if (err) break;

		// If key or value is missing, abort
		if (matches[1].rm_so < 0 || matches[2].rm_so < 0)
		{
			fail = 1;
			break;
		}

		// Extract key and value
		char *key = strndup(str + matches[1].rm_so, matches[1].rm_eo - matches[1].rm_so);
		int value = atoi(str + matches[2].rm_so);
		
		// TODO: handle negative values
		if (!strcmp(key, "cc"))
			ent->midi_ctl.cc = value;
		else if (!strcmp(key, "min"))
			ent->midi_ctl.min = value;
		else if (!strcmp(key, "max"))
			ent->midi_ctl.max = value;
		else if (!strcmp(key, "def"))
			ent->midi_ctl.def = value;
		else if (!strcmp(key, "chan"))
			ent->midi_ctl.channel = value;
		else if (!strcmp(key, "slider"))
			ent->midi_ctl.slider = value != 0;
		else
			fail = 1;

		free(key);
		offset += matches[0].rm_eo;
		cnt++;
	}

	if (cnt == 0) fail = 1;
	return fail;
}

/**
	Builds menu entry from a config file line
	\returns -1 on error, 1 if the menu entry was set up and 0 if the line has been ignored (and the entry has not been set up)
*/
static int parse_config_line(menu_entry *ent, char *line, const char **errstr)
{
	// Regex maches array
	int max_matches = 16;
	regmatch_t matches[max_matches];

	// Ignore comments and empty lines
	if (isempty(line) || line[0] == '#') return 0;

	// Lines starting with --- are horizontal rules/headers
	if (strstr(line, "---") == line)
	{
		ent->type = ENTRY_HRULE;
		ent->text = NULL;
		sscanf(line, "---%*[ \t]%m[^\n]", &ent->text);
		return 1;
	}

	// Try to match to controller line format
	if (!regexec(&midi_ctl_regex, line, max_matches, matches, 0))
	{
		ent->type = ENTRY_MIDI_CTL;
		ent->text = NULL;
		ent->midi_ctl.cc = -1;
		ent->midi_ctl.min = 0;
		ent->midi_ctl.max = 127;
		ent->midi_ctl.channel = -1;
		ent->midi_ctl.def = -1;
		ent->midi_ctl.slider = 1;
		ent->midi_ctl.changed = 0;

		// Match CC ID
		if (matches[1].rm_so >= 0)
			sscanf(line + matches[1].rm_so, "%d", &ent->midi_ctl.cc);

		// Match metadata
		if (matches[3].rm_so >= 0)
		{
			char *metadata = strndup(line + matches[3].rm_so, matches[3].rm_eo - matches[3].rm_so);
			int err = parse_metadata(ent, metadata);
			free(metadata);

			// Metadata parsing failed
			if (err)
			{
				*errstr = "Invalid metadata syntax or key!";
				return -1;
			}
		}

		// Check ranges
		if (ent->midi_ctl.min >= ent->midi_ctl.max)
		{
			*errstr = "'min' must be less than max!";
			return -1;
		}

		if (!INRANGE(ent->midi_ctl.min, 0, 127))
		{
			*errstr = "Invalid value for 'min'!";
			return -1;
		}

		if (!INRANGE(ent->midi_ctl.max, 0, 127))
		{
			*errstr = "Invalid value for 'max'!";
			return -1;
		}

		if (ent->midi_ctl.def > 0 && !INRANGE(ent->midi_ctl.def, ent->midi_ctl.min, ent->midi_ctl.max))
		{
			*errstr = "Default value cannot be outside defined range!";
			return -1;
		}

		if (ent->midi_ctl.channel > 0 && !INRANGE(ent->midi_ctl.channel, 0, 15))
		{
			*errstr = "Invalid MIDI channel!";
			return -1;
		}

		// Default
		// TODO replace with midi_ctl_reset
		// Controllers that have default value defined are marked as changed at the beginning
		if (ent->midi_ctl.def < 0)
			ent->midi_ctl.value = (ent->midi_ctl.min + ent->midi_ctl.max) / 2;
		else
		{
			ent->midi_ctl.value = ent->midi_ctl.def;
		}

		// CC is not set
		if (ent->midi_ctl.cc == -1)
		{
			*errstr = "MIDI CC value missing!";
			return -1;
		}

		// Check CC range
		if (ent->midi_ctl.cc < 0 || ent->midi_ctl.cc > 127)
		{
			*errstr = "MIDI CC value invalid (bad range)!";
			return -1;
		}

		// Get name
		if (matches[4].rm_so >= 0)
		{
			ent->text = strndup(line + matches[4].rm_so, matches[4].rm_eo - matches[4].rm_so);
		}
		else
		{
			*errstr = "Missing text!";
			return -1;
		}

		return 1;
	}

	// If we got here, something is wrong
	*errstr = "Bad syntax";
	return -1;
}

/**
	Build menu based on config file

	\todo Fix memory leaks when handling errors
*/
menu_entry *build_menu_from_config_file(FILE *f, int *count)
{
	int fail = 0;

	// Allocate memory for some controllers
	const int max_menu_size = 512;
	menu_entry *menu = calloc(max_menu_size, sizeof(menu_entry));
	int menu_size = 0;
	
	// Read file line by line
	char *line = NULL;
	size_t line_buffer_len = 0;
	int line_len;
	for (int line_number = 1; menu_size < max_menu_size && (line_len = getline(&line, &line_buffer_len, f)) > 0; line_number++)
	{
		// Remove the newline character and parse
		trim_newline(line);

		const char *errstr = NULL;
		int err;

		err = parse_config_line(&menu[menu_size], line, &errstr);

		if (err < 0)
		{
			// Parsing error
			fprintf(stderr, "Failed parsing config!\nOn line %d: %s\n", line_number, errstr);
			fail = 1;
			break;
		}
		else
			menu_size += err;
	}
	free(line);

	// Check for duplicated MIDI controllers
	char found[128] = {0};
	for (int i = 0; i < menu_size; i++)
	{
		if (menu[i].type == ENTRY_MIDI_CTL)
		{
			int id = menu[i].midi_ctl.cc;
			if (found[id])
			{
				fprintf(stderr, "Duplicate %d controller found!\n", id);
				fail = 1;
				break;
			}
			
			found[id] = 1;
		}
	}
	
	// Exit with error
	if (fail)
	{
		for (int i = 0; i < menu_size; i++)
			free(menu[i].text);
		free(menu);
		*count = 0;
		return NULL;
	}

	*count = menu_size;
	return menu;
}

/**
	Initializes config parser's regex
*/
int config_parser_init(void)
{
	int err = 0;

	err |= regcomp(&midi_ctl_regex, midi_ctl_regex_str, REG_EXTENDED);
	assert(!err && "Failed to compile regex for matching ctl lines");

	err |= regcomp(&metadata_regex, metadata_regex_str, REG_EXTENDED);
	assert(!err && "Failed to compile metadata regex");

	return err;
}

void config_parser_destroy(void)
{
	regfree(&midi_ctl_regex);
	regfree(&metadata_regex);
}