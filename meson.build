project(
  'wlclock',
  'c',
  version: '0.1.0',
  license: 'GPLv3',
  default_options: [
    'c_std=c11',
    'warning_level=3',
    'werror=true',
  ]
)

cc = meson.get_compiler('c')

add_project_arguments(cc.get_supported_arguments([
  '-D_POSIX_C_SOURCE=200809L',
  '-DVERSION="0.1.0"',
  '-fsigned-char',
  '-Wno-unused-parameter',
  '-Wpointer-arith',
  '-Wformat-security',
  '-Wunreachable-code',
  '-Wformat',
]), language: 'c')

if get_option('handle-signals').enabled()
  add_project_arguments(cc.get_supported_arguments([ '-DHANDLE_SIGNALS' ]), language: 'c')
endif

wayland_protocols = dependency('wayland-protocols')
wayland_client    = dependency('wayland-client', include_type: 'system')
wayland_cursor    = dependency('wayland-cursor', include_type: 'system')
cairo             = dependency('cairo')
realtime          = cc.find_library('rt')

if ['dragonfly', 'freebsd', 'netbsd', 'openbsd'].contains(host_machine.system())
  libepoll        = dependency('epoll-shim', required: get_option('handle-signals'))
else
  libepoll        = []
endif

subdir('protocol')

executable(
  'wlclock',
  files(
    'src/buffer.c',
    'src/misc.c',
    'src/output.c',
    'src/surface.c',
    'src/wlclock.c',
  ),
  dependencies: [
    wayland_client,
    wayland_protocols,
    wayland_cursor,
    cairo,
    wl_protocols,
    realtime,
    libepoll,
  ],
  include_directories: include_directories('src'),
  install: true,
)

scdoc = dependency(
  'scdoc',
  version: '>=1.9.2',
  native: true,
  required: get_option('man-pages'),
)
if scdoc.found()
  scdoc_prog = find_program(scdoc.get_pkgconfig_variable('scdoc'), native: true)
  sh = find_program('sh', native: true)
  mandir = get_option('mandir')
  man_files = [
    'doc/wlclock.1.scd'
  ]
  foreach filename : man_files
    topic = filename.split('.')[-3].split('/')[-1]
    section = filename.split('.')[-2]
    output = '@0@.@1@'.format(topic, section)

    custom_target(
      output,
      input: filename,
      output: output,
      command: [
        sh, '-c', '@0@ < @INPUT@ > @1@'.format(scdoc_prog.path(), output)
      ],
      install: true,
      install_dir: '@0@/man@1@'.format(mandir, section)
    )
  endforeach
endif
