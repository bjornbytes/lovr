#include <stdint.h>
#include <stdbool.h>

#pragma once

typedef struct job job;
typedef void fn_job(job* job, void* arg);

bool job_init(uint32_t workerCount);
void job_destroy(void);
job* job_start(fn_job* fn, void* arg);
void job_abort(job* job, const char* error);
void job_wait(job* job);
char* job_get_error(job* job);
void job_free(job* job);
