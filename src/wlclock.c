#include<errno.h>
#include<getopt.h>
#include<poll.h>
#include<stdbool.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>

#if HANDLE_SIGNALS
#include<sys/signalfd.h>
#include<signal.h>
#endif

#include<wayland-server.h>
#include<wayland-client.h>
#include<wayland-client-protocol.h>

#include"wlr-layer-shell-unstable-v1-protocol.h"
#include"xdg-output-unstable-v1-protocol.h"
#include"xdg-shell-protocol.h"

#include"wlclock.h"
#include"misc.h"
#include"output.h"
#include"surface.h"
#include"colour.h"

static void registry_handle_global (void *data, struct wl_registry *registry,
		uint32_t name, const char *interface, uint32_t version)
{
	struct Wlclock *clock = (struct Wlclock *)data;

	if (! strcmp(interface, wl_compositor_interface.name))
	{
		clocklog(clock, 2, "[main] Get wl_compositor.\n");
		clock->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
	}
	else if (! strcmp(interface, wl_shm_interface.name))
	{
		clocklog(clock, 2, "[main] Get wl_shm.\n");
		clock->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
	}
	else if (! strcmp(interface, zwlr_layer_shell_v1_interface.name))
	{
		clocklog(clock, 2, "[main] Get zwlr_layer_shell_v1.\n");
		clock->layer_shell = wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 1);
	}
	else if (! strcmp(interface, zxdg_output_manager_v1_interface.name))
	{
		clocklog(clock, 2, "[main] Get zxdg_output_manager_v1.\n");
		clock->xdg_output_manager = wl_registry_bind(registry, name, &zxdg_output_manager_v1_interface, 3);
	}
	else if (! strcmp(interface, wl_output_interface.name))
	{
		if (! create_output(data, registry, name, interface, version))
			goto error;
	}

	return;
error:
	clock->loop = false;
	clock->ret  = EXIT_FAILURE;
}

static void registry_handle_global_remove (void *data,
		struct wl_registry *registry, uint32_t name)
{
	struct Wlclock *clock = (struct Wlclock *)data;
	clocklog(clock, 1, "[main] Global remove.\n");
	destroy_output(get_output_from_global_name(clock, name));
}

static const struct wl_registry_listener registry_listener = {
	.global        = registry_handle_global,
	.global_remove = registry_handle_global_remove
};

/* Helper function for capability support error message. */
static bool capability_test (void *ptr, const char *name)
{
	if ( ptr != NULL )
		return true;
	clocklog(NULL, 0, "ERROR: Wayland compositor does not support %s.\n", name);
	return false;
}

static bool init_wayland (struct Wlclock *clock)
{
	clocklog(clock, 1, "[main] Init Wayland.\n");

	/* Connect to Wayland server. */
	clocklog(clock, 2, "[main] Connecting to server.\n");
	if ( NULL == (clock->display = wl_display_connect(NULL)) )
	{
		clocklog(NULL, 0, "ERROR: Can not connect to a Wayland server.\n");
		return false;
	}

	/* Get registry and add listeners. */
	clocklog(clock, 2, "[main] Get wl_registry.\n");
	if ( NULL == (clock->registry = wl_display_get_registry(clock->display)) )
	{
		clocklog(NULL, 0, "ERROR: Can not get registry.\n");
		return false;
	}
	wl_registry_add_listener(clock->registry, &registry_listener, clock);

	/* Allow registry listeners to catch up. */
	if ( wl_display_roundtrip(clock->display) == -1 )
	{
		clocklog(NULL, 0, "ERROR: Roundtrip failed.\n");
		return false;
	}

	/* Testing compatibilities. */
	if (! capability_test(clock->compositor, "wl_compositor"))
		return false;
	if (! capability_test(clock->shm, "wl_shm"))
		return false;
	if (! capability_test(clock->layer_shell, "zwlr_layer_shell"))
		return false;
	if (! capability_test(clock->xdg_output_manager, "xdg_output_manager"))
		return false;

	clocklog(clock, 2, "[main] Catching up on output configuration.\n");
	struct Wlclock_output *op;
	wl_list_for_each(op, &clock->outputs, link)
		if ( ! op->configured && ! configure_output(op) )
			return false;

	return true;
}

/* Finish him! */
static void finish_wayland (struct Wlclock *clock)
{
	clocklog(clock, 1, "[main] Finish Wayland.\n");

	destroy_all_outputs(clock);

	clocklog(clock, 2, "[main] Destroying Wayland objects.\n");
	if ( clock->layer_shell != NULL )
		zwlr_layer_shell_v1_destroy(clock->layer_shell);
	if ( clock->compositor != NULL )
		wl_compositor_destroy(clock->compositor);
	if ( clock->shm != NULL )
		wl_shm_destroy(clock->shm);
	if ( clock->registry != NULL )
		wl_registry_destroy(clock->registry);

	if ( clock->display != NULL )
	{
		clocklog(clock, 2, "[main] Diconnecting from server.\n");
		wl_display_disconnect(clock->display);
	}
}

