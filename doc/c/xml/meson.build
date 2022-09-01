# Copyright 2020-2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: CC0-1.0 OR ISC

doxygen = find_program('doxygen')

config = configuration_data()
config.set('SERD_SRCDIR', serd_src_root)
config.set('DOX_OUTPUT', meson.current_build_dir() / '..')

c_doxyfile = configure_file(
  configuration: config,
  input: files('../Doxyfile.in'),
  output: 'Doxyfile',
)

c_index_xml = custom_target(
  'index.xml',
  command: [doxygen, '@INPUT0@'],
  input: [c_doxyfile] + c_headers,
  output: 'index.xml',
)
