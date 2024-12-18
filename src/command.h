#ifndef COMMAND_H
#define COMMAND_H

#include "gear.h"
void parse_command(char *buf);
void repl(void);
void init_command_history(axel_t *root);
void display_command_history(void);

#endif /* COMMAND_H */
