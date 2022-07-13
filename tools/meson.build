# Copyright 2021-2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

tool_c_args = c_suppressions + platform_c_args
tool_link_args = []

if get_option('static') and cc.get_id() != 'msvc'
  tool_link_args += ['-static']
endif

serdi = executable(
  'serdi',
  files(
    'console.c',
    'serdi.c',
  ),
  c_args: tool_c_args,
  dependencies: [serd_dep, zix_dep],
  install: true,
  link_args: tool_link_args,
)

meson.override_find_program('serdi', serdi)
