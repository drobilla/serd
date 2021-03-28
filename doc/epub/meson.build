# Copyright 2021-2023 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

epub_dir = docdir / versioned_name / 'epub'

# TODO: Add install_tag: 'doc' after requiring meson 0.60.0

custom_target(
  'epub',
  build_by_default: true,
  command: [
    sphinx_build,
    '-M', 'epub',
    '@OUTDIR@' / '..',
    '@OUTDIR@' / '..',
    '-t', 'epub',
  ] + sphinx_flags,
  input: [c_rst_files, c_serd_rst, c_index_xml, conf_py],
  install: true,
  install_dir: epub_dir,
  output: ['Serd-1.1.1.epub'],
)

if not meson.is_subproject()
  summary('EPub', get_option('prefix') / epub_dir, section: 'Directories')
endif
