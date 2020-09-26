#ifndef WLCLOCK_RENDER_H
#define WLCLOCK_RENDER_H

#include<stdbool.h>

struct Wlclock_surface;

void schedule_frame (struct Wlclock_surface *surface, bool background, bool hands);

#endif
