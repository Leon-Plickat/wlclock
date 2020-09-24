#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
#include<unistd.h>
#include<string.h>
#include<assert.h>

#include<wayland-server.h>
#include<wayland-client.h>
#include<wayland-client-protocol.h>

#include"xdg-output-unstable-v1-protocol.h"
#include"xdg-shell-protocol.h"

#include"wlclock.h"
#include"misc.h"
#include"output.h"
#include"surface.h"

/* No-Op function. */
static void noop () {}

static void output_handle_scale (void *data, struct wl_output *wl_output,
		int32_t factor)
{
	struct Wlclock_output *output = (struct Wlclock_output *)data;
	output->scale                 = (uint32_t)factor;
	clocklog(output->clock, 1, "[output] Property update: global_name=%d scale=%d\n",
				output->global_name, output->scale);
}

static void output_update_surface (struct Wlclock_output *output)
{
	if ( ! output->configured || output->name == NULL )
		return;

	struct Wlclock *clock = output->clock;
	if ( clock->output == NULL || ! strcmp(clock->output, output->name) )
		create_surface(output);
}

static void output_handle_done (void *data, struct wl_output *wl_output)
{
	/* This event is sent after all output property changes (by wl_output
	 * and by xdg_output) have been advertised by preceding events.
	 */
	struct Wlclock_output *output = (struct Wlclock_output *)data;
	clocklog(output->clock, 1, "[output] Atomic update complete: global_name=%d\n",
				output->global_name);
	if ( output->surface != NULL )
		update_surface(output->surface);
	else
		output_update_surface(output);
}

static const struct wl_output_listener output_listener = {
	.scale    = output_handle_scale,
	.geometry = noop,
	.mode     = noop,
	.done     = output_handle_done
};

static void xdg_output_handle_name (void *data, struct zxdg_output_v1 *xdg_output,
		const char *name)
{
	struct Wlclock_output *output = (struct Wlclock_output *)data;
	set_string(&output->name, (char *)name);
	clocklog(output->clock, 1, "[output] Property update: global_name=%d name=%s\n",
				output->global_name, name);
}

static const struct zxdg_output_v1_listener xdg_output_listener = {
	.name             = xdg_output_handle_name,
	.logical_size     = noop,
	.logical_position = noop,
	.description      = noop,

	/* Deprecated since version 3, xdg_output property changes now send wl_output.done */
	.done             = noop
};

bool configure_output (struct Wlclock_output *output)
{
	struct Wlclock *clock = output->clock;
	clocklog(clock, 1, "[output] Configuring: global_name=%d\n", output->global_name);

	/* Create xdg_output and attach listeners. */
	if ( NULL == (output->xdg_output = zxdg_output_manager_v1_get_xdg_output(
					clock->xdg_output_manager, output->wl_output)) )
	{
		clocklog(NULL, 0, "ERROR: Could not get XDG output.\n");
		return false;
	}

	zxdg_output_v1_add_listener(output->xdg_output, &xdg_output_listener, output);
	output->configured = true;
	return true;
}

bool create_output (struct Wlclock *clock, struct wl_registry *registry,
		uint32_t name, const char *interface, uint32_t version)
{
	clocklog(clock, 1, "[output] Creating: global_name=%d\n", name);

	struct wl_output *wl_output = wl_registry_bind(registry, name,
			&wl_output_interface, 3);
	assert(wl_output);

	struct Wlclock_output *output = calloc(1, sizeof(struct Wlclock_output));
	if ( output == NULL )
	{
		clocklog(NULL, 0, "ERROR: Could not allocate.\n");
		return false;
	}

	output->clock       = clock;
	output->global_name = name;
	output->scale       = 1;
	output->wl_output   = wl_output;
	output->configured  = false;
	output->name        = NULL;

	wl_list_insert(&clock->outputs, &output->link);
	wl_output_set_user_data(wl_output, output);
	wl_output_add_listener(wl_output, &output_listener, output);

	/* We can only use the output if we have both xdg_output_manager and
	 * the layer_shell. If either one is not available yet, we have to
	 * configure the output later (see init_wayland()).
	 */
	if ( clock->xdg_output_manager != NULL && clock->layer_shell != NULL )
	{
		if (! configure_output(output))
			return false;
	}
	else
		clocklog(clock, 2, "[output] Not yet configureable.\n");

	return true;
}

struct Wlclock_output *get_output_from_global_name (struct Wlclock *clock, uint32_t name)
{
	struct Wlclock_output *op;
	wl_list_for_each(op, &clock->outputs, link)
		if ( op->global_name == name )
			return op;
	return NULL;
}

void destroy_output (struct Wlclock_output *output)
{
	if ( output == NULL )
		return;
	 if ( output->surface != NULL )
		destroy_surface(output->surface);
	wl_list_remove(&output->link);
	wl_output_destroy(output->wl_output);
	free(output);
}

void destroy_all_outputs (struct Wlclock *clock)
{
	clocklog(clock, 1, "[output] Destroying all outputs.\n");
	struct Wlclock_output *op, *tmp;
	wl_list_for_each_safe(op, tmp, &clock->outputs, link)
		destroy_output(op);
}

