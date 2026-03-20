project(
  'cogito', 'c', 'cpp', 'objcpp',
  version : '1.0',
  default_options : ['warning_level=2', 'c_std=c11', 'cpp_std=c++17']
)

add_project_arguments('-march=native', language : 'c')

cogito_inc = include_directories('src', 'c/backend')

# SDL3 dependency (pkg-config name first, fallback to CMake name)
sdl3_dep = dependency('sdl3', required : false)
if not sdl3_dep.found()
  sdl3_dep = dependency('SDL3', required : true)
endif

# SDL3_ttf for text rendering
sdl3_ttf_dep = dependency('sdl3-ttf', required : false)
if not sdl3_ttf_dep.found()
  sdl3_ttf_dep = dependency('SDL3_ttf', required : false)
endif

# SDL3_image for themed icon file loading
sdl3_image_dep = dependency('sdl3-image', required : false)
if not sdl3_image_dep.found()
  sdl3_image_dep = dependency('SDL3_image', required : false)
endif

# Optional GStreamer playback backend used by Extras/Victrola gst.yi
gst_dep = dependency('gstreamer-1.0', required : false)

# FriBidi for Unicode Bidirectional Algorithm (RTL text support)
fribidi_dep = dependency('fribidi', required : false)

# librsvg + Cairo for full-fidelity SVG rendering on Linux
rsvg_dep = dependency('librsvg-2.0', version : '>=2.46', required : false)
cairo_dep = dependency('cairo', required : false)

cogito_c_args = []
if sdl3_image_dep.found()
  cogito_c_args += ['-DCOGITO_HAS_SDL3_IMAGE=1']
endif
if gst_dep.found()
  cogito_c_args += ['-DCOGITO_HAS_GSTREAMER=1']
endif
if fribidi_dep.found()
  cogito_c_args += ['-DCOGITO_HAS_FRIBIDI=1']
endif
if rsvg_dep.found() and cairo_dep.found()
  cogito_c_args += ['-DCOGITO_HAS_RSVG=1']
endif

cogito_deps = [
  sdl3_dep,
  sdl3_ttf_dep,
  dependency('freetype2', required : true),
]
if sdl3_image_dep.found()
  cogito_deps += sdl3_image_dep
endif
if gst_dep.found()
  cogito_deps += gst_dep
endif
if fribidi_dep.found()
  cogito_deps += fribidi_dep
endif
if rsvg_dep.found() and cairo_dep.found()
  cogito_deps += [rsvg_dep, cairo_dep]
endif

# Detect target platform
target_system = target_machine.system()

# Platform-specific source files and link args
cogito_src = files(
  'src/cogito.c',
  'src/yis_compat.c',
  'c/backend/backend_sdl3.c',
  'c/backend/csd_sdl3.c',
)

cogito_link_args = []

# macOS-specific: Objective-C autorelease pool and Cocoa frameworks
if target_system == 'darwin'
  cogito_src += files('c/backend/cocoa_autoreleasepool.mm', 'c/backend/font_dialog_macos.mm', 'c/backend/svg_native_macos.mm', 'c/backend/mpris_macos.mm')
  cogito_link_args += ['-framework', 'Cocoa', '-framework', 'CoreGraphics', '-framework', 'Foundation', '-framework', 'AppKit', '-framework', 'CoreText', '-framework', 'MediaPlayer']
endif

# Linux-specific: librsvg SVG renderer
if target_system == 'linux' and rsvg_dep.found() and cairo_dep.found()
  cogito_src += files('c/backend/svg_native_linux.c')
endif

# Build shared library
libcogito = shared_library(
  'cogito',
  cogito_src,
  include_directories : cogito_inc,
  dependencies : cogito_deps,
  c_args : cogito_c_args,
  link_args : cogito_link_args,
  install : true,
)

# Install header files
install_headers('src/cogito.h', subdir : 'cogito')

libcogito_dep = declare_dependency(
  include_directories : cogito_inc,
  link_with : libcogito,
)

# Install Cogito Yis module into the Yis stdlib
install_data(
  'cogito.yi',
  install_dir : get_option('datadir') / 'yis' / 'stdlib'
)

# Install Yis-side C bindings shim used by `yis run` codegen when `import cogito` is present.
install_data(
  'yis/cogito_bindings.inc',
  install_dir : get_option('datadir') / 'yis' / 'cogito'
)

# Install Cogito app packager hook used by Yis after compiling Cogito apps.
install_data(
  'yis/cogito_packager.sh',
  install_dir : get_option('datadir') / 'yis' / 'cogito'
)
