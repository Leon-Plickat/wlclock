#ifndef WLCLOCK_BUFFER_H
#define WLCLOCK_BUFFER_H

#include<stdint.h>
#include<stdbool.h>
#include<cairo/cairo.h>
#include<wayland-client.h>

struct Wlclock_buffer
{
	struct wl_buffer *buffer;
	cairo_surface_t  *surface;
	cairo_t          *cairo;
	uint32_t          w;
	uint32_t          h;
	void             *memory_object;
	size_t            size;
	bool              busy;
};

bool next_buffer (struct Wlclock_buffer **buffer, struct wl_shm *shm,
		struct Wlclock_buffer buffers[static 2], uint32_t w, uint32_t h);
void finish_buffer (struct Wlclock_buffer *buffer);

#endif