static bool handle_command_flags (struct Wlclock *clock, int argc, char *argv[])
{
	static struct option opts[] = {
		{"help",     no_argument,       NULL, 'h'},
		{"verbose",  no_argument,       NULL, 'v'},
		{"version",  no_argument,       NULL, 'V'},

		{"anchor",            required_argument, NULL, 1100},
		{"background-colour", required_argument, NULL, 1101},
		{"border-colour",     required_argument, NULL, 1102},
		{"border-size",       required_argument, NULL, 1103},
		{"clock-colour",      required_argument, NULL, 1104},
		{"exclusive-zone",    required_argument, NULL, 1105},
		{"layer",             required_argument, NULL, 1106},
		{"margin",            required_argument, NULL, 1107},
		{"namespace",         required_argument, NULL, 1108},
		{"no-input",          no_argument,       NULL, 1109},
		{"output",            required_argument, NULL, 1110},
		{"corner-radius",     required_argument, NULL, 1111},
		{"size",              required_argument, NULL, 1112},
	};

	const char *usage =
		"Usage: wlclock [options]\n"
		"\n"
		"  -h, --help               Show this help text.\n"
		"  -v, --verbose            Increase verbosity of output.\n"
		"  -V, --version            Show the version.\n"
		"      --anchor             Set the layer shell anchors.\n"
		"      --background-colour  Background colour.\n"
		"      --border-colour      Border colour.\n"
		"      --border-size        Size of the border.\n"
		"      --clock-colour       Colour of the clock elements.\n"
		"      --exclusive-zone     Exclusive zone of the layer surface.\n"
		"      --layer              Layer of the layer surface.\n"
		"      --margin             Directional margins.\n"
		"      --namespace          Namespace of the layer surface.\n"
		"      --no-input           Make inputs surface pass trough the layer surface.\n"
		"      --output             The output which the clock will be displayed.\n"
		"      --corner-radius      Corner radii.\n"
		"      --size               Size of the clock.\n"
		"\n";

	int opt;
	extern int optind;
	extern char *optarg;
	while ( (opt = getopt_long(argc, argv, "hvV", opts, &optind)) != -1 ) switch (opt)
	{
		case 'h':
			fputs(usage, stderr);
			clock->ret = EXIT_SUCCESS;
			return false;

		case 'v':
			clock->verbosity++;
			break;

		case 'V':
			fputs("wlclock version " VERSION "\n", stderr);
			clock->ret = EXIT_SUCCESS;
			return false;

		case 1100: // TODO anchor
			break;

		case 1101: /* Background colour */
			colour_from_string(&clock->background_colour, optarg);
			break;

		case 1102: /* Border colour */
			colour_from_string(&clock->border_colour, optarg);
			break;

		case 1103: // TODO border size
			break;

		case 1104: /* Clock colour */
			colour_from_string(&clock->clock_colour, optarg);
			break;

		case 1105: // TODO exclusive zone
			break;

		case 1106: // TODO layer
			break;

		case 1107: // TODO margins
			break;

		case 1108: /* Namespace */
			set_string(&clock->namespace, optarg);
			break;

		case 1109: /* Input */
			clock->input = false;
			break;

		case 1110: /* Output */
			set_string(&clock->output, optarg);
			break;

		case 1111: // TODO corner radii
			break;

		case 1112: // TODO size
			break;

		default:
			return false;
	}

	return true;
}

static time_t get_timeout (time_t now)
{
	return ((now / 60 * 60 ) + 60 - now) * 1000;
}

static void clock_run (struct Wlclock *clock)
{
	clocklog(clock, 1, "[main] Starting loop.\n");

	clock->ret = EXIT_SUCCESS;

	struct pollfd fds[] = {
		{ .fd = wl_display_get_fd(clock->display), .events = POLLIN }
	};

	while (clock->loop)
	{
		/* Flush Wayland events. */
		errno = 0;
		do {
			if ( wl_display_flush(clock->display) == 1 && errno != EAGAIN )
			{
				clocklog(NULL, 0, "ERROR: wl_display_flush: %s\n",
						strerror(errno));
				break;
			}
		} while ( errno == EAGAIN );

		clock->now = time(NULL);
		errno = 0;
		int ret = poll(fds, 1, get_timeout(clock->now));

		if ( ret == 0 )
			update_all_surfaces(clock);
		else if ( ret > 0 )
		{
			if ( fds[0].revents & POLLIN && wl_display_dispatch(clock->display) == -1 )
			{
				clocklog(NULL, 0, "ERROR: wl_display_flush: %s\n", strerror(errno));
				goto error;
			}
			if ( fds[0].revents & POLLOUT && wl_display_flush(clock->display) == -1 )
			{
				clocklog(NULL, 0, "ERROR: wl_display_flush: %s\n", strerror(errno));
				goto error;
			}
		}
		else
			clocklog(NULL, 0, "ERROR: poll: %s\n", strerror(errno));
	}

	return;
error:
	clock->ret = EXIT_FAILURE;
	return;
}

int main (int argc, char *argv[])
{
	struct Wlclock clock = { 0 };
	clock.ret = EXIT_FAILURE;
	clock.loop = true;
	clock.verbosity = 0;
	clock.size = 100;
	clock.exclusive_zone = 1;
	clock.input = true;
	clock.layer = ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM;
	clock.anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
	set_string(&clock.namespace, "wlclock");
	wl_list_init(&clock.outputs);
	clock.border_bottom = clock.border_top
		= clock.border_left = clock.border_right = 1;
	clock.radius_bottom_left = clock.radius_bottom_right
		= clock.radius_top_left = clock.radius_top_right = 5;
	clock.margin_bottom = clock.margin_top
		= clock.margin_left = clock.margin_right = 0;
	colour_from_string(&clock.background_colour, "#000000");
	colour_from_string(&clock.border_colour,     "#FFFFFF");
	colour_from_string(&clock.clock_colour,      "#FFFFFF");

	if (! handle_command_flags(&clock, argc, argv))
		return clock.ret;

	clocklog(&clock, 1, "[main] wlclock: version=%s\n", VERSION);

	if (! init_wayland(&clock))
		goto exit;

	clock_run(&clock);

exit:
	finish_wayland(&clock);
	return clock.ret;
}

