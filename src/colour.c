#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<stdbool.h>
#include<string.h>
#include<cairo/cairo.h>

#include"misc.h"
#include"colour.h"

static bool colour_from_hex_string (struct Wlclock_colour *colour, const char *hex)
{
	unsigned int r = 0, g = 0, b = 0, a = 255;
	if ( 4 != sscanf(hex, "#%02x%02x%02x%02x", &r, &g, &b, &a)
			&& 3 != sscanf(hex, "#%02x%02x%02x", &r, &g, &b)
			&& 4 != sscanf(hex, "0x%02x%02x%02x%02x", &r, &g, &b, &a)
			&& 3 != sscanf(hex, "0x%02x%02x%02x", &r, &g, &b) )
		return false;

	colour->r = (float)r / 255.0f;
	colour->g = (float)g / 255.0f;
	colour->b = (float)b / 255.0f;
	colour->a = (float)a / 255.0f;

	return true;
}

static bool colour_from_rgb_string (struct Wlclock_colour *colour, const char *str)
{
	int32_t r = 0, g = 0, b = 0, a = 255;
	if ( 4 != sscanf(str, "rgba(%d,%d,%d,%d)", &r, &g, &b, &a)
			&& 3 != sscanf(str, "rgb(%d,%d,%d)", &r, &g, &b) )
		return false;

	if ( r > 255 || g > 255 || b > 255 || a > 255 )
		return false;
	if ( r < 0 || g < 0 || b < 0 || a < 0 )
		return false;

	colour->r = (float)r / 255.0f;
	colour->g = (float)g / 255.0f;
	colour->b = (float)b / 255.0f;
	colour->a = (float)a / 255.0f;

	return true;
}

bool colour_from_string (struct Wlclock_colour *colour, const char *str)
{
	if ( colour == NULL || str == NULL || *str == '\0' )
		goto error;

	if ( *str == '#' || strstr(str, "0x") == str )
	{
		if (! colour_from_hex_string(colour, str))
			goto error;
	}
	else if ( strstr(str, "rgb") == str )
	{
		if (! colour_from_rgb_string(colour, str))
			goto error;
	}
	else
		goto error;

	return true;

error:
	clocklog(NULL, 0, "ERROR: \"%s\" is not a valid colour.\n");
	return false;
}

void colour_set_cairo_source (cairo_t *cairo, struct Wlclock_colour *colour)
{
	cairo_set_source_rgba(cairo, colour->r, colour->g, colour->b, colour->a);
}

bool colour_is_transparent (struct Wlclock_colour *colour)
{
	return colour->a == 0.0;
}

