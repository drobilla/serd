# Copyright 2021-2023 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

tool_c_args = c_suppressions + platform_c_args
tool_link_args = []

if get_option('static') and cc.get_id() != 'msvc'
  tool_link_args += ['-static']
endif

serd_filter = executable(
  'serd-filter',
  files('console.c', 'serd-filter.c'),
  c_args: tool_c_args,
  dependencies: [serd_dep, zix_dep],
  install: true,
  link_args: tool_link_args,
)

serd_pipe = executable(
  'serd-pipe',
  files('console.c', 'serd-pipe.c'),
  c_args: tool_c_args,
  dependencies: [serd_dep, zix_dep],
  install: true,
  link_args: tool_link_args,
)

serd_sort = executable(
  'serd-sort',
  files('console.c', 'serd-sort.c'),
  c_args: tool_c_args,
  dependencies: [serd_dep, zix_dep],
  install: true,
  link_args: tool_link_args,
)

meson.override_find_program('serd-filter', serd_filter)
meson.override_find_program('serd-pipe', serd_pipe)
meson.override_find_program('serd-sort', serd_sort)
