#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/time.h>
#include <time.h>
#include "sham.h"

// Global log file pointer, static means it's only visible within this file.
static FILE *log_file = NULL;

// Function to initialize the logger based on environment variable
void init_logger(const char* filename) {
    if (getenv("RUDP_LOG") != NULL) {
        log_file = fopen(filename, "w");
        if (log_file == NULL) {
            perror("Failed to open log file");
        }
    }
}

// Function to close the logger
void close_logger(void) {
    if (log_file != NULL) {
        fclose(log_file);
    }
}

// The main logging function as required
void log_event(const char *format, ...) {
    if (log_file == NULL) {
        return; // Logging is disabled
    }

    char time_buffer[30];
    struct timeval tv;
    time_t curtime;

    gettimeofday(&tv, NULL);
    curtime = tv.tv_sec;

    strftime(time_buffer, 30, "%Y-%m-%d %H:%M:%S", localtime(&curtime));

    // Print the timestamp prefix
    fprintf(log_file, "[%s.%06ld] [LOG] ", time_buffer, tv.tv_usec);

    // Print the actual log message
    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);

    fprintf(log_file, "\n");
    fflush(log_file); // Ensure log is written immediately so you can tail the file
}

// Function to simulate packet loss
int should_drop(float loss_rate) {
    if (loss_rate <= 0.0) {
        return 0; // No loss
    }
    // Generate a random float between 0.0 and 1.0
    double random_val = (double)rand() / (double)RAND_MAX;
    return random_val < loss_rate;
}