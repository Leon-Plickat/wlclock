# wlclock

wlclock is a digital analog clock for Wayland desktops.

wlclock is inspired by xclock and the default configuration has been chosen to
mimic it. However unlike xclock, wlclock is not a regular window but a
desktop-widget.

A Wayland compositor must implement the Layer-Shell and XDG-Output for wlclock
to work.

## Building

wlclock depends on Wayland, Wayland protocols and Cairo.

To build this program you will need a C compiler, the meson & ninja build system
and `scdoc` to generate the manpage.

    git clone https://git.sr.ht/~leon_plickat/wlclock
    cd wlclock
    meson build
    ninja -C build
    sudo ninja -C build install


## Contributing

**Contributions are welcome!** Read `CONTRIBUTING.md` to find out how you can
contribute. Do not be afraid, it is really not that complicated.

If you found this project on [GitHub](https://github.com/Leon-Plickat/wlclock),
you can use that platforms contribution workflow and bug tracking system, but
beware that I may be slow to respond to anything but email and that development
officially takes place over at [sourcehut](https://sr.ht/~leon_plickat/wlclock/),
the main hosting platform for this project.


## Licensing

wlclock is licensed under the GPLv3.

The contents of the `protocol` directory are licensed differently. Please see
the header of the files for more information.


## Authors

Leon Plickat <leonhenrik.plickat@stud.uni-goettingen.de>

