#include<errno.h>
#include<getopt.h>
#include<poll.h>
#include<stdbool.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/signalfd.h>
#include<signal.h>

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
	if (! strcmp(interface, wl_subcompositor_interface.name))
	{
		clocklog(clock, 2, "[main] Get wl_subcompositor.\n");
		clock->subcompositor = wl_registry_bind(registry, name,
				&wl_subcompositor_interface, 1);
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
	if ( clock->display == NULL )
		return;

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

	clocklog(clock, 2, "[main] Diconnecting from server.\n");
	wl_display_disconnect(clock->display);
}

static int count_args (int index, int argc, char *argv[])
{
	index--;
	int args = 0;
	while ( index < argc )
	{
		if ( *argv[index] == '-' )
			break;
		args++;
		index++;
	}
	return args;
}

static bool handle_command_flags (struct Wlclock *clock, int argc, char *argv[])
{
	enum
	{
		ANCHOR,
		BACKGROUND_COLOUR,
		BORDER_COLOUR,
		BORDER_SIZE,
		CLOCK_COLOUR,
		CLOCK_FACE_SIZE,
		EXCLUSIVE_ZONE,
		LAYER,
		MARGIN,
		NAMEPSACE,
		NO_INPUT,
		SNAP,
		OUTPUT,
		CORNER_RADIUS,
		SIZE
	};

	static struct option opts[] = {
		{"help",     no_argument,       NULL, 'h'},
		{"verbose",  no_argument,       NULL, 'v'},
		{"version",  no_argument,       NULL, 'V'},

		{"anchor",            required_argument, NULL, ANCHOR},
		{"background-colour", required_argument, NULL, BACKGROUND_COLOUR},
		{"border-colour",     required_argument, NULL, BORDER_COLOUR},
		{"border-size",       required_argument, NULL, BORDER_SIZE},
		{"clock-colour",      required_argument, NULL, CLOCK_COLOUR},
		{"clock-face-size",   required_argument, NULL, CLOCK_FACE_SIZE},
		{"exclusive-zone",    required_argument, NULL, EXCLUSIVE_ZONE},
		{"layer",             required_argument, NULL, LAYER},
		{"margin",            required_argument, NULL, MARGIN},
		{"namespace",         required_argument, NULL, NAMEPSACE},
		{"no-input",          no_argument,       NULL, NO_INPUT},
		{"snap",              no_argument,       NULL, SNAP},
		{"output",            required_argument, NULL, OUTPUT},
		{"corner-radius",     required_argument, NULL, CORNER_RADIUS},
		{"size",              required_argument, NULL, SIZE}
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
		"      --clock-face-size    Size of clock face lines.\n"
		"      --exclusive-zone     Exclusive zone of the layer surface.\n"
		"      --layer              Layer of the layer surface.\n"
		"      --margin             Directional margins.\n"
		"      --namespace          Namespace of the layer surface.\n"
		"      --no-input           Let inputs surface pass trough the layer surface.\n"
		"      --snap               Let the hour hand snap to the next position instead of slowly progressing.\n"
		"      --output             The output which the clock will be displayed.\n"
		"      --corner-radius      Corner radii.\n"
		"      --size               Size of the clock.\n"
		"\n";

	int opt, args;
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

		case ANCHOR:
			args = count_args(optind, argc, argv);
			if ( args != 4 )
			{
				clocklog(NULL, 0, "ERROR: Anchor configuration requires four arguments.\n");
				return false;
			}
			if (is_boolean_true(argv[optind-1]))
				clock->anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
			if (is_boolean_true(argv[optind]))
				clock->anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
			if (is_boolean_true(argv[optind+1]))
				clock->anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
			if (is_boolean_true(argv[optind+2]))
				clock->anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
			optind += 3; /* Tell getopt() to skip three argv fields. */
			break;

		case BACKGROUND_COLOUR:
			if (! colour_from_string(&clock->background_colour, optarg))
				return false;
			break;

		case BORDER_COLOUR:
			if (! colour_from_string(&clock->border_colour, optarg))
				return false;
			break;

		case BORDER_SIZE:
			args = count_args(optind, argc, argv);
			if ( args == 1 )
				clock->border_top = clock->border_right =
					clock->border_bottom = clock->border_left =
					atoi(optarg);
			else if ( args == 4 )
			{
				clock->border_top    = atoi(argv[optind-1]);
				clock->border_right  = atoi(argv[optind]);
				clock->border_bottom = atoi(argv[optind+1]);
				clock->border_left   = atoi(argv[optind+2]);
				optind += 3; /* Tell getopt() to skip three argv fields. */
			}
			else
			{
				clocklog(NULL, 0, "ERROR: Border configuration "
						"requires one or four arguments.\n");
				return false;
			}
			if ( clock->border_top < 0 || clock->border_right < 0
					|| clock->border_bottom < 0 || clock->border_left < 0 )
			{
				clocklog(NULL, 0, "ERROR: Borders may not be smaller than zero.\n");
				return false;
			}
			break;

		case CLOCK_COLOUR:
			if (! colour_from_string(&clock->clock_colour, optarg))
				return false;
			break;

		case CLOCK_FACE_SIZE:
			clock->clock_size = atoi(optarg);
			if ( clock->clock_size < 0 )
			{
				clocklog(NULL, 0, "ERROR: Size of clock face elements may not be smaller then 0.\n");
				return false;
			}
			break;

		case EXCLUSIVE_ZONE:
			if (is_boolean_true(optarg))
				clock->exclusive_zone = 1;
			else if (is_boolean_false(optarg))
				clock->exclusive_zone = 0;
			else if (! strcmp(optarg, "stationary"))
				clock->exclusive_zone = -1;
			else
			{
				clocklog(NULL, 0, "ERROR: Unrecognized exclusive zone option \"%s\".\n"
						"INFO: Possible options are 'true', "
						"'false' and 'stationary'.\n", optarg);
				return false;
			}
			break;

		case LAYER:
			if (! strcmp(optarg, "overlay"))
				clock->layer = ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY;
			else if (! strcmp(optarg, "top"))
				clock->layer = ZWLR_LAYER_SHELL_V1_LAYER_TOP;
			else if (! strcmp(optarg, "bottom"))
				clock->layer = ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM;
			else if (! strcmp(optarg, "background"))
				clock->layer = ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND;
			else
			{
				clocklog(NULL, 0, "ERROR: Unrecognized layer \"%s\".\n"
						"INFO: Possible layers are 'overlay', "
						"'top', 'bottom', and 'background'.\n", optarg);
				return false;
			}
			break;

		case MARGIN:
			args = count_args(optind, argc, argv);
			if ( args == 1 )
				clock->margin_top = clock->margin_right =
					clock->margin_bottom = clock->margin_left =
					atoi(optarg);
			else if ( args == 4 )
			{
				clock->margin_top    = atoi(argv[optind-1]);
				clock->margin_right  = atoi(argv[optind]);
				clock->margin_bottom = atoi(argv[optind+1]);
				clock->margin_left   = atoi(argv[optind+2]);
				optind += 3; /* Tell getopt() to skip three argv fields. */
			}
			else
			{
				clocklog(NULL, 0, "ERROR: Margin configuration "
						"requires one or four arguments.\n");
				return false;
			}
			if ( clock->margin_top < 0 || clock->margin_right < 0
					|| clock->margin_bottom < 0 || clock->margin_left < 0 )
			{
				clocklog(NULL, 0, "ERROR: Margins may not be smaller than zero.\n");
				return false;
			}
			break;

		case NAMEPSACE:
			set_string(&clock->namespace, optarg);
			break;

		case NO_INPUT:
			clock->input = false;
			break;

		case SNAP:
			clock->snap = true;
			break;

		case OUTPUT:
			if ( ! strcmp("all", optarg) || ! strcmp("*", optarg) )
				free_if_set(clock->output);
			else
				set_string(&clock->output, optarg);
			break;

		case CORNER_RADIUS:
			args = count_args(optind, argc, argv);
			if ( args == 1 )
				clock->radius_top_left = clock->radius_top_right =
					clock->radius_bottom_right = clock->radius_bottom_left =
					atoi(optarg);
			else if ( args == 4 )
			{
				clock->radius_top_left     = atoi(argv[optind-1]);
				clock->radius_top_right    = atoi(argv[optind]);
				clock->radius_bottom_right = atoi(argv[optind+1]);
				clock->radius_bottom_left  = atoi(argv[optind+2]);
				optind += 3; /* Tell getopt() to skip three argv fields. */
			}
			else
			{
				clocklog(NULL, 0, "ERROR: Radius configuration "
						"requires one or four arguments.\n");
				return false;
			}
			if ( clock->radius_top_left < 0 || clock->radius_top_right < 0
					|| clock->radius_bottom_right < 0 || clock->radius_bottom_left < 0 )
			{
				clocklog(NULL, 0, "ERROR: Radii may not be smaller than zero.\n");
				return false;
			}
			break;

		case SIZE:
			clock->dimensions.center_size = atoi(optarg);
			if ( clock->dimensions.center_size <= 10 )
			{
				clocklog(NULL, 0, "ERROR: Unreasonably small size \"%d\".\n",
						clock->dimensions.center_size);
				return false;
			}
			break;

		default:
			return false;
	}

	return true;
}

