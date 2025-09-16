#include "execute.h"
#include "jobs.h"
#include "signals.h" // Now includes the extern declaration
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

void execute_job(Job *job) {
    pid_t pid;
    int is_foreground = !job->is_background;

    pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) { // Child process
        // Put the child in a new process group
        setpgid(0, 0);

        if (is_foreground) {
            tcsetpgrp(STDIN_FILENO, getpgrp());
        }

        // Child process should handle signals by default
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);

        int prev_pipe_read_end = STDIN_FILENO;
        int pipe_fds[2];

        for (int i = 0; i < job->command_count; i++) {
            Command *cmd = &job->commands[i];
            if (i < job->command_count - 1) {
                if (pipe(pipe_fds) == -1) {
                    perror("pipe");
                    exit(1);
                }
            }
            pid_t cmd_pid = fork();
            if (cmd_pid < 0) {
                perror("fork");
                exit(1);
            }
            if (cmd_pid == 0) {
                if (prev_pipe_read_end != STDIN_FILENO) {
                    dup2(prev_pipe_read_end, STDIN_FILENO);
                    close(prev_pipe_read_end);
                }
                if (i < job->command_count - 1) {
                    close(pipe_fds[0]);
                    dup2(pipe_fds[1], STDOUT_FILENO);
                    close(pipe_fds[1]);
                }
                if (cmd->input_file) {
                    int in_fd = open(cmd->input_file, O_RDONLY);
                    if (in_fd < 0) {
                        fprintf(stderr, "No such file or directory: %s\n", cmd->input_file);
                        exit(1);
                    }
                    dup2(in_fd, STDIN_FILENO);
                    close(in_fd);
                }
                if (cmd->output_file) {
                    int flags = O_WRONLY | O_CREAT | (cmd->append_mode ? O_APPEND : O_TRUNC);
                    int out_fd = open(cmd->output_file, flags, 0644);
                    if (out_fd < 0) {
                        fprintf(stderr, "Cannot open output file: %s\n", cmd->output_file);
                        exit(1);
                    }
                    dup2(out_fd, STDOUT_FILENO);
                    close(out_fd);
                }
                execvp(cmd->argv[0], cmd->argv);
                fprintf(stderr, "Command not found: %s\n", cmd->argv[0]);
                exit(1);
            }
            if (prev_pipe_read_end != STDIN_FILENO) close(prev_pipe_read_end);
            if (i < job->command_count - 1) {
                close(pipe_fds[1]);
                prev_pipe_read_end = pipe_fds[0];
            }
        }
        for (int i = 0; i < job->command_count; i++) {
            wait(NULL);
        }
        exit(0);
    } else { // Parent process
        if (is_foreground) {
            foreground_pgid = pid;
            tcsetpgrp(STDIN_FILENO, pid);
            int status;
            waitpid(pid, &status, WUNTRACED);
            tcsetpgrp(STDIN_FILENO, shell_pgid);

            if (WIFSTOPPED(status)) {
                 add_stopped_job(pid, job);
            }
            foreground_pgid = 0;
        } else {
            add_job(pid, job, RUNNING);
        }
    }
}