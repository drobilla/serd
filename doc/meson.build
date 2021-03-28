# Copyright 2020-2023 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

docdir = get_option('datadir') / 'doc'

#############
# Man pages #
#############

subdir('man')

#############
# Reference #
#############

doc_config = configuration_data()
if is_variable('mandoc') and mandoc.found()
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

command_line_tools_rst = configure_file(
  configuration: doc_config,
  input: files('command_line_tools.rst.in'),
  output: 'command_line_tools.rst',
)

c_rst_files = files(
  'data_model.rst',
  'getting_started.rst',
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

# Copy hand-written documentation files
sphinx_input = []
foreach f : c_rst_files
  sphinx_input += [
    configure_file(copy: true, input: f, output: '@PLAINNAME@'),
  ]
endforeach

configure_file(
  copy: true,
  input: files('overview_code.c'),
  output: 'overview_code.c',
)

if not get_option('docs').disabled()
  executable(
    'overview_code',
    files('overview_code.c'),
    dependencies: [serd_dep],
    c_args: c_suppressions,
  )
endif

# Find required programs
doxygen = find_program('doxygen', required: get_option('docs'))
sphinx_build = find_program('sphinx-build', required: get_option('docs'))

# Find sphinxygen or fall back to subproject
sphinxygen = disabler()
if doxygen.found() and sphinx_build.found()
  sphinxygen = find_program('sphinxygen', required: false)
  if not sphinxygen.found()
    subproject('sphinxygen')
    sphinxygen = find_program('sphinxygen', required: get_option('docs'))
  endif
endif

# Build documentation if all required tools are found
build_docs = doxygen.found() and sphinxygen.found() and sphinx_build.found()
if build_docs
  subdir('_static')

  # Warn if the "official" theme isn't present
  pymod = import('python')
  doc_modules = ['sphinx_lv2_theme']
  py = pymod.find_installation('python3', modules: doc_modules, required: false)
  if not py.found()
    warning('Missing sphinx_lv2_theme module, falling back to alabaster')
  endif

  # Configure conf.py for Sphinx
  conf_config = configuration_data()
  conf_config.set('SERD_SRCDIR', serd_src_root)
  conf_config.set('SERD_STATIC_PATH', doc_static_build_dir)
  conf_config.set('SERD_TITLE', get_option('title'))
  conf_config.set('SERD_VERSION', meson.project_version())
  conf_py = configure_file(
    configuration: conf_config,
    input: files('conf.py.in'),
    output: 'conf.py',
  )

  subdir('xml')
  subdir('api')

  # Build strict Sphinx flags, with termination on warnings if werror=true
  sphinx_in_dir = meson.current_build_dir()
  sphinx_flags = ['-E', '-a', '-q']
  if get_option('werror')
    sphinx_flags += ['-W']
  endif

  # Run Sphinx to generate final documentation for each format
  sphinx_build_command = [sphinx_build] + sphinx_flags
  foreach format : ['epub', 'html', 'singlehtml']
    if not get_option(format).disabled()
      subdir(format)
    endif
  endforeach
endif

if not meson.is_subproject()
  summary(
    'Reference',
    build_docs,
    bool_yn: true,
    section: 'Components',
  )
endif
