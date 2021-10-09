docdir = get_option('datadir') / 'doc'

docs_required = get_option('docs').enabled()
doxygen = find_program('doxygen', required: docs_required)
dox_to_sphinx = find_program('../scripts/dox_to_sphinx.py')
sphinx_build = find_program('sphinx-build', required: docs_required)

build_docs = doxygen.found() and sphinx_build.found()

if build_docs
  subdir('_static')
  subdir('c')
endif

if not meson.is_subproject() and meson.version().version_compare('>=0.53.0')
  summary('Documentation', build_docs, bool_yn: true)
endif

subdir('man')