static time_t get_timeout (time_t now)
{
	/* Timeout until the next minute. */
	return ((now / 60 * 60 ) + 60 - now) * 1000;
}

static void clock_run (struct Wlclock *clock)
{
	clocklog(clock, 1, "[main] Starting loop.\n");
	clock->ret = EXIT_SUCCESS;

	struct pollfd fds[2] = { 0 };
	size_t wayland_fd = 0;
	size_t signal_fd = 1;

	fds[wayland_fd].events = POLLIN;
	if ( -1 ==  (fds[wayland_fd].fd = wl_display_get_fd(clock->display)) )
	{
		clocklog(NULL, 0, "ERROR: Unable to open Wayland display fd.\n");
		goto error;
	}

	sigset_t mask;
	struct signalfd_siginfo fdsi;
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGTERM);
	sigaddset(&mask, SIGQUIT);
	sigaddset(&mask, SIGUSR1);
	sigaddset(&mask, SIGUSR2);
	if ( sigprocmask(SIG_BLOCK, &mask, NULL) == -1 )
	{
		clocklog(NULL, 0, "ERROR: sigprocmask() failed.\n");
		goto error;
	}
	fds[signal_fd].events = POLLIN;
	if ( -1 ==  (fds[signal_fd].fd = signalfd(-1, &mask, 0)) )
	{
		clocklog(NULL, 0, "ERROR: Unable to open signal fd.\n"
				"ERROR: signalfd: %s\n", strerror(errno));
		goto error;
	}

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
		int ret = poll(fds, 2, get_timeout(clock->now));

		if ( ret == 0 )
		{
			clock->now = time(NULL);
			update_all_hands(clock);
			continue;
		}
		else if ( ret < 0 )
		{
			clocklog(NULL, 0, "ERROR: poll: %s\n", strerror(errno));
			continue;
		}
	
		/* Wayland events */
		if ( fds[wayland_fd].revents & POLLIN && wl_display_dispatch(clock->display) == -1 )
		{
			clocklog(NULL, 0, "ERROR: wl_display_flush: %s\n", strerror(errno));
			goto error;
		}
		if ( fds[wayland_fd].revents & POLLOUT && wl_display_flush(clock->display) == -1 )
		{
			clocklog(NULL, 0, "ERROR: wl_display_flush: %s\n", strerror(errno));
			goto error;
		}

		/* Signal events. */
		if ( fds[signal_fd].revents & POLLIN )
		{
			if ( read(fds[signal_fd].fd, &fdsi, sizeof(struct signalfd_siginfo))
					!= sizeof(struct signalfd_siginfo) )
			{
				clocklog(NULL, 0, "ERROR: Can not read signal info.\n");
				goto error;
			}

			if ( fdsi.ssi_signo == SIGINT || fdsi.ssi_signo == SIGQUIT || fdsi.ssi_signo == SIGTERM )
			{
				clocklog(clock, 1, "[main] Received SIGINT, SIGQUIT or SIGTERM; Exiting.\n");
				goto exit;
			}
			else if ( fdsi.ssi_signo == SIGUSR1 || fdsi.ssi_signo == SIGUSR2 )
				clocklog(clock, 1, "[main] Received SIGUSR; Ignoring.\n");
		}
	}

	return;
