#ifndef WLCLOCK_MISC_H
#define WLCLOCK_MISC_H

#include<stdbool.h>

struct Wlclock;

void free_if_set (void *ptr);
void set_string (char **ptr, char *arg);
void clocklog (struct Wlclock *clock, int level, const char *fmt, ...);
bool is_boolean_true (const char *in);
bool is_boolean_false (const char *in);

#endif
