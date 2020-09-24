#ifndef WLCLOCK_COLOUR_H
#define WLCLOCK_COLOUR_H

#include<stdbool.h>
#include<cairo/cairo.h>

struct Wlclock_colour
{
	double r, g, b, a;
};

bool colour_from_string (struct Wlclock_colour *colour, const char *hex);
void colour_set_cairo_source (cairo_t *cairo, struct Wlclock_colour *colour);
bool colour_is_transparent (struct Wlclock_colour *colour);

#endif

