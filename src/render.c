#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
#include<unistd.h>
#include<string.h>
#include<math.h>

#include<cairo/cairo.h>
#include<wayland-server.h>
#include<wayland-client.h>
#include<wayland-client-protocol.h>

#include"wlclock.h"
#include"surface.h"
#include"output.h"
#include"misc.h"
#include"colour.h"
#include"render.h"

#define PI 3.141592653589793238462643383279502884

static void rounded_rectangle (cairo_t *cairo, uint32_t x, uint32_t y, uint32_t w, uint32_t h,
		double tl_r, double tr_r, double bl_r, double br_r)
{
	double degrees = PI / 180.0;
	cairo_new_sub_path(cairo);
	cairo_arc(cairo, x + w - tr_r, y     + tr_r, tr_r, -90 * degrees,   0 * degrees);
	cairo_arc(cairo, x + w - br_r, y + h - br_r, br_r,   0 * degrees,  90 * degrees);
	cairo_arc(cairo, x +     bl_r, y + h - bl_r, bl_r,  90 * degrees, 180 * degrees);
	cairo_arc(cairo, x +     tl_r, y     + tl_r, tl_r, 180 * degrees, 270 * degrees);
	cairo_close_path(cairo);
}

static void draw_background (cairo_t *cairo, struct Wlclock_dimensions *dimensions,
		int32_t scale, struct Wlclock *clock)
{
	if ( colour_is_transparent(&clock->background_colour)
			&& colour_is_transparent(&clock->border_colour) )
		return;

	int32_t w                   = scale * dimensions->w;
	int32_t h                   = scale * dimensions->h;
	int32_t center_x            = scale * dimensions->center_x;
	int32_t center_y            = scale * dimensions->center_y;
	int32_t center_size         = scale * dimensions->center_size;
	int32_t radius_top_left     = scale * clock->radius_top_left;
	int32_t radius_top_right    = scale * clock->radius_top_right;
	int32_t radius_bottom_left  = scale * clock->radius_bottom_left;
	int32_t radius_bottom_right = scale * clock->radius_bottom_right;

	clocklog(clock, 2, "[render] Render dimensions: size=%d cx=%d cy=%d w=%d h=%d\n",
			center_size, center_x, center_y, w, h);

	cairo_save(cairo);
	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);

	if ( radius_top_left == 0 && radius_top_right == 0
			&& radius_bottom_left == 0 && radius_bottom_right == 0 )
	{
		if ( center_x == 0 && center_y == 0 && center_size == w && center_size == h )
		{
			cairo_rectangle(cairo, 0, 0, w, h);
			colour_set_cairo_source(cairo, &clock->background_colour);
			cairo_fill(cairo);
		}
		else
		{
			cairo_rectangle(cairo, 0, 0, w, h);
			colour_set_cairo_source(cairo, &clock->border_colour);
			cairo_fill(cairo);

			cairo_rectangle(cairo, center_x, center_y, center_size, center_size);
			colour_set_cairo_source(cairo, &clock->background_colour);
			cairo_fill(cairo);
		}
	}
	else
	{
		if ( center_x == 0 && center_y == 0 && center_size == w && center_size == h )
		{
			rounded_rectangle(cairo, 0, 0, w, h,
					radius_top_left, radius_top_right,
					radius_bottom_left, radius_bottom_right);
			colour_set_cairo_source(cairo, &clock->background_colour);
			cairo_fill(cairo);
		}
		else
		{
			rounded_rectangle(cairo, 0, 0, w, h,
					radius_top_left, radius_top_right,
					radius_bottom_left, radius_bottom_right);
			colour_set_cairo_source(cairo, &clock->border_colour);
			cairo_fill(cairo);

			rounded_rectangle(cairo, center_x, center_y, center_size, center_size,
					radius_top_left, radius_top_right,
					radius_bottom_left, radius_bottom_right);
			colour_set_cairo_source(cairo, &clock->background_colour);
			cairo_fill(cairo);
		}
	}

	cairo_restore(cairo);
}

static void draw_clock_face (cairo_t *cairo, struct Wlclock_dimensions *dimensions,
		int32_t scale, struct Wlclock *clock)
{
	double cx  = dimensions->center_x + (dimensions->center_size / 2);
	double cy  = dimensions->center_y + (dimensions->center_size / 2);
	double or  = 0.9  * (double)(dimensions->center_size / 2);
	double ir  = 0.85 * (double)(dimensions->center_size / 2);
	double bir = 0.8  * (double)(dimensions->center_size / 2);
	double phi;
	double phi_step = 2 * PI / 60;

	cairo_save(cairo);
	for (int i = 0; i < 60; i++)
	{
		phi = i * phi_step;
		cairo_move_to(cairo, cx + or * cos(phi), cy + or * sin(phi));
		if ( i % 5 == 0 )
			cairo_line_to(cairo, cx + bir * cos(phi), cy + bir * sin(phi));
		else
			cairo_line_to(cairo, cx + ir * cos(phi), cy + ir * sin(phi));
	}
	cairo_set_line_width(cairo, 1);
	colour_set_cairo_source(cairo, &clock->clock_colour);
	cairo_stroke(cairo);
	cairo_restore(cairo);
}

static void clear_buffer (cairo_t *cairo)
{
	cairo_save(cairo);
	cairo_set_operator(cairo, CAIRO_OPERATOR_CLEAR);
	cairo_paint(cairo);
	cairo_restore(cairo);
}

void render_surface_frame (struct Wlclock_surface *surface)
{
	struct Wlclock_output *output = surface->output;
	struct Wlclock        *clock  = output->clock;
	uint32_t               scale  = output->scale;
	clocklog(clock, 2, "[render] Render frame: global_name=%d\n", output->global_name);

	/* Get new/next buffer. */
	if (! next_buffer(&surface->current_buffer, clock->shm, surface->buffers,
				surface->dimensions.w * scale,
				surface->dimensions.h * scale))
		return;

	cairo_t *cairo = surface->current_buffer->cairo;
	clear_buffer(cairo);

	draw_background(cairo, &surface->dimensions, scale, clock);
	draw_clock_face(cairo, &surface->dimensions, scale, clock);

	// TODO draw clock hands to subsurface

	wl_surface_set_buffer_scale(surface->surface, scale);
	wl_surface_attach(surface->surface, surface->current_buffer->buffer, 0, 0);
	wl_surface_damage_buffer(surface->surface, 0, 0, INT32_MAX, INT32_MAX);
}

