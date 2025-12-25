project('gslapper', ['c'])
cc = meson.get_compiler('c')

dl_dep = cc.find_library('dl', required : false)
wl_protocols=dependency('wayland-protocols')
wl_client=dependency('wayland-client')
wl_egl=dependency('wayland-egl')
egl=dependency('egl')
gst_dep=dependency('gstreamer-1.0')
gst_video_dep=dependency('gstreamer-video-1.0')
gst_gl_dep=dependency('gstreamer-gl-1.0')
threads=dependency('threads')

# Optional systemd support
systemd_dep = dependency('libsystemd', required: false)
if systemd_dep.found()
  add_project_arguments('-DHAVE_SYSTEMD', language: 'c')
  message('Systemd support enabled')
else
  message('Systemd support disabled (libsystemd not found)')
endif

scanner=find_program('wayland-scanner')
scanner_private_code=generator(scanner,output: '@BASENAME@-protocol.c',arguments: ['private-code','@INPUT@','@OUTPUT@'])
scanner_client_header=generator(scanner,output: '@BASENAME@-client-protocol.h',arguments: ['client-header','@INPUT@','@OUTPUT@'])

protocols_src=[
  scanner_private_code.process('proto/wlr-layer-shell-unstable-v1.xml'),
  scanner_private_code.process(wl_protocols.get_pkgconfig_variable('pkgdatadir')+'/stable/xdg-shell/xdg-shell.xml')
]

protocols_headers=[
  scanner_client_header.process('proto/wlr-layer-shell-unstable-v1.xml')
]

lib_protocols=static_library('protocols',protocols_src+protocols_headers,dependencies: wl_client)
protocols_dep=declare_dependency(link_with: lib_protocols,sources: protocols_headers)

executable(meson.project_name(), ['src/main.c', 'src/glad.c', 'src/cflogprinter.c', 'src/ipc.c', 'src/state.c'],
include_directories : ['inc'],
dependencies: [dl_dep, wl_client, wl_egl, egl, gst_dep, gst_video_dep, gst_gl_dep, threads, protocols_dep, systemd_dep], install: true)

shm_dep = cc.find_library('rt', required : false)
executable(meson.project_name() + '-holder', ['src/holder.c'],
include_directories : ['inc'],
dependencies: [dl_dep, wl_client, shm_dep, protocols_dep], install: true)