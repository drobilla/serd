# Copyright 2020-2022 David Robillard <d@drobilla.net>
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

#############
# Man Pages #
#############

mandoc = find_program('mandoc', required: false)
mandoc_css = files('mandoc.css')

if get_option('lint')
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
    command: [
      mandoc,
      '-Kutf-8',
      '-Ostyle=mandoc.css,man=%N.html',
      '-Thtml',
      '-Wwarning,stop',
      '@INPUT@',
    ],
    input: files('serdi.1'),
    output: 'serdi.html',
  )
endif

if not meson.is_subproject()
  summary('HTML man pages', build_docs, bool_yn: true)
  summary('Reference documentation', build_docs, bool_yn: true)
endif
