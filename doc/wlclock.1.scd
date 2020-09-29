wlclock(1) ["Version 1.0.0" ["Version 1.0.0"]]

# NAME
wlclock - A digital analog clock for Wayland


# DESCRIPTION
wlclock displays an analog clock on Wayland desktops.

wlclock is inspired by xclock(1) and the default configuration has been chosen
to mimic it. However unlike xclock, wlclock is not a regular window but a
desktop-widget.

A Wayland compositor must implement the Layer-Shell and XDG-Output for wlclock
to work.

# OPTIONS
*-h*, *--help*
	Display help message and exit.

*-V*, *--version*
	Display version and exit.

*-v*, *--verbose*
	Increase verbosity of logging.

*--background-colour* <colour>
	The background colour of the clock. The default is "#FFFFFF".

*--border-colour* <colour>
	The border colour of the clock. The default is "#000000".

*--border-size* <size>, *--border-size* <top> <right> <bottom> <left>
	Set the border size. Requires either one argument, to set all border sizes,
	or four arguments, to set the border sizes individually. The default border
	size is 1.

*--clock-colour* <colour>
	The colour of clock elements. The default is "#000000".

*--corner-radius* <radius>, *--corner-radius* <top-left> <top-right> <bottom-right> <bottom-left>
	The radius of the corners. Expects either one argument, to set the radius
	of all corners, or four arguments, to set the individual corner radii. If
	set to sufficiently high values, wlclock will be completely round. Set to 0
	to disable corner roundness. The default radius is 0.

*--exclusive-zone* <option>
	Exclusive zone of the surface. Can be "true", "false" or "stationary". If
	"true", the compositor is informed that it should not obstruct the surface
	of wlclock with other surfaces. If "false", the compositor is informed that
	it can place other surfaces over or under the surface of wlclock. If
	"stationary", the compositor is informed that the surface of wlclock should
	not be moved to accommodate other layer shell surfaces and that it can
	freely place other surfaces over or under the surface of wlclock. The
	default is "stationary". The exact implementation is compositor dependant;
	A compositor may choose to ignore the requested exclusive zone of surfaces.

*--hand-width* <size>
	The width of the clock hands. If set to 0, wlclock will draw xclock-like
	clock hands. The default is 0.

*--layer* <layer>
	Layer of the layer surface. Can be "overlay", "top", "bottom" or
	"background". Typically, "bottom" and "background" will be underneath
	regular windows, while "top" and "overlay" will be above them, however
	the exact placement on the z axis depends on the implementation of the
	compositor. The default layer is "overlay".

*--margin* <size>, *--margin* <top> <right> <bottom> <left>
	Set the margin. Requires either one argument, to set all margins, or four
	arguments, to set the margins individually. The default margin is 0.

*--marking-width* <size>
	The width of the markings on the clock face. The default is 1.

*--namespace* <namespace>
	The namespace of the layers surface. Some compositors may treat layer
	surfaces differently based on their namespace. The default is "wlclock".

*--no-input*
	If this flag is used, wlclock will ask the compositor to not send it
	input events; This will make mouse events pass through wlclock to windows
	beneath it.

*--output* <output name>
	Name of the output on which wlclock should be displayed. Output names
	are compositor dependant. If set to "all" or "\*", wlclock will be displayed
	on all outputs, which is the default behaviour.

*--position* <position>
	Set the position of the clock. Can be "center", "top", "right", "bottom",
	"left", "top-right", "top-left", "bottom-right" and "bottom-left". Defaults
	to "center".

*--size* <size>
	The size of the clock, without borders. The default size is 165.

*--snap*
	If this flag is used, the hour hand will instantly snap to the next position
	once the new hour begins, instead of slowly progressing over the hour.

# COLOURS
wlclock can parse hex code colours and read RGB values directly.

The supported formats are "#RRGGBB", "#RRGGBBAA", "0xRRGGBB" and "0xRRGGBBAA"
for hex code colours and "rgb(rrr,ggg,bbb)" and "rgba(rrr,ggg,bbb,aaa)" for RGB
colours.


# AUTHORS
Leon Henrik Plickat <leonhenrik.plickat@stud.uni-goettingen.de>

