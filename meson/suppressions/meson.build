# Copyright 2020-2023 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

# Project-specific warning suppressions.
#
# This should be used in conjunction with the generic "warnings" sibling that
# enables all reasonable warnings for the compiler.  It lives here just to keep
# the top-level meson.build more readable.

#####
# C #
#####

warning_level = get_option('warning_level')

if is_variable('cc')
  c_suppressions = []

  if warning_level == 'everything'
    if cc.get_id() == 'clang'
      c_suppressions += [
        '-Wno-cast-align',
        '-Wno-cast-qual',
        '-Wno-declaration-after-statement',
        '-Wno-double-promotion',
        '-Wno-format-nonliteral',
        '-Wno-nullability-extension',
        '-Wno-nullable-to-nonnull-conversion',
        '-Wno-padded',
      ]

      if host_machine.system() == 'windows'
        c_suppressions += [
          '-Wno-deprecated-declarations',
          '-Wno-nonportable-system-include-path',
          '-Wno-unused-macros',
        ]
      endif

    elif cc.get_id() == 'gcc'
      c_suppressions += [
        '-Wno-cast-align',
        '-Wno-cast-qual',
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

    elif cc.get_id() == 'msvc'
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
  endif

  if cc.get_id() == 'clang'
    c_suppressions += [
      '-Wno-nullability-extension',
    ]
  elif cc.get_id() == 'msvc'
    c_suppressions += [
      '/experimental:external',
      '/external:W0',
      '/external:anglebrackets',
      '/wd4706', # assignment within conditional expression
      '/wd4996', # POSIX name for this item is deprecated
    ]
  endif

  if host_machine.system() == 'freebsd' and warning_level in ['3', 'everything']
    c_suppressions += [
      '-Wno-c11-extensions',
    ]
  endif

  c_suppressions = cc.get_supported_arguments(c_suppressions)
endif
