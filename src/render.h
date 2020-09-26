#ifndef WLCLOCK_RENDER_H
#define WLCLOCK_RENDER_H

#include<stdbool.h>

struct Wlclock_surface;

void render_background_frame (struct Wlclock_surface *surface);
void render_hands_frame (struct Wlclock_surface *surface);

#endif
