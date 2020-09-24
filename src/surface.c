#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
#include<unistd.h>
#include<string.h>

#include<cairo/cairo.h>
#include<wayland-server.h>
#include<wayland-client.h>
#include<wayland-client-protocol.h>

#include"wlr-layer-shell-unstable-v1-protocol.h"
#include"xdg-output-unstable-v1-protocol.h"
#include"xdg-shell-protocol.h"

#include"wlclock.h"
#include"output.h"
#include"misc.h"
#include"surface.h"
#include"buffer.h"
#include"render.h"

static uint32_t min (uint32_t a, uint32_t b)
{
	return a > b ? b : a;
}

static void layer_surface_handle_configure (void *data,
		struct zwlr_layer_surface_v1 *layer_surface, uint32_t serial,
		uint32_t w, uint32_t h)
{
	struct Wlclock_surface *surface = (struct Wlclock_surface *)data;
	clocklog(surface->output->clock, 1,
			"[surface] Layer surface configure request: global_name=%d w=%d h=%d serial=%d\n",
			surface->output->global_name, w, h, serial);
	zwlr_layer_surface_v1_ack_configure(layer_surface, serial);
	if ( w > 0 && h > 0 ) /* Try to fit as best as possible. */
		surface->size = min(w, h);
	surface->configured = true;
	update_surface(surface);
}

static void layer_surface_handle_closed (void *data, struct zwlr_layer_surface_v1 *layer_surface)
{
	struct Wlclock_surface *surface = (struct Wlclock_surface *)data;
	clocklog(surface->output->clock, 1,
			"[surface] Layer surface has been closed: global_name=%d\n",
			surface->output->global_name);
	destroy_surface(surface);
}

const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
	.configure = layer_surface_handle_configure,
	.closed    = layer_surface_handle_closed
};

static int32_t get_exclusive_zone (struct Wlclock_surface *surface)
{
	if ( surface->output->clock->exclusive_zone == 1 )
		return surface->size;
	return surface->output->clock->exclusive_zone;
}

static void configure_layer_surface (struct Wlclock_surface *surface)
{
	struct Wlclock *clock = surface->output->clock;
	clocklog(clock, 1, "[surface] Configuring surface: global_name=%d\n",
			surface->output->global_name);
	zwlr_layer_surface_v1_set_size(surface->layer_surface,
			surface->size, surface->size);
	zwlr_layer_surface_v1_set_anchor(surface->layer_surface, clock->anchor);
	zwlr_layer_surface_v1_set_margin(surface->layer_surface,
			clock->margin_top, clock->margin_right,
			clock->margin_bottom, clock->margin_left);
	zwlr_layer_surface_v1_set_exclusive_zone(surface->layer_surface,
			get_exclusive_zone(surface));
	if (! clock->input)
	{
		struct wl_region *region = wl_compositor_create_region(clock->compositor);
		wl_surface_set_input_region(surface->surface, region);
		wl_region_destroy(region);
	}
}

bool create_surface (struct Wlclock_output *output)
{
	struct Wlclock *clock = output->clock;
	clocklog(clock, 1, "[surface] Creating surface: global_name=%d\n", output->global_name);

	struct Wlclock_surface *surface = calloc(1, sizeof(struct Wlclock_surface));
	if ( surface == NULL )
	{
		clocklog(NULL, 0, "ERROR: Could not allocate.\n");
		return false;
	}

	output->surface        = surface;
	surface->size          = clock->size;
	surface->output        = output;
	surface->surface       = NULL;
	surface->layer_surface = NULL;
	surface->configured    = false;

	if ( NULL == (surface->surface = wl_compositor_create_surface(clock->compositor)) )
	{
		clocklog(NULL, 0, "ERROR: Compositor did not create wl_surface.\n");
		return false;
	}
	if ( NULL == (surface->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
					clock->layer_shell, surface->surface,
					output->wl_output, clock->layer,
					clock->namespace)) )
	{
		clocklog(NULL, 0, "ERROR: Compositor did not create layer_surface.\n");
		return false;
	}

	configure_layer_surface(surface);
	zwlr_layer_surface_v1_add_listener(surface->layer_surface,
			&layer_surface_listener, surface);
	wl_surface_commit(surface->surface);

	return true;
}

void destroy_surface (struct Wlclock_surface *surface)
{
	if ( surface == NULL )
		return;
	if ( surface->layer_surface != NULL )
		zwlr_layer_surface_v1_destroy(surface->layer_surface);
	if ( surface->surface != NULL )
		wl_surface_destroy(surface->surface);
	finish_buffer(&surface->buffers[0]);
	finish_buffer(&surface->buffers[1]);
	free(surface);
}

void destroy_all_surfaces (struct Wlclock *clock)
{
	clocklog(clock, 1, "[surface] Destroying all surfaces.\n");
	struct Wlclock_output *op, *tmp;
	wl_list_for_each_safe(op, tmp, &clock->outputs, link)
		if ( op->surface != NULL )
			destroy_surface(op->surface);
}

void update_surface (struct Wlclock_surface *surface)
{
	if ( surface == NULL || ! surface->configured )
		return;
	configure_layer_surface(surface);
	render_surface_frame(surface);
	wl_surface_commit(surface->surface);
}

void update_all_surfaces (struct Wlclock *clock)
{
	clocklog(clock, 1, "[surface] Updating all surfaces.\n");
	struct Wlclock_output *op, *tmp;
	wl_list_for_each_safe(op, tmp, &clock->outputs, link)
		if ( op->surface != NULL )
			update_surface(op->surface);
}
