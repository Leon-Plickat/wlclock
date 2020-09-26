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

	/* Avoid too radii so big that they cause unexpected drawing behaviour. */
	if ( radius_top_left > center_size / 2 )
		radius_top_left = center_size / 2;
	if ( radius_top_right > center_size / 2 )
		radius_top_right = center_size / 2;
	if ( radius_bottom_left > center_size / 2 )
		radius_bottom_left = center_size / 2;
	if ( radius_bottom_right > center_size / 2 )
		radius_bottom_right = center_size / 2;

	clocklog(clock, 3, "[render] Render dimensions (scaled): size=%d cx=%d cy=%d w=%d h=%d\n",
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
	if ( clock->face_line_size == 0 )
		return;

	/* Radii are choosen to roughly mimic xclock. */
	double cx  = scale * (dimensions->center_x + (dimensions->center_size / 2));
	double cy  = scale * (dimensions->center_y + (dimensions->center_size / 2));
	double or  = scale * 0.9  * dimensions->center_size / 2;
	double ir  = scale * 0.85 * dimensions->center_size / 2;
	double bir = scale * 0.8  * dimensions->center_size / 2;
	double phi, phi_step = 2 * PI / 60;

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
	cairo_set_line_width(cairo, clock->face_line_size * scale);
	colour_set_cairo_source(cairo, &clock->clock_colour);
	cairo_stroke(cairo);
	cairo_restore(cairo);
}

