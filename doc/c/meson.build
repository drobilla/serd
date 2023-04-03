# Copyright 2020-2023 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

# Check for Sphinx LV2 theme

pymod = import('python')
sphinx_lv2_theme_python = pymod.find_installation(
  modules: ['sphinx_lv2_theme'],
  required: false
)

if not sphinx_lv2_theme_python.found()
  warning('Missing sphinx_lv2_theme module, falling back to alabaster')
endif

# Generate Sphinx configuration file

config = configuration_data()
config.set('SERD_VERSION', meson.project_version())
config.set('SERD_SRCDIR', serd_src_root)
config.set('SERD_TITLE', get_option('title'))

conf_py = configure_file(
  configuration: config,
  input: files('../conf.py.in'),
  output: 'conf.py',
)

# Copy hand-written documentation sources

configure_file(
  copy: true,
  input: files('../summary.rst'),
  output: 'summary.rst',
)

c_rst_files = files(
  'index.rst',
  'overview.rst',
)

foreach f : c_rst_files
  configure_file(copy: true, input: f, output: '@PLAINNAME@')
endforeach

# Generate reference documentation sources

subdir('xml')
subdir('api')

# Generate documentation with sphinx

# TODO: Add install_tag: 'doc' after requiring meson 0.60.0

custom_target(
  'singlehtml',
  build_by_default: true,
  command: [
    sphinx_build,
    '-M', 'singlehtml', '@OUTDIR@', '@OUTDIR@',
    '-E', '-q', '-t', 'singlehtml',
  ],
  input: [c_rst_files, c_serd_rst, c_index_xml, conf_py],
  install: true,
  install_dir: docdir / versioned_name,
  output: 'singlehtml',
)

custom_target(
  'html',
  build_by_default: true,
  command: [
    sphinx_build,
    '-M', 'html', '@OUTDIR@', '@OUTDIR@',
    '-E', '-q', '-t', 'html',
  ],
  input: [c_rst_files, c_serd_rst, c_index_xml, conf_py],
  install: true,
  install_dir: docdir / versioned_name,
  output: 'html',
)
