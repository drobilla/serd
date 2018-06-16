# Copyright 2020-2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

docdir = get_option('datadir') / 'doc'

config = configuration_data()
config.set('SERD_STATIC_PATH', serd_src_root / 'doc' / '_static')
config.set('SERD_TITLE', get_option('title'))
config.set('SERD_VERSION', meson.project_version())

conf_py = configure_file(
  configuration: config,
  input: files(serd_src_root / 'doc' / 'conf.py.in'),
  output: 'conf.py',
)

configure_file(
  copy: true,
  input: files('overview.cpp'),
  output: 'overview.cpp',
)

executable(
  'overview',
  files('overview.cpp'),
  cpp_args: cpp_suppressions,
  dependencies: [serd_dep, serdpp_dep],
)

cpp_rst_files = files(
  'cpp_facilities.rst',
  'index.rst',
  'model.rst',
  'nodes.rst',
  'overview.rst',
  'reading_and_writing.rst',
  'statements.rst',
  'stream_processing.rst',
  'using_serd.rst',
  'world.rst',
)

foreach f : cpp_rst_files
  configure_file(copy: true, input: f, output: '@PLAINNAME@')
endforeach

subdir('xml')
subdir('api')

docs = custom_target(
  'singlehtml',
  build_by_default: true,
  command: [
    sphinx_build,
    '-M', 'singlehtml',
    meson.current_build_dir(),
    meson.current_build_dir(),
    '-E',
    '-q',
    '-t', 'singlehtml',
  ],
  input: [cpp_rst_files, cpp_serd_rst, cpp_index_xml],
  install: true,
  install_dir: docdir / 'serd-0',
  output: 'singlehtml',
)

docs = custom_target(
  'html',
  build_by_default: true,
  command: [
    sphinx_build,
    '-M', 'html',
    meson.current_build_dir(),
    meson.current_build_dir(),
    '-E',
    '-q',
    '-t', 'html',
  ],
  input: [cpp_rst_files, cpp_serd_rst, cpp_index_xml],
  install: true,
  install_dir: docdir / 'serd-0',
  output: 'html',
)
