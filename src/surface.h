#ifndef WLCLOCK_SURFACE_H
#define WLCLOCK_SURFACE_H

#include<wayland-server.h>

#include"buffer.h"
#include"wlclock.h"

#include<stdint.h>
#include<stdbool.h>

struct Wlclock;
struct Wlclock_output;

struct Wlclock_surface
{
	struct Wlclock_output        *output;
	struct wl_surface            *background_surface;
	struct wl_surface            *hands_surface;
	struct wl_subsurface         *subsurface;
	struct zwlr_layer_surface_v1 *layer_surface;

	struct Wlclock_dimensions dimensions;
	struct Wlclock_buffer  background_buffers[2];
	struct Wlclock_buffer *current_background_buffer;
	struct Wlclock_buffer  hands_buffers[2];
	struct Wlclock_buffer *current_hands_buffer;
	bool configured;
};

bool create_surface (struct Wlclock_output *output);
void destroy_surface (struct Wlclock_surface *surface);
void update_surface (struct Wlclock_surface *surface);
void update_all_hands (struct Wlclock *clock);

#endif
