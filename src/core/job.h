#include <stdint.h>
#include <stdbool.h>

#pragma once

typedef void fn_job(void* arg);
typedef void fn_hook(uint32_t worker);

bool job_init(uint32_t workerCount, fn_hook* init, fn_hook* quit);
void job_destroy(void);
bool job_start(fn_job* fn, void* arg);
void job_spin(void);
