# Copyright 2021-2023 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

singlehtml_dir = docdir / versioned_name / 'singlehtml'

# TODO: Add install_tag: 'doc' after requiring meson 0.60.0

custom_target(
  'singlehtml',
  build_by_default: true,
  command: [
    sphinx_build, '-M', 'singlehtml', '@OUTDIR@' / '..', '@OUTDIR@' / '..',
    '-t', 'singlehtml',
  ] + sphinx_flags,
  input: [c_rst_files, c_serd_rst, c_index_xml, conf_py],
  install: true,
  install_dir: singlehtml_dir,
  output: [
    '_static',
    'index.html',
  ],
)

if not meson.is_subproject()
  summary('Unified HTML', get_option('prefix') / singlehtml_dir, section: 'Directories')
endif
