# Copyright 2020-2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: CC0-1.0 OR ISC

cpp_serd_rst = custom_target(
  'Serd C++ API ReST Documentation',
  command: [dox_to_sphinx,
            '-l', 'cpp',
            '-f', '@INPUT0@',
            meson.current_build_dir()],
  input: [cpp_index_xml] + cpp_rst_files,
  output: 'serd.rst')
