#ifndef WLCLOCK_WLCLOCK_H
#define WLCLOCK_WLCLOCK_H

#include<stdbool.h>
#include<stdint.h>
#include<time.h>
#include<wayland-server.h>

#include"wlr-layer-shell-unstable-v1-protocol.h"

struct Wlclock
{
	struct wl_display             *display;
	struct wl_registry            *registry;
	struct wl_compositor          *compositor;
	struct wl_shm                 *shm;
	struct zwlr_layer_shell_v1    *layer_shell;
	struct zxdg_output_manager_v1 *xdg_output_manager;

	struct wl_list outputs;
	char *output;

	time_t now;

	bool loop;
	int  verbosity;
	int  ret;

	enum zwlr_layer_shell_v1_layer layer;
	uint32_t size;
	char *namespace;
	int32_t exclusive_zone;
	uint32_t border_top, border_right, border_bottom, border_left;
	uint32_t margin_top, margin_right, margin_bottom, margin_left;
	uint32_t radius_top_left, radius_top_right, radius_bottom_left, radius_bottom_right;
	uint32_t anchor;
	bool input;
};

#endif
