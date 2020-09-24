#ifndef WLCLOCK_OUTPUT_H
#define WLCLOCK_OUTPUT_H

#include<wayland-server.h>

struct Wlclock;
struct Wlclock_surface;

struct Wlclock_output
{
	struct wl_list  link;
	struct Wlclock *clock;

	struct wl_output      *wl_output;
	struct zxdg_output_v1 *xdg_output;

	char     *name;
	uint32_t  global_name;
	uint32_t  scale;

	bool configured;

	struct Wlclock_surface *surface;
};

bool create_output (struct Wlclock *clock, struct wl_registry *registry,
		uint32_t name, const char *interface, uint32_t version);
bool configure_output (struct Wlclock_output *output);
struct Wlclock_output *get_output_from_global_name (struct Wlclock *clock, uint32_t name);
void destroy_output (struct Wlclock_output *output);
void destroy_all_outputs (struct Wlclock *clock);

#endif
