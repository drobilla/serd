# Copyright 2020-2023 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

docdir = get_option('datadir') / 'doc'

#############
# Reference #
#############

doxygen = find_program('doxygen', required: get_option('docs'))
sphinx_build = find_program('sphinx-build', required: get_option('docs'))

if doxygen.found() and sphinx_build.found()
  sphinxygen = find_program('sphinxygen', required: false)
  if not sphinxygen.found()
    subproject('sphinxygen')
    sphinxygen = find_program('sphinxygen', required: get_option('docs'))
  endif
else
  sphinxygen = disabler()
endif

build_docs = doxygen.found() and sphinxygen.found() and sphinx_build.found()
if build_docs
  subdir('c')
endif

subdir('man')
