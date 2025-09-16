#include "signals.h"
#include "jobs.h"
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

volatile pid_t foreground_pgid = 0;

void sigint_handler(int signum) {
    if (foreground_pgid != 0) {
        kill(-foreground_pgid, SIGINT);
    }
    write(STDOUT_FILENO, "\n", 1);
}

void sigtstp_handler(int signum) {
    if (foreground_pgid != 0) {
        kill(-foreground_pgid, SIGTSTP);
    }
    // No 'else' block needed. The main loop will handle the visual refresh.
}

void setup_shell_signals() {
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    struct sigaction sa;
    
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction for SIGINT failed");
    }

    sa.sa_handler = sigtstp_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGTSTP, &sa, NULL) == -1) {
        perror("sigaction for SIGTSTP failed");
    }
}