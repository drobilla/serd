# Copyright 2020-2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: CC0-1.0 OR ISC

docdir = get_option('datadir') / 'doc'

docs_required = get_option('docs').enabled()
doxygen = find_program('doxygen', required: docs_required)
dox_to_sphinx = find_program('../scripts/dox_to_sphinx.py')

build_docs = doxygen.found() and sphinx_build.found()

if build_docs
  subdir('_static')
  subdir('c')
endif

if not meson.is_subproject() and meson.version().version_compare('>=0.53.0')
  summary('Documentation', build_docs, bool_yn: true)
endif

subdir('man')
