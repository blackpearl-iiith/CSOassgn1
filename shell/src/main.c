#include "prompt.h"
#include "parser.h"
#include "intrinsics.h"
#include "execute.h"
#include "jobs.h"
#include "signals.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

char shell_home_dir[1024];

void dispatch_job(Job *job) {
    if (job->command_count > 0) {
        // This is a special case for intrinsics, which don't support pipes.
        // We only check the first command in a potential pipeline.
        char *first_command = job->commands[0].argv[0];

        if (strcmp(first_command, "hop") == 0) {
            execute_hop(job->commands[0].argv);
        } else if (strcmp(first_command, "reveal") == 0) {
            execute_reveal(job->commands[0].argv);
        } else if (strcmp(first_command, "log") == 0) {
            execute_log(job->commands[0].argv);
        } else if (strcmp(first_command, "activities") == 0) {
            execute_activities(job->commands[0].argv);
        } else if (strcmp(first_command, "ping") == 0) {
            execute_ping(job->commands[0].argv);
        } else if (strcmp(first_command, "fg") == 0) {
            execute_fg(job->commands[0].argv);
        } else if (strcmp(first_command, "bg") == 0) {
            execute_bg(job->commands[0].argv);
        } else {
            // If it's not an intrinsic, it's an external command.
            execute_job(job);
        }
    }
}

int main() {
    if (!isatty(STDIN_FILENO)) { return 1; }
    shell_pgid = getpgrp();
    // if (setpgid(shell_pgid, shell_pgid) < 0) {
    //     perror("setpgid"); exit(1);
    // }
    tcsetpgrp(STDIN_FILENO, shell_pgid);
    setup_shell_signals();
    
    if (getcwd(shell_home_dir, sizeof(shell_home_dir)) == NULL) {
        perror("getcwd() error");
        return 1;
    }
    init_history();
    init_jobs();

    while (1) {
        display_prompt();
        
        char *line = NULL;
        size_t len = 0;
        ssize_t nread = getline(&line, &len, stdin);

        check_completed_jobs();

        if (nread == -1) {
            if (errno == EINTR) {
                // --- THE FIX IS HERE ---
                // A signal (like Ctrl-C/Z) interrupted getline.
                // Print a newline to ensure the next prompt starts fresh.
                printf("\n");
                clearerr(stdin);
                free(line);
                continue;
            } else {
                printf("logout\n");
                save_history();
                for(int i = 0; i < MAX_ACTIVE_JOBS; i++) {
                    if(job_list[i].pid > 0) {
                        kill(-job_list[i].pgid, SIGKILL);
                    }
                }
                break;
            }
        }

        line[strcspn(line, "\n")] = 0;
        if (strlen(line) > 0) {
            add_to_history(line);
        }

        char line_copy[1024];
        strcpy(line_copy, line);
        
        Job jobs[MAX_JOBS_PER_LINE];
        int job_count = parse_jobs(line_copy, jobs);

        for (int i = 0; i < job_count; i++) {
            dispatch_job(&jobs[i]);
        }
        free(line);
    }
    return 0;
}