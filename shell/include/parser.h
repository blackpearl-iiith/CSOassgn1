#ifndef PARSER_H
#define PARSER_H

#define MAX_TOKENS 128
#define MAX_COMMANDS 16
#define MAX_JOBS_PER_LINE 16

typedef struct {
    char *argv[MAX_TOKENS];
    char *input_file;
    char *output_file;
    int append_mode;
} Command;

typedef struct {
    Command commands[MAX_COMMANDS];
    int command_count;
    int is_background;
} Job;

int parse_jobs(char *line, Job jobs[]);

#endif // PARSER_H