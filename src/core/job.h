#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>

#pragma once

typedef struct job job;
typedef void fn_job(void* arg);

bool job_init(uint32_t workerCount);
void job_destroy(void);
job* job_start(fn_job* fn, void* arg);
void job_wait(job* job);
