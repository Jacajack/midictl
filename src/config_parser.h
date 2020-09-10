#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include <stdio.h>
#include "midictl.h"

extern menu_entry *build_menu_from_config_file(FILE *f, int *count);
extern int config_parser_init(void);
extern void config_parser_destroy(void);

#endif