error:
	clock->ret = EXIT_FAILURE;
exit:
	if ( fds[wayland_fd].fd != -1 )
		close(fds[wayland_fd].fd);
	if ( fds[signal_fd].fd != -1 )
		close(fds[signal_fd].fd);
	return;
}

int main (int argc, char *argv[])
{
	struct Wlclock clock = { 0 };
	wl_list_init(&clock.outputs);
	clock.ret = EXIT_FAILURE;
	clock.loop = true;
	clock.verbosity = 0;

	clock.dimensions.center_size = 165; /* About the size of xclock, at least on my machine. */
	clock.exclusive_zone = -1;
	clock.input = true;
	clock.snap = false;
	clock.layer = ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY;
	clock.anchor = 0; /* Center */
	set_string(&clock.namespace, "wlclock");
	clock.border_bottom = clock.border_top
		= clock.border_left = clock.border_right = 1;
	clock.radius_bottom_left = clock.radius_bottom_right
		= clock.radius_top_left = clock.radius_top_right = 0;
	clock.margin_bottom = clock.margin_top
		= clock.margin_left = clock.margin_right = 0;
	clock.clock_size = 1;
	colour_from_string(&clock.background_colour, "#FFFFFF");
	colour_from_string(&clock.border_colour,     "#000000");
	colour_from_string(&clock.clock_colour,      "#000000");

	if (! handle_command_flags(&clock, argc, argv))
		goto exit;

	clock.dimensions.w = clock.dimensions.center_size
		+ clock.border_left + clock.border_right;
	clock.dimensions.h = clock.dimensions.center_size
		+ clock.border_top + clock.border_bottom;
	clock.dimensions.center_x = clock.border_left;
	clock.dimensions.center_y = clock.border_top;


	clocklog(&clock, 1, "[main] wlclock: version=%s\n"
			"[main] Default dimensions: size=%d cx=%d cy=%d w=%d h=%d\n",
			VERSION, clock.dimensions.center_size,
			clock.dimensions.center_x, clock.dimensions.center_y,
			clock.dimensions.w, clock.dimensions.h);

	if (! init_wayland(&clock))
		goto exit;

	clock_run(&clock);

exit:
	finish_wayland(&clock);
	free_if_set(clock.output);
	free_if_set(clock.namespace);
	return clock.ret;
}

