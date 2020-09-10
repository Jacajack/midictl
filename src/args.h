#ifndef ARGS_H
#define ARGS_H

#include <argp.h>
#include "midictl.h"

extern const char *argp_program_version;
extern const char *argp_program_bug_address;
extern char argp_doc[];
extern char argp_keydoc[];
extern struct argp_option argp_options[];

extern error_t args_parser(int key, char *arg, struct argp_state *state);
extern int args_config_interpret(midictl_args *conf);

#endif