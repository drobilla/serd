# Copyright 2020-2024 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

# Project-specific warning suppressions

warning_level = get_option('warning_level')

#####
# C #
#####

if is_variable('cc')
  c_suppressions = []

  if cc.get_id() in ['clang', 'emscripten']
    if warning_level == 'everything'
      c_suppressions += [
        '-Wno-cast-align',
        '-Wno-cast-function-type-strict',
        '-Wno-declaration-after-statement',
        '-Wno-format-nonliteral',
        '-Wno-padded',
        '-Wno-unsafe-buffer-usage',
      ]

      if host_machine.system() == 'windows'
        c_suppressions += [
          '-Wno-deprecated-declarations',
          '-Wno-nonportable-system-include-path',
          '-Wno-unused-macros',
        ]
      endif
    endif

    if warning_level in ['everything', '3']
      c_suppressions += [
        '-Wno-nullability-extension',
      ]

      if host_machine.system() == 'freebsd'
        c_suppressions += [
          '-Wno-c11-extensions',
        ]
      endif
    endif

    if not meson.is_cross_build()
      c_suppressions += ['-Wno-poison-system-directories']
    endif

  elif cc.get_id() == 'gcc'
    if warning_level == 'everything'
      c_suppressions += [
        '-Wno-array-bounds',
        '-Wno-cast-align',
        '-Wno-format-nonliteral',
        '-Wno-inline',
        '-Wno-padded',
        '-Wno-switch-default',
        '-Wno-unsuffixed-float-constants',
        '-Wno-unused-const-variable',
      ]

      if host_machine.system() == 'windows'
        c_suppressions += [
          '-Wno-float-conversion',
        ]
      endif
    endif

    if warning_level in ['everything', '3']
      c_suppressions += [
        '-Wno-pedantic', # C11
      ]
    endif

  elif cc.get_id() == 'msvc'
    c_suppressions += [
      '/experimental:external',
      '/external:W0',
      '/external:anglebrackets',
    ]

    if warning_level == 'everything'
      c_suppressions += [
        '/wd4061', # enumerator in switch is not explicitly handled
        '/wd4514', # unreferenced inline function has been removed
        '/wd4710', # function not inlined
        '/wd4711', # function selected for automatic inline expansion
        '/wd4800', # implicit conversion from int to bool
        '/wd4820', # padding added after construct
        '/wd5045', # will insert Spectre mitigation for memory load
      ]
    endif

    if warning_level in ['everything', '3']
      c_suppressions += [
        '/wd4706', # assignment within conditional expression
      ]
    endif

    if warning_level in ['everything', '3', '2']
      c_suppressions += [
        '/wd4996', # POSIX name for this item is deprecated
      ]
    endif
  endif

  c_suppressions = cc.get_supported_arguments(c_suppressions)
endif
