#include "intrinsics.h"
#include "parser.h"
#include "execute.h"
#include "jobs.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/wait.h> // needed for waitpid()

void dispatch_job(Job *job);

#define MAX_HISTORY_SIZE 15
#define HISTORY_FILE ".shell_log.txt"

extern char shell_home_dir[1024];
extern pid_t shell_pgid;

static char previous_dir[1024] = "";
static char *history[MAX_HISTORY_SIZE];
static int history_count = 0;

static int compare_strings(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

static int compare_jobs_by_name(const void *a, const void *b) {
    BackgroundJob *job_a = *(BackgroundJob **)a;
    BackgroundJob *job_b = *(BackgroundJob **)b;
    return strcmp(job_a->command_name, job_b->command_name);
}



/* ---------------- HISTORY ---------------- */

void init_history() {
    char history_path[2048];
    snprintf(history_path, sizeof(history_path), "%s/%s", shell_home_dir, HISTORY_FILE);
    FILE *file = fopen(history_path, "r");
    if (file == NULL) return;
    char *line = NULL;
    size_t len = 0;
    while (getline(&line, &len, file) != -1) {
        line[strcspn(line, "\n")] = 0;
        if (history_count < MAX_HISTORY_SIZE) {
            history[history_count++] = strdup(line);
        }
    }
    free(line);
    fclose(file);
}

void save_history() {
    char history_path[2048];
    snprintf(history_path, sizeof(history_path), "%s/%s", shell_home_dir, HISTORY_FILE);
    FILE *file = fopen(history_path, "w");
    if (file == NULL) { perror("Failed to save history"); return; }
    for (int i = 0; i < history_count; i++) {
        fprintf(file, "%s\n", history[i]);
        free(history[i]);
    }
    fclose(file);
}

void add_to_history(const char *command) {
    if (strncmp(command, "log", 3) == 0 || strncmp(command, "activities", 10) == 0) return;
    if (history_count > 0 && strcmp(history[history_count - 1], command) == 0) return;
    if (history_count < MAX_HISTORY_SIZE) {
        history[history_count++] = strdup(command);
    } else {
        free(history[0]);
        for (int i = 1; i < MAX_HISTORY_SIZE; i++) {
            history[i - 1] = history[i];
        }
        history[MAX_HISTORY_SIZE - 1] = strdup(command);
    }
}

void execute_log(char *tokens[]) {
    if (tokens[1] == NULL) {
        for (int i = 0; i < history_count; i++) {
            printf("%s\n", history[i]);
        }
    } else if (strcmp(tokens[1], "purge") == 0) {
        for (int i = 0; i < history_count; i++) {
            free(history[i]);
        }
        history_count = 0;
        char history_path[2048];
        snprintf(history_path, sizeof(history_path), "%s/%s", shell_home_dir, HISTORY_FILE);
        fclose(fopen(history_path, "w"));
    } else if (strcmp(tokens[1], "execute") == 0) {
        if (tokens[2] == NULL) {
            printf("log: execute requires an index.\n");
            return;
        }
        int index = atoi(tokens[2]);
        if (index <= 0 || index > history_count) {
            printf("log: Invalid history index.\n");
            return;
        }
        char *command_to_run_raw = history[history_count - index];
        printf("Executing: %s\n", command_to_run_raw);
        char command_copy[1024];
        strcpy(command_copy, command_to_run_raw);
        Job jobs[MAX_JOBS_PER_LINE];
        int job_count = parse_jobs(command_copy, jobs);
        for (int i = 0; i < job_count; i++) {
            dispatch_job(&jobs[i]);
        }
    } else {
        printf("log: Invalid Syntax!\n");
    }
}

/* ---------------- hop ---------------- */


void execute_hop(char *tokens[]) {
    char current_dir[1024];
    if (getcwd(current_dir, sizeof(current_dir)) == NULL) {
        perror("getcwd failed");
        return;
    }

    if (tokens[1] == NULL) { // Handle "hop" with no arguments
        chdir(shell_home_dir);
        strcpy(previous_dir, current_dir);
        return;
    }

    for (int i = 1; tokens[i] != NULL; i++) {
        char* target_dir = tokens[i];
        int result;

        if (strcmp(target_dir, "~") == 0) {
            result = chdir(shell_home_dir);
        } else if (strcmp(target_dir, "-") == 0) {
            if (strlen(previous_dir) == 0) {
                fprintf(stderr, "hop: OLDPWD not set\n");
                continue; // Continue to next argument
            }
            result = chdir(previous_dir);
            if (result == 0) {
                // Swap previous and current directories for the next iteration
                char temp[1024];
                strcpy(temp, previous_dir);
                strcpy(previous_dir, current_dir);
                strcpy(current_dir, temp);
                printf("%s\n", current_dir);
            }
        } else {
            result = chdir(target_dir);
        }

        if (result == -1) {
            fprintf(stderr, "hop: No such file or directory: %s\n", target_dir);
            // On failure, stop processing further arguments
            return; 
        } else {
            // On success, update previous_dir and get new current_dir
            strcpy(previous_dir, current_dir);
            getcwd(current_dir, sizeof(current_dir));
        }
    }
}
/* ---------------- reveal ---------------- */

void execute_reveal(char *tokens[]) {
    int show_hidden = 0, long_format = 0;
    char *path_arg = ".";
    int path_count = 0;
    for (int i = 1; tokens[i] != NULL; i++) {
        if (tokens[i][0] == '-') {
            for (size_t j = 1; j < strlen(tokens[i]); j++) {
                if (tokens[i][j] == 'a') show_hidden = 1;
                else if (tokens[i][j] == 'l') long_format = 1;
            }
        } else { 
            path_arg = tokens[i];
            path_count++;
        }
    }
    if(path_count > 1) {
        printf("reveal: Invalid Syntax!\n");
        return;
    }
    char target_path[1024];
    if (strcmp(path_arg, "~") == 0) strcpy(target_path, shell_home_dir);
    else strcpy(target_path, path_arg);
    DIR *dir = opendir(target_path);
    if (dir == NULL) { printf("reveal: No such directory!\n"); return; }
    char *entries[1024];
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && count < 1023) {
        if (!show_hidden && entry->d_name[0] == '.') continue;
        entries[count++] = strdup(entry->d_name);
    }
    closedir(dir);
    qsort(entries, count, sizeof(char *), compare_strings);
    for (int i = 0; i < count; i++) {
        printf("%s%s", entries[i], (long_format ? "\n" : "  "));
        free(entries[i]);
    }
    if (!long_format && count > 0) printf("\n");
}

/* ---------------- activities ---------------- */

void execute_activities(char *tokens[]) {
    BackgroundJob *active_jobs[MAX_ACTIVE_JOBS];
    int active_count = 0;

    for (int i = 0; i < MAX_ACTIVE_JOBS; i++) {
        if (job_list[i].pid > 0) {
            active_jobs[active_count++] = &job_list[i];
        }
    }

    if (active_count == 0) {
        return;
    }
    qsort(active_jobs, active_count, sizeof(BackgroundJob *), compare_jobs_by_name);

    for (int i = 0; i < active_count; i++) {
        const char *state_str = (active_jobs[i]->state == RUNNING) ? "Running" : "Stopped";
        printf("[%d] : %s - %s\n", active_jobs[i]->pid, active_jobs[i]->command_name, state_str);
    }
}

/* ---------------- ping ---------------- */

void execute_ping(char *tokens[]) {
    if (tokens[1] == NULL || tokens[2] == NULL) {
        fprintf(stderr, "ping: Invalid syntax!\n");
        return;
    }
    pid_t pid = atoi(tokens[1]);
    int signal_number = atoi(tokens[2]);

    if (pid == 0 && strcmp(tokens[1], "0") != 0) {
        fprintf(stderr, "ping: Invalid PID!\n");
        return;
    }

    int actual_signal = signal_number % 32;

    if (kill(pid, actual_signal) == -1) {
        fprintf(stderr, "No such process found\n");
    } else {
        printf("Sent signal %d to process with pid %d\n", actual_signal, pid);
    }
}

/* ---------------- fg ---------------- */

void execute_fg(char *tokens[]) {
    int job_num = 0; // Default to most recent
    if(tokens[1] != NULL) {
        job_num = atoi(tokens[1]);
    }
    BackgroundJob* job = find_job_by_number(job_num);

    if(job == NULL) {
        fprintf(stderr, "fg: job not found: %s\n", tokens[1] ? tokens[1] : "most recent");
        return;
    }

    printf("%s\n", job->command_name);

    tcsetpgrp(STDIN_FILENO, job->pgid);

    if(job->state == STOPPED) {
        if(kill(-job->pgid, SIGCONT) < 0) {
            perror("kill (SIGCONT)");
        }
    }
    
    int status;
    if(waitpid(job->pid, &status, WUNTRACED) < 0) {
        perror("waitpid");
    }

    tcsetpgrp(STDIN_FILENO, shell_pgid);

    if(WIFSTOPPED(status)) {
        job->state = STOPPED;
        fprintf(stderr, "\n[%d] Stopped %s\n", job->job_number, job->command_name);
    } else {
        job->pid = 0; 
    }
}

/* ---------------- bg ---------------- */

void execute_bg(char *tokens[]) {
    int job_num = 0; // Default to most recent
    if(tokens[1] != NULL) {
        job_num = atoi(tokens[1]);
    }
    BackgroundJob* job = find_job_by_number(job_num);

    if(job == NULL) {
        fprintf(stderr, "bg: job not found: %s\n", tokens[1] ? tokens[1] : "most recent");
        return;
    }

    if(job->state == RUNNING) {
        fprintf(stderr, "bg: job %d already running in background\n", job->job_number);
        return;
    }

    if(kill(-job->pgid, SIGCONT) < 0) {
        perror("kill (SIGCONT)");
    }
    job->state = RUNNING;

    printf("[%d] %s &\n", job->job_number, job->command_name);
}
