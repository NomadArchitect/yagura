#pragma once

#include "api/time.h"
#include <common/extra.h>
#include <stdatomic.h>

#define CLK_TCK 250

struct timespec;

extern volatile atomic_uint uptime;

void timespec_add(struct timespec*, const struct timespec*);
void timespec_saturating_sub(struct timespec*, const struct timespec*);
int timespec_compare(const struct timespec*, const struct timespec*);

void time_init(void);
void time_tick(void);
NODISCARD int time_now(clockid_t, struct timespec*);
NODISCARD int time_set(clockid_t, const struct timespec*);
NODISCARD int time_get_resolution(clockid_t, struct timespec*);
