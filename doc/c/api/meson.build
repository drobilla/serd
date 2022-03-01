# Copyright 2020-2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: CC0-1.0 OR ISC

c_serd_rst = custom_target(
  'Serd C API ReST Documentation',
  command: [dox_to_sphinx, '-f', '@INPUT0@', meson.current_build_dir()],
  input: [c_index_xml] + c_rst_files,
  output: 'serd.rst')
