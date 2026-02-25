project('cogito', 'c', 'cpp', 'objcpp', version : '1.0')

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

cogito_c_args = []
if sdl3_image_dep.found()
  cogito_c_args += ['-DCOGITO_HAS_SDL3_IMAGE=1']
endif

cogito_deps = [
  sdl3_dep,
  sdl3_ttf_dep,
  dependency('freetype2', required : true),
]
if sdl3_image_dep.found()
  cogito_deps += sdl3_image_dep
endif

# Detect target platform
target_system = target_machine.system()

# Platform-specific source files and link args
cogito_src = files(
  'src/cogito.c',
  'src/ergo_compat.c',
  'c/backend/backend_sdl3.c',
  'c/backend/csd_sdl3.c',
)

cogito_link_args = []

# macOS-specific: Objective-C autorelease pool and Cocoa frameworks
if target_system == 'darwin'
  cogito_src += files('c/backend/cocoa_autoreleasepool.mm')
  cogito_link_args += ['-framework', 'Cocoa', '-framework', 'CoreGraphics', '-framework', 'Foundation', '-framework', 'AppKit']
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

# Install Cogito Ergo module into the Ergo stdlib
install_data(
  'cogito.ergo',
  install_dir : get_option('datadir') / 'ergo' / 'stdlib'
)

# Install Ergo-side C bindings shim used by `ergo run` codegen when `import cogito` is present.
install_data(
  'ergo/cogito_bindings.inc',
  install_dir : get_option('datadir') / 'ergo' / 'cogito'
)