static void draw_clock_hands (cairo_t *cairo, int32_t size, int32_t scale, struct Wlclock *clock)
{
	/* Radii are choosen to roughly mimic xclock. */
	double cxy = scale       * size / 2;
	double mr  = scale * 0.6 * size / 2;
	double hr  = scale * 0.4 * size / 2;
	double ir  = scale * 0.075 * size / 2;

	time_t now   =  time(NULL);
	struct tm tm = *localtime(&now);
	
	double phi_min_step = 2 * PI / 60;
	double phi_min      = phi_min_step * (tm.tm_min + 45);
	double phi_h_step   = 2 * PI / 12;
	double phi_h        = phi_h_step * (tm.tm_hour + 9);
	if (! clock->snap)
		phi_h += tm.tm_min * phi_h_step / 60.0;

	cairo_save(cairo);
	colour_set_cairo_source(cairo, &clock->clock_colour);

	if ( clock->hand_style == STYLE_XCLOCK )
	{
		/* Minutes */
		cairo_move_to(cairo, cxy + mr * cos(phi_min), cxy + mr * sin(phi_min));
		cairo_line_to(cairo, cxy + ir * cos(phi_min + 2.0 / 3.0 * PI),
				cxy + ir * sin(phi_min + 2.0 / 3.0 * PI));
		cairo_line_to(cairo, cxy + ir * cos(phi_min + 4.0 / 3.0 * PI),
				cxy + ir * sin(phi_min + 4.0 / 3.0 * PI));
		cairo_line_to(cairo, cxy + mr * cos(phi_min), cxy + mr * sin(phi_min));

		/* Hours */
		cairo_move_to(cairo, cxy + hr * cos(phi_h),    cxy + hr * sin(phi_h));
		cairo_line_to(cairo, cxy + ir * cos(phi_h + 2.0 / 3.0 * PI),
				cxy + ir * sin(phi_h + 2.0 / 3.0 * PI));
		cairo_line_to(cairo, cxy + ir * cos(phi_h + 4.0 / 3.0 * PI),
				cxy + ir * sin(phi_h + 4.0 / 3.0 * PI));
		cairo_line_to(cairo, cxy + hr * cos(phi_h), cxy + hr * sin(phi_h));

		cairo_close_path(cairo);
		cairo_fill(cairo);
	}
	else if ( clock->hand_style == STYLE_LINES )
	{
		/* Minutes */
		cairo_move_to(cairo, cxy + mr * cos(phi_min), cxy + mr * sin(phi_min));
		cairo_line_to(cairo, cxy + ir * cos(phi_min + PI), cxy + ir * sin(phi_min + PI));

		/* Hours */
		cairo_move_to(cairo, cxy + hr * cos(phi_h), cxy + hr * sin(phi_h));
		cairo_line_to(cairo, cxy + ir * cos(phi_h + PI), cxy + ir * sin(phi_h +PI));

		cairo_set_line_width(cairo, scale * clock->hand_line_size);
		cairo_stroke(cairo);
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

static void render_background_frame (struct Wlclock_surface *surface)
{
	struct Wlclock_output *output = surface->output;
	struct Wlclock        *clock  = output->clock;
	uint32_t               scale  = output->scale;

	clocklog(clock, 2, "[render] Render background frame: global_name=%d\n",
			output->global_name);

	cairo_t *cairo = surface->current_background_buffer->cairo;
	clear_buffer(cairo);

	draw_background(cairo, &surface->dimensions, scale, clock);
	draw_clock_face(cairo, &surface->dimensions, scale, clock);

	wl_surface_damage_buffer(surface->background_surface, 0, 0, INT32_MAX, INT32_MAX);
	wl_surface_attach(surface->background_surface, surface->current_background_buffer->buffer, 0, 0);

	surface->background_dirty = false;
}

static void render_hands_frame (struct Wlclock_surface *surface)
{
	struct Wlclock_output *output = surface->output;
	struct Wlclock        *clock  = output->clock;
	uint32_t               scale  = output->scale;

	clocklog(clock, 2, "[render] Render hands frame: global_name=%d\n",
			output->global_name);

	cairo_t *cairo = surface->current_hands_buffer->cairo;
	clear_buffer(cairo);

	draw_clock_hands(cairo, surface->dimensions.center_size, scale, clock);

	wl_surface_damage_buffer(surface->hands_surface, 0, 0, INT32_MAX, INT32_MAX);
	wl_surface_attach(surface->hands_surface, surface->current_hands_buffer->buffer, 0, 0);

	surface->hands_dirty = false;
}

void make_background_dirty (struct Wlclock_surface *surface)
{
	if (surface->background_dirty)
		return;

	struct Wlclock_output *output = surface->output;
	struct Wlclock        *clock  = output->clock;
	uint32_t               scale  = output->scale;

	clocklog(clock, 2, "[render] Making background dirty: global_name=%d\n",
			output->global_name);

	if (! next_buffer(&surface->current_background_buffer, clock->shm,
				surface->background_buffers,
				surface->dimensions.w * scale,
				surface->dimensions.h * scale))
		return;
	surface->current_background_buffer->busy = true;
	wl_surface_set_buffer_scale(surface->background_surface, scale);
	wl_surface_attach(surface->background_surface, surface->current_background_buffer->buffer, 0, 0);

	surface->background_dirty = true;
}

void make_hands_dirty (struct Wlclock_surface *surface)
{
	if (surface->hands_dirty)
		return;

	struct Wlclock_output *output = surface->output;
	struct Wlclock        *clock  = output->clock;
	uint32_t               scale  = output->scale;

	clocklog(clock, 2, "[render] Making hands dirty: global_name=%d\n",
			output->global_name);

	if (! next_buffer(&surface->current_hands_buffer, clock->shm,
				surface->hands_buffers,
				surface->dimensions.center_size * scale,
				surface->dimensions.center_size * scale))
		return;
	surface->current_hands_buffer->busy = true;
	wl_surface_set_buffer_scale(surface->hands_surface, scale);
	wl_surface_attach(surface->hands_surface, surface->current_hands_buffer->buffer, 0, 0);

	surface->hands_dirty = true;
}

static void frame_handle_done (void *data, struct wl_callback *callback, uint32_t time)
{
	struct Wlclock_surface *surface = (struct Wlclock_surface *)data;
	wl_callback_destroy(surface->frame_callback);
	surface->frame_callback = NULL;
	clocklog(surface->output->clock, 2, "[render] Frame callback: "
			"global-name=%d background=%d hands=%d.\n",
			surface->output->global_name,
			surface->background_dirty, surface->hands_dirty);
	if (surface->background_dirty)
		render_background_frame(surface);
	if (surface->hands_dirty)
		render_hands_frame(surface);
	wl_surface_commit(surface->hands_surface);
	wl_surface_commit(surface->background_surface);
}

static const struct wl_callback_listener frame_callback_listener = {
	.done = frame_handle_done
};

void schedule_frame (struct Wlclock_surface *surface, bool background, bool hands)
{
	if (! surface->configured)
		return;
	clocklog(surface->output->clock, 2, "[render] Scheduling frame: "
			"global-name=%d background=%d hands=%d\n",
			surface->output->global_name, background, hands);
	if (background)
		make_background_dirty(surface);
	if (hands)
		make_hands_dirty(surface);
	if ( surface->frame_callback != NULL )
		return;
	surface->frame_callback = wl_surface_frame(surface->background_surface);
	wl_callback_add_listener(surface->frame_callback, &frame_callback_listener, surface);
}

