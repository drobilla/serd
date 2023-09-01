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
  # Warn if the "official" theme isn't present
  pymod = import('python')
  doc_modules = ['sphinx_lv2_theme']
  if not pymod.find_installation('python3', modules: doc_modules, required: false).found()
    warning('Missing sphinx_lv2_theme module, falling back to alabaster')
  endif

  # Configure conf.py for Sphinx
  conf_config = configuration_data()
  conf_config.set('SERD_SRCDIR', serd_src_root)
  conf_config.set('SERD_TITLE', get_option('title'))
  conf_config.set('SERD_VERSION', meson.project_version())
  conf_py = configure_file(
    configuration: conf_config,
    input: files('conf.py.in'),
    output: 'conf.py',
  )

  # Copy hand-written documentation files
  c_rst_files = files('index.rst', 'overview.rst')
  foreach f : c_rst_files
    configure_file(copy: true, input: f, output: '@PLAINNAME@')
  endforeach

  # Generate reference documentation input with Doxygen and Sphinxygen
  subdir('xml')
  subdir('api')

  # Build strict Sphinx flags, with termination on warnings if werror=true
  sphinx_flags = ['-E', '-a', '-q']
  if get_option('werror')
    sphinx_flags += ['-W']
  endif

  # Run Sphinx to generate final documentation for each format
  foreach format : ['html', 'singlehtml']
    if not get_option(format).disabled()
      subdir(format)
    endif
  endforeach
endif
