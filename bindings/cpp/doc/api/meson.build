# Copyright 2020-2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

dox_to_sphinx = find_program(serd_src_root / 'scripts' / 'dox_to_sphinx.py')

cpp_serd_rst = custom_target(
  'Serd C++ API ReST Documentation',
  command: [dox_to_sphinx,
            '-l', 'cpp',
            '-f', '@INPUT0@',
            meson.current_build_dir()],
  input: [cpp_index_xml] + cpp_rst_files,
  output: 'serd.rst')
