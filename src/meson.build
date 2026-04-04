project(
  'vmana', 'c', 'objcpp',
  version : '1.0',
  default_options : ['warning_level=2', 'c_std=c11']
)

add_project_arguments('-march=native', language : 'c')

vimana_inc = include_directories('src', 'c/backend')

sdl3_dep = dependency('sdl3', required : false)
if not sdl3_dep.found()
  sdl3_dep = dependency('SDL3', required : true)
endif

vimana_src = files('src/vimana.c')
vimana_link_args = []

if target_machine.system() == 'darwin'
  vimana_src += files('c/backend/cocoa_autoreleasepool.mm')
  vimana_link_args += ['-framework', 'Foundation']
endif

libvimana = shared_library(
  'vimana',
  vimana_src,
  include_directories : vimana_inc,
  dependencies : [sdl3_dep],
  link_args : vimana_link_args,
  install : true,
)

install_headers('src/vimana.h', subdir : 'vimana')

libvimana_dep = declare_dependency(
  include_directories : vimana_inc,
  link_with : libvimana,
)

install_data(
  'vimana.yi',
  install_dir : get_option('datadir') / 'yis' / 'stdlib'
)

install_data(
  'yis/vimana_bindings.inc',
  install_dir : get_option('datadir') / 'yis' / 'vimana'
)

install_data(
  'yis/vimana_packager.sh',
  install_dir : get_option('datadir') / 'yis' / 'vimana'
)
