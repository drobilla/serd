# Copyright 2020-2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

header_c_suppressions = []

if get_option('strict')
  if cc.get_id() == 'clang'
    header_c_suppressions += [
      '-Wno-padded',
    ]
  elif cc.get_id() == 'gcc'
    header_c_suppressions += [
      '-Wno-padded',
      '-Wno-unused-const-variable',
    ]
  elif cc.get_id() == 'msvc'
    header_c_suppressions += [
      '/wd4820', # padding added after construct
    ]
  endif
endif

if cc.get_id() == 'clang'
  header_c_suppressions += [
    '-Wno-nullability-extension',
  ]
endif

test(
  'headers',
  executable(
    'test_headers',
    files('test_headers.c'),
    c_args: header_c_suppressions,
    dependencies: serd_dep,
  ),
  suite: 'unit',
)
