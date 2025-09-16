#ifndef INTRINSICS_H
#define INTRINSICS_H

#include "parser.h"

void init_history();
void save_history();
void add_to_history(const char *command);

void execute_hop(char *tokens[]);
void execute_reveal(char *tokens[]);
void execute_log(char *tokens[]);
void execute_activities(char *tokens[]);
void execute_ping(char *tokens[]);
void execute_fg(char *tokens[]);
void execute_bg(char *tokens[]);

#endif // INTRINSICS_H