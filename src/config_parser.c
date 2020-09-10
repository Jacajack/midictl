#include "midictl.h"
#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <string.h>
#include <assert.h>
#include "utils.h"

static const char *midi_ctl_regex_str = "^\\s*([0-9]*)?\\s*(\\[(.*)\\])?\\s*(.*)$";
static regex_t midi_ctl_regex;

/**
	Parses metadata string and properly configures
	MIDI_CTL menu entry
*/
static int midi_ctl_apply_metadata(menu_entry *ctl, const char *metadata)
{
	char *meta = strdup(metadata);
	char *save_ptr = NULL;

	for (char *entry = strtok_r(meta, ",", &save_ptr); entry != NULL; entry = strtok_r(NULL, ",", &save_ptr))
	{
		// char key[1024];
		// int value;
		// sscanf(entry, "%1023[a-zA-Z]%*[\t ]=%d", key, value);
	}

	free(meta);

	return 0;
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
		sscanf(line, "---%*[ \t]%ms", &ent->text);
		return 1;
	}

	// Try to match to controller line format
	if (!regexec(&midi_ctl_regex, line, max_matches, matches, 0))
	{
		ent->type = ENTRY_MIDI_CTL;
		ent->cc = -1;
		ent->value = 64;

		// Match CC ID
		if (matches[1].rm_so >= 0)
			sscanf(line + matches[1].rm_so, "%d", &ent->cc);

		// Match metadata
		if (matches[3].rm_so >= 0)
		{
			char *metadata = strndup(line + matches[3].rm_so, matches[3].rm_eo - matches[3].rm_so);
			int err = midi_ctl_apply_metadata(ent, metadata);
			free(metadata);

			// Metadata parsing failed
			if (err)
			{
				*errstr = "Invalid metadata syntax!";
				return -1;
			}
		}

		// CC is not set
		if (ent->cc == -1)
		{
			*errstr = "MIDI CC value missing!";
			return -1;
		}

		// Check CC range
		if (ent->cc < 0 || ent->cc > 127)
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
		line[line_len - 1] = 0;

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
			int id = menu[i].cc;
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

	return err;
}

void config_parser_destroy(void)
{
	regfree(&midi_ctl_regex);
}