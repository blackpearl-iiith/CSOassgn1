#include "jobs.h"
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

BackgroundJob job_list[MAX_ACTIVE_JOBS];
static int next_job_number = 1;
static int last_job_number = 0; // New variable
pid_t shell_pgid;

void init_jobs() {
    for (int i = 0; i < MAX_ACTIVE_JOBS; i++) {
        job_list[i].pid = 0;
    }
}

static void get_job_command_string(Job *job_info, char *buffer, size_t buffer_size) {
    buffer[0] = '\0';
    for (int i = 0; i < job_info->command_count; i++) {
        for (int j = 0; job_info->commands[i].argv[j] != NULL; j++) {
            strncat(buffer, job_info->commands[i].argv[j], buffer_size - strlen(buffer) - 1);
            strncat(buffer, " ", buffer_size - strlen(buffer) - 1);
        }
        if (i < job_info->command_count - 1) {
            strncat(buffer, "| ", buffer_size - strlen(buffer) - 1);
        }
    }
    if (strlen(buffer) > 0 && buffer[strlen(buffer) - 1] == ' ') {
        buffer[strlen(buffer) - 1] = '\0';
    }
}

void add_job(pid_t pgid, Job *job_info, JobState state) {
    for (int i = 0; i < MAX_ACTIVE_JOBS; i++) {
        if (job_list[i].pid == 0) {
            job_list[i].pgid = pgid;
            job_list[i].pid = pgid; // For now, let's assume pgid is the lead process pid
            job_list[i].job_number = next_job_number;
            last_job_number = next_job_number; // Update the last job number
            next_job_number++;
            job_list[i].state = state;
            get_job_command_string(job_info, job_list[i].command_name, sizeof(job_list[i].command_name));
            if (state == RUNNING) {
                printf("[%d] %d\n", job_list[i].job_number, job_list[i].pid);
            }
            return;
        }
    }
    fprintf(stderr, "shell: too many background jobs\n");
}

void add_stopped_job(pid_t pgid, Job *job_info) {
    for (int i = 0; i < MAX_ACTIVE_JOBS; i++) {
        if (job_list[i].pid == 0) {
            job_list[i].pgid = pgid;
            job_list[i].pid = pgid;
            job_list[i].job_number = next_job_number;
            last_job_number = next_job_number;
            next_job_number++;
            job_list[i].state = STOPPED;
            // Use the helper to get the full command string
            get_job_command_string(job_info, job_list[i].command_name, sizeof(job_list[i].command_name));
            fprintf(stderr, "\n[%d] Stopped %s\n", job_list[i].job_number, job_list[i].command_name);
            return;
        }
    }
}

void check_completed_jobs() {
    for (int i = 0; i < MAX_ACTIVE_JOBS; i++) {
        if (job_list[i].pid > 0) {
            int status;
            pid_t result = waitpid(job_list[i].pid, &status, WNOHANG | WUNTRACED);
            if (result == job_list[i].pid) {
                if (WIFEXITED(status)) {
                    fprintf(stderr, "\n%s with pid %d exited normally\n", job_list[i].command_name, job_list[i].pid);
                    job_list[i].pid = 0;
                } else if(WIFSIGNALED(status)) {
                    fprintf(stderr, "\n%s with pid %d exited abnormally\n", job_list[i].command_name, job_list[i].pid);
                    job_list[i].pid = 0;
                } else if(WIFSTOPPED(status)) {
                    job_list[i].state = STOPPED;
                }
            }
        }
    }
}

BackgroundJob* find_job_by_number(int job_num) {
    // If job_num is 0, it means "find the most recent"
    if (job_num == 0) {
        job_num = last_job_number;
    }

    for (int i = 0; i < MAX_ACTIVE_JOBS; i++) {
        if (job_list[i].pid > 0 && job_list[i].job_number == job_num) {
            return &job_list[i];
        }
    }
    return NULL;
}
