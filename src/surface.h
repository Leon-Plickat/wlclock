#ifndef WLCLOCK_SURFACE_H
#define WLCLOCK_SURFACE_H

#include<wayland-server.h>

#include"buffer.h"

#include<stdint.h>
#include<stdbool.h>

struct Wlclock;
struct Wlclock_output;

struct Wlclock_surface
{
	struct Wlclock_output        *output;
	struct wl_surface            *surface;
	struct zwlr_layer_surface_v1 *layer_surface;

	int32_t size;
	struct Wlclock_buffer  buffers[2];
	struct Wlclock_buffer *current_buffer;
	bool configured;
};

bool create_surface (struct Wlclock_output *output);
void destroy_surface (struct Wlclock_surface *surface);
void destroy_all_surfaces (struct Wlclock *clock);
void update_surface (struct Wlclock_surface *surface);
void update_all_surfaces (struct Wlclock *clock);

#endif
