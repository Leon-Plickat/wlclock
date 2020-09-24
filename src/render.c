#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
#include<unistd.h>
#include<string.h>
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

static void rounded_rectangle (cairo_t *cairo, uint32_t x, uint32_t y, uint32_t w, uint32_t h,
		double tl_r, double tr_r, double bl_r, double br_r)
{
	double degrees = 3.1415927 / 180.0;
	cairo_new_sub_path(cairo);
	cairo_arc(cairo, x + w - tr_r, y     + tr_r, tr_r, -90 * degrees,   0 * degrees);
	cairo_arc(cairo, x + w - br_r, y + h - br_r, br_r,   0 * degrees,  90 * degrees);
	cairo_arc(cairo, x +     bl_r, y + h - bl_r, bl_r,  90 * degrees, 180 * degrees);
	cairo_arc(cairo, x +     tl_r, y     + tl_r, tl_r, 180 * degrees, 270 * degrees);
	cairo_close_path(cairo);
}

static void draw_background (cairo_t *cairo,
		uint32_t x, uint32_t y, uint32_t w, uint32_t h,
		uint32_t border_top, uint32_t border_right,
		uint32_t border_bottom, uint32_t border_left,
		uint32_t top_left_radius, uint32_t top_right_radius,
		uint32_t bottom_left_radius, uint32_t bottom_right_radius,
		uint32_t scale,
		struct Wlclock_colour *background_colour,
		struct Wlclock_colour *border_colour)
{
	if ( colour_is_transparent(background_colour) && colour_is_transparent(border_colour) )
		return;

	/* Scale. */
	x *= scale, y *= scale, w *= scale, h *= scale;
	border_top *= scale, border_bottom *= scale;
	border_left *= scale, border_right *= scale;
	top_left_radius *= scale, top_right_radius *= scale;
	bottom_left_radius *= scale, bottom_right_radius *= scale;

	cairo_save(cairo);
	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);

	if ( top_left_radius == 0 && top_right_radius == 0
			&& bottom_left_radius == 0 && bottom_right_radius == 0 )
	{
		if ( border_top == 0 && border_bottom == 0
				&& border_left == 0 && border_right == 0 )
		{
			cairo_rectangle(cairo, x, y, w, h);
			colour_set_cairo_source(cairo, background_colour);
			cairo_fill(cairo);
		}
		else
		{
			fputs("here\n", stderr);
			/* Calculate dimensions of center. */
			uint32_t cx = x + border_left,
				cy = y + border_top,
				cw = w - (border_left + border_right),
				ch = h - (border_top + border_bottom);

			/* Borders. */
			cairo_rectangle(cairo, x, y, w, border_top);
			cairo_rectangle(cairo, x + w - border_right, y + border_top,
					border_right, h - border_top - border_bottom);
			cairo_rectangle(cairo, x, y + h - border_bottom, w, border_bottom);
			cairo_rectangle(cairo, x, y + border_top, border_left,
					h - border_top - border_bottom);
			colour_set_cairo_source(cairo, border_colour);
			cairo_fill(cairo);

			/* Center. */
			cairo_rectangle(cairo, cx, cy, cw, ch);
			colour_set_cairo_source(cairo, background_colour);
			cairo_fill(cairo);
		}
	}
	else
	{
		if ( border_top == 0 && border_bottom == 0
				&& border_left == 0 && border_right == 0 )
		{
			rounded_rectangle(cairo, x, y, w, h,
					top_left_radius, top_right_radius,
					bottom_left_radius, bottom_right_radius);
			colour_set_cairo_source(cairo, background_colour);
			cairo_fill(cairo);
		}
		else
		{
			rounded_rectangle(cairo, x, y, w, h,
					top_left_radius, top_right_radius,
					bottom_left_radius, bottom_right_radius);
			colour_set_cairo_source(cairo, border_colour);
			cairo_fill(cairo);

			rounded_rectangle(cairo, x + border_left, y + border_top,
					w - (border_left + border_right),
					h - (border_bottom + border_top),
					top_left_radius, top_right_radius,
					bottom_left_radius, bottom_right_radius);
			colour_set_cairo_source(cairo, background_colour);
			cairo_fill(cairo);
		}
	}
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
				surface->size * scale, surface->size * scale))
		return;

	cairo_t *cairo = surface->current_buffer->cairo;
	clear_buffer(cairo);

	draw_background(cairo, 0, 0, surface->size, surface->size,
			clock->border_top, clock->border_right,
			clock->border_bottom, clock->border_left,
			clock->radius_top_left, clock->radius_top_right,
			clock->radius_bottom_left, clock->radius_bottom_right,
			scale, &clock->background_colour, &clock->border_colour);

	// TODO draw clock stuff

	wl_surface_set_buffer_scale(surface->surface, scale);
	wl_surface_attach(surface->surface, surface->current_buffer->buffer, 0, 0);
	wl_surface_damage_buffer(surface->surface, 0, 0, INT32_MAX, INT32_MAX);
}

