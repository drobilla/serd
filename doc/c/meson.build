# Copyright 2020-2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

########################
# Sphinx Configuration #
########################

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

conf_config = configuration_data()
conf_config.set('SERD_SRCDIR', serd_src_root)
conf_config.set('SERD_STATIC_PATH', serd_src_root / 'doc' / '_static')
conf_config.set('SERD_TITLE', get_option('title'))
conf_config.set('SERD_VERSION', meson.project_version())

conf_py = configure_file(
  configuration: conf_config,
  input: files('../conf.py.in'),
  output: 'conf.py',
)

##############
# ReST Pages #
##############

doc_config = configuration_data()
if mandoc.found()
  doc_config.set('SERD_COMMAND_LINE_INDEX_ENTRY', '\n   command_line_tools\n')
  doc_config.set('SERD_FILTER_LINK', '`serd-filter <../man/serd-filter.html>`_')
  doc_config.set('SERD_PIPE_LINK', '`serd-pipe <../man/serd-pipe.html>`_')
  doc_config.set('SERD_SORT_LINK', '`serd-sort <../man/serd-sort.html>`_')
else
  doc_config.set('SERD_COMMAND_LINE_INDEX_ENTRY', '')
  doc_config.set('SERD_FILTER_LINK', '``serd-filter``')
  doc_config.set('SERD_PIPE_LINK', '``serd-pipe``')
  doc_config.set('SERD_SORT_LINK', '``serd-sort``')
endif

configure_file(
  configuration: doc_config,
  input: files('../command_line_tools.rst.in'),
  output: 'command_line_tools.rst',
)

c_rst_files = files(
  '../data_model.rst',
  '../getting_started.rst',
  '../summary.rst',
  'index.rst',
  'model.rst',
  'nodes.rst',
  'overview.rst',
  'reading_and_writing.rst',
  'statements.rst',
  'stream_processing.rst',
  'string_views.rst',
  'using_serd.rst',
  'world.rst',
)

foreach f : c_rst_files
  configure_file(copy: true, input: f, output: '@PLAINNAME@')
endforeach

##############################
# Documentation/Example Code #
##############################

configure_file(
  copy: true,
  input: files('overview_code.c'),
  output: 'overview_code.c',
)

executable(
  'overview_code',
  files('overview_code.c'),
  dependencies: [serd_dep],
  c_args: c_suppressions,
)

###########################
# Generated API Reference #
###########################

subdir('xml')
subdir('api')

# Generate documentation with sphinx

# TODO: Add install_tag: 'doc' after requiring meson 0.60.0

if not get_option('singlehtml').disabled()
  custom_target(
    'singlehtml',
    build_by_default: true,
    command: [sphinx_build, '-M', 'singlehtml', '@OUTDIR@', '@OUTDIR@',
              '-W', '-E', '-a', '-q', '-t', 'singlehtml'],
    input: [c_rst_files, c_serd_rst, c_index_xml, conf_py],
    install: true,
    install_dir: docdir / versioned_name,
    output: 'singlehtml',
  )
endif

if not get_option('html').disabled()
  custom_target(
    'html',
    build_by_default: true,
    command: [sphinx_build, '-M', 'html', '@OUTDIR@', '@OUTDIR@',
              '-W', '-E', '-a', '-q', '-t', 'html'],
    input: [c_rst_files, c_serd_rst, c_index_xml, conf_py],
    install: true,
    install_dir: docdir / versioned_name,
    output: 'html',
  )
endif

if not get_option('epub').disabled()
  custom_target(
    'epub C documentation for serd',
    build_by_default: true,
    command: [sphinx_build, '-M', 'epub', '@OUTDIR@', '@OUTDIR@',
              '-W', '-E', '-a', '-q', '-t', 'epub'],
    input: [c_rst_files, c_serd_rst, c_index_xml, conf_py],
    install: true,
    install_dir: docdir / versioned_name,
    output: 'epub',
  )
endif
