#include "parser.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

// Helper to trim leading/trailing whitespace from a string
static char* trim_whitespace(char *str) {
    if (!str) return NULL;
    char *end;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

// FINAL, CORRECTED TOKENIZER
static int tokenize_simple_command(char *command_str, Command *cmd) {
    memset(cmd, 0, sizeof(Command));
    int argc = 0;
    char *current = command_str;

    while (*current != '\0' && argc < MAX_TOKENS - 1) {
        while (*current && isspace((unsigned char)*current)) current++;
        if (*current == '\0') break;

        char *token_start = current;
        if (*current == '"') {
            token_start++;
            current++;
            while (*current && *current != '"') current++;
            if (*current == '"') {
                *current = '\0';
                current++;
            } else {
                printf("Invalid Syntax: Unmatched quote!\n");
                return -1;
            }
        } else {
            while (*current && !isspace((unsigned char)*current)) current++;
        }
        
        char temp_char = *current;
        if(temp_char != '\0') *current = '\0';
        
        if (strcmp(token_start, "<") == 0) {
            *current = temp_char;
            while (*current && isspace((unsigned char)*current)) current++;
            if(*current == '\0' || strchr("<>|;&", *current)) { printf("Invalid Syntax!\n"); return -1; }
            cmd->input_file = current;
            while (*current && !isspace((unsigned char)*current)) current++;
            if(*current != '\0') *current = '\0';
        } else if (strcmp(token_start, ">") == 0 || strcmp(token_start, ">>") == 0) {
            cmd->append_mode = (strcmp(token_start, ">>") == 0);
            *current = temp_char;
            while (*current && isspace((unsigned char)*current)) current++;
            if(*current == '\0' || strchr("<>|;&", *current)) { printf("Invalid Syntax!\n"); return -1; }
            cmd->output_file = current;
            while (*current && !isspace((unsigned char)*current)) current++;
            if(*current != '\0') *current = '\0';
        } else {
            cmd->argv[argc++] = token_start;
        }

        if(temp_char != '\0') current++;
    }
    cmd->argv[argc] = NULL;
    return (argc > 0);
}


// YOUR ORIGINAL CORRECT FUNCTION
static int parse_pipeline(char *pipeline_str, Job *job) {
    job->command_count = 0;
    char *current_pos = pipeline_str;
    char *pipe_pos;

    while ((pipe_pos = strchr(current_pos, '|')) != NULL) {
        *pipe_pos = '\0';
        char *trimmed_chunk = trim_whitespace(current_pos);
        if (*trimmed_chunk == '\0') {
            printf("Invalid Syntax!\n");
            return -1;
        }
        if (tokenize_simple_command(trimmed_chunk, &job->commands[job->command_count]) > 0) {
            job->command_count++;
        } else if (job->commands[job->command_count].input_file || job->commands[job->command_count].output_file) {
            job->command_count++;
        } else {
            return -1;
        }
        current_pos = pipe_pos + 1;
    }
    
    char *trimmed_chunk = trim_whitespace(current_pos);
    if (*trimmed_chunk == '\0') {
        if (job->command_count > 0) {
             printf("Invalid Syntax!\n");
             return -1;
        }
    } else {
        if (tokenize_simple_command(trimmed_chunk, &job->commands[job->command_count]) > 0) {
            job->command_count++;
        } else if (job->commands[job->command_count].input_file || job->commands[job->command_count].output_file) {
             job->command_count++;
        } else {
             return -1;
        }
    }
    return (job->command_count > 0);
}

// YOUR ORIGINAL CORRECT FUNCTION
int parse_jobs(char *line, Job jobs[]) {
    int job_count = 0;
    char *current_pos = line;
    char *semicolon_pos;

    while ((semicolon_pos = strchr(current_pos, ';')) != NULL && job_count < MAX_JOBS_PER_LINE) {
        *semicolon_pos = '\0';
        char *trimmed_chunk = trim_whitespace(current_pos);
        if (*trimmed_chunk == '\0') {
            printf("Invalid Syntax!\n");
            return 0;
        }
        if (parse_pipeline(trimmed_chunk, &jobs[job_count]) > 0) {
            jobs[job_count].is_background = 0;
            job_count++;
        } else {
            return 0;
        }
        current_pos = semicolon_pos + 1;
    }

    if (job_count < MAX_JOBS_PER_LINE) {
        char *trimmed_chunk = trim_whitespace(current_pos);
        if (*trimmed_chunk != '\0') {
            jobs[job_count].is_background = 0;
            size_t len = strlen(trimmed_chunk);
            if (len > 0 && trimmed_chunk[len - 1] == '&') {
                jobs[job_count].is_background = 1;
                trimmed_chunk[len - 1] = '\0';
                trimmed_chunk = trim_whitespace(trimmed_chunk);
            }

            if (*trimmed_chunk == '\0' && jobs[job_count].is_background) {
                 printf("Invalid Syntax!\n");
                 return 0;
            }

            if(strlen(trimmed_chunk) > 0) {
                if (parse_pipeline(trimmed_chunk, &jobs[job_count]) > 0) {
                    job_count++;
                } else {
                    return 0;
                }
            }
        }
    }
    return job_count;
}