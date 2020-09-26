#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
#include<unistd.h>
#include<string.h>
#include<time.h>

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

static void layer_surface_handle_configure (void *data,
		struct zwlr_layer_surface_v1 *layer_surface, uint32_t serial,
		uint32_t w, uint32_t h)
{
	struct Wlclock_surface *surface = (struct Wlclock_surface *)data;
	struct Wlclock         *clock   = surface->output->clock;
	clocklog(clock, 1, "[surface] Layer surface configure request: global_name=%d w=%d h=%d serial=%d\n",
			surface->output->global_name, w, h, serial);
	zwlr_layer_surface_v1_ack_configure(layer_surface, serial);
	if ( (w != (uint32_t)surface->dimensions.w || h != (uint32_t)surface->dimensions.h)
			&& (w > 0 && h > 0) )
	{
		/* Try to fit into the space the compositor wants us to occupy
		 * while also keeping the center square and not changing the
		 * border sizes.
		 */
		int32_t size_a = (int32_t)w - clock->border_left - clock->border_right;
		int32_t size_b = (int32_t)h - clock->border_top  - clock->border_bottom;
		surface->dimensions.center_size = size_a < size_b ? size_a : size_b;
		if ( surface->dimensions.center_size < 10 )
			surface->dimensions.center_size = 10;
	}

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
	struct Wlclock *clock = surface->output->clock;
	if ( clock->exclusive_zone == 1 ) switch (clock->anchor)
	{
		case ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM:
		case ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP:
			return surface->dimensions.h;

		case ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT:
		case ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT:
			return surface->dimensions.w;

		default:
			return 0;
	}
	else
		return surface->output->clock->exclusive_zone;
}

static void configure_subsurface (struct Wlclock_surface *surface)
{
	clocklog(surface->output->clock, 1, "[surface] Configuring sub surface: global_name=%d\n",
			surface->output->global_name);
	wl_subsurface_set_position(surface->subsurface,
			surface->dimensions.center_x, surface->dimensions.center_y);
	struct wl_region *region = wl_compositor_create_region(surface->output->clock->compositor);
	wl_surface_set_input_region(surface->hands_surface, region);
	wl_region_destroy(region);
}

static void configure_layer_surface (struct Wlclock_surface *surface)
{
	struct Wlclock *clock = surface->output->clock;
	clocklog(clock, 1, "[surface] Configuring layer surface: global_name=%d\n",
			surface->output->global_name);
	zwlr_layer_surface_v1_set_size(surface->layer_surface,
			surface->dimensions.w, surface->dimensions.h);
	zwlr_layer_surface_v1_set_anchor(surface->layer_surface, clock->anchor);
	zwlr_layer_surface_v1_set_margin(surface->layer_surface,
			clock->margin_top, clock->margin_right,
			clock->margin_bottom, clock->margin_left);
	zwlr_layer_surface_v1_set_exclusive_zone(surface->layer_surface,
			get_exclusive_zone(surface));
	if (! clock->input)
	{
		struct wl_region *region = wl_compositor_create_region(clock->compositor);
		wl_surface_set_input_region(surface->background_surface, region);
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

	output->surface             = surface;
	surface->dimensions         = clock->dimensions;
	surface->output             = output;
	surface->background_surface = NULL;
	surface->hands_surface      = NULL;
	surface->layer_surface      = NULL;
	surface->configured         = false;
	surface->frame_callback     = NULL;
	surface->background_dirty   = false;
	surface->hands_dirty        = false;

	if ( NULL == (surface->background_surface = wl_compositor_create_surface(clock->compositor)) )
	{
		clocklog(NULL, 0, "ERROR: Compositor did not create wl_surface (background).\n");
		return false;
	}
	if ( NULL == (surface->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
					clock->layer_shell, surface->background_surface,
					output->wl_output, clock->layer,
					clock->namespace)) )
	{
		clocklog(NULL, 0, "ERROR: Compositor did not create layer_surface.\n");
		return false;
	}
	zwlr_layer_surface_v1_add_listener(surface->layer_surface,
			&layer_surface_listener, surface);

	if ( NULL == (surface->hands_surface = wl_compositor_create_surface(clock->compositor)) )
	{
		clocklog(NULL, 0, "ERROR: Compositor did not create wl_surface (hands).\n");
		return false;
	}
	if ( NULL == (surface->subsurface = wl_subcompositor_get_subsurface(
					clock->subcompositor, surface->hands_surface,
					surface->background_surface)) )
	{
		clocklog(NULL, 0, "ERROR: Compositor did not create wl_subsurface.\n");
		return false;
	}

	configure_layer_surface(surface);
	configure_subsurface(surface);

	wl_surface_commit(surface->hands_surface);
	wl_surface_commit(surface->background_surface);
	return true;
}

void destroy_surface (struct Wlclock_surface *surface)
{
	if ( surface == NULL )
		return;
	if ( surface->output != NULL )
		surface->output->surface = NULL;
	if ( surface->layer_surface != NULL )
		zwlr_layer_surface_v1_destroy(surface->layer_surface);
	if ( surface->subsurface != NULL )
		wl_subsurface_destroy(surface->subsurface);
	if ( surface->background_surface != NULL )
		wl_surface_destroy(surface->background_surface);
	if ( surface->hands_surface != NULL )
		wl_surface_destroy(surface->hands_surface);
	if ( surface->frame_callback != NULL )
		wl_callback_destroy(surface->frame_callback);
	finish_buffer(&surface->background_buffers[0]);
	finish_buffer(&surface->background_buffers[1]);
	finish_buffer(&surface->hands_buffers[0]);
	finish_buffer(&surface->hands_buffers[1]);
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
	configure_subsurface(surface);
	schedule_frame(surface, true, true);
	wl_surface_commit(surface->hands_surface);
	wl_surface_commit(surface->background_surface);
}

void update_all_surfaces (struct Wlclock *clock)
{
	clocklog(clock, 1, "[surface] Updating all surfaces.\n");
	struct Wlclock_output *op, *tmp;
	wl_list_for_each_safe(op, tmp, &clock->outputs, link)
		if ( op->surface != NULL )
			update_surface(op->surface);
}

void update_all_hands (struct Wlclock *clock)
{
	clocklog(clock, 1, "[surface] Updating all hands.\n");
	struct Wlclock_output *op, *tmp;
	wl_list_for_each_safe(op, tmp, &clock->outputs, link)
		if ( op->surface != NULL )
		{
			schedule_frame(op->surface, false, true);
			wl_surface_commit(op->surface->hands_surface);
			wl_surface_commit(op->surface->background_surface);
		}
}

