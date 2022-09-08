# Copyright 2020-2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

docdir = get_option('datadir') / 'doc'

doxygen = find_program('doxygen', required: get_option('docs'))
dox_to_sphinx = files('../scripts/dox_to_sphinx.py')
sphinx_build = find_program('sphinx-build', required: get_option('docs'))
mandoc = find_program('mandoc', required: false)

build_docs = doxygen.found() and sphinx_build.found()

if build_docs
  subdir('c')
endif

mandoc_css = files('mandoc.css')

if not get_option('tests').disabled()
  stylelint = find_program('stylelint', required: false)
  if stylelint.found()
    test('stylelint', stylelint, args: [mandoc_css], suite: 'data')
  endif
endif

if mandoc.found()
  configure_file(input: mandoc_css, output: '@PLAINNAME@', copy: true)

  serdi_html = custom_target(
    'serdi.html',
    build_by_default: true,
    capture: true,
    command: [mandoc, '-Thtml', '-Werror', '-O', 'style=mandoc.css', '@INPUT@'],
    input: files('serdi.1'),
    output: 'serdi.html',
  )
endif

if not meson.is_subproject()
  summary('API Documentation', build_docs, bool_yn: true)
endif
