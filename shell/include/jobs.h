#ifndef JOBS_H
#define JOBS_H

#include "parser.h"
#include <sys/types.h>

#define MAX_ACTIVE_JOBS 32

typedef enum {
    RUNNING,
    STOPPED
} JobState;

typedef struct {
    pid_t pid;
    pid_t pgid; // Process Group ID
    char command_name[1024];
    int job_number;
    JobState state;
} BackgroundJob;

void init_jobs();
void add_job(pid_t pgid, Job *job_info, JobState state);
void add_stopped_job(pid_t pgid, Job *job_info);
void check_completed_jobs();
BackgroundJob* find_job_by_number(int job_num);

extern BackgroundJob job_list[MAX_ACTIVE_JOBS];
extern pid_t shell_pgid;

#endif // JOBS_H