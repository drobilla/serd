# Copyright 2021-2026 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

project(
  'serd',
  ['c'],
  default_options: [
    'b_ndebug=if-release',
    'buildtype=release',
    'c_std=c99',
    'c_winlibs=[]',
    'cpp_std=c++17',
  ],
  license: 'ISC',
  meson_version: '>= 0.56.0',
  version: '1.1.1',
)

serd_major_version = meson.project_version().split('.')[0]
serd_minor_version = meson.project_version().split('.')[1]
serd_micro_version = meson.project_version().split('.')[2]
serd_src_root = meson.current_source_dir()
serd_build_root = meson.current_build_dir()
version_suffix = '-@0@'.format(serd_major_version)
versioned_name = 'serd' + version_suffix

#######################
# Compilers and Flags #
#######################

# Required tools
pkg = import('pkgconfig')
cc = meson.get_compiler('c')

# Set global warning suppressions
warning_level = get_option('warning_level')
c_suppressions = []
if cc.get_id() in ['clang', 'emscripten']
  if warning_level == 'everything'
    c_suppressions += [
      '-Wno-cast-qual',
      '-Wno-declaration-after-statement',
      '-Wno-nrvo',
      '-Wno-padded',
      '-Wno-switch-default',
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
    c_suppressions += ['-Wno-nullability-extension']

    if host_machine.system() == 'freebsd'
      c_suppressions += ['-Wno-c11-extensions']
    endif
  endif

  if not meson.is_cross_build()
    c_suppressions += ['-Wno-poison-system-directories']
  endif

elif cc.get_id() == 'gcc'
  if warning_level == 'everything'
    c_suppressions += [
      '-Wno-cast-qual',
      '-Wno-inline',
      '-Wno-padded',
      '-Wno-switch-default',
      '-Wno-unsuffixed-float-constants',
      '-Wno-unused-const-variable',
    ]

    if host_machine.system() == 'windows'
      c_suppressions += ['-Wno-float-conversion']
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

################
# Dependencies #
################

m_dep = cc.find_library('m', required: false)

##########################
# Platform Configuration #
##########################

# Use versioned name everywhere to support parallel major version installations
if host_machine.system() == 'windows'
  if get_option('default_library') == 'both'
    error('default_library=both is not supported on Windows')
  endif
  soversion = ''
else
  soversion = meson.project_version().split('.')[0]
endif

# Request POSIX APIs from standard headers if necessary
system_c_args = []
if host_machine.system() in ['cygwin', 'gnu', 'linux']
  system_c_args += ['-D_POSIX_C_SOURCE=200809L']
endif

# Build platform-specific configuration arguments
platform_c_args = []
if get_option('checks').disabled()
  # Generic build without platform-specific features
  platform_c_args += ['-DSERD_NO_DEFAULT_CONFIG']
elif get_option('checks').auto()
  # Statically detect configuration from the build environment
  platform_c_args += system_c_args
else
  # Only use the features detected by the build system
  platform_c_args += ['-DSERD_NO_DEFAULT_CONFIG'] + system_c_args

  # Feature checks have a header to include and code for the body of main()
  template = '#include <@0@>\nint main(void) { @1@; }'
  feature_tests = {
    'fileno': [
      'stdio.h',
      'return fileno(stdin);',
    ],
    'isatty': [
      'unistd.h',
      'return isatty(2);',
    ],
    'posix_fadvise': [
      'fcntl.h',
      'posix_fadvise(0, 0, 4096, POSIX_FADV_SEQUENTIAL);',
    ],
  }

  # Define HAVE_SOMETHING symbols for all detected features
  foreach name, args : feature_tests
    code = template.format(args[0], args[1])
    if cc.links(code, args: system_c_args, name: name)
      platform_c_args += ['-DHAVE_' + name.to_upper()]
    endif
  endforeach
endif

################
# Dependencies #
################

exess_dep = dependency(
  'exess-0',
  default_options: [
    'docs=disabled',
    'lint=false',
    'tests=disabled',
  ],
  fallback: ['exess', 'exess_dep'],
  include_type: 'system',
  version: '>= 0.0.1',
)

zix_dep = dependency(
  'zix-0',
  default_options: [
    'benchmarks=disabled',
    'docs=disabled',
    'lint=false',
    'tests=disabled',
    'tests_cpp=disabled',
  ],
  fallback: ['zix', 'zix_dep'],
  include_type: 'system',
  version: '>= 0.6.2',
)

###########
# Library #
###########

include_dirs = include_directories('include')

c_headers = files(
  'include/serd/attributes.h',
  'include/serd/buffer.h',
  'include/serd/canon.h',
  'include/serd/caret_view.h',
  'include/serd/env.h',
  'include/serd/event.h',
  'include/serd/field.h',
  'include/serd/file_uri.h',
  'include/serd/filter.h',
  'include/serd/handler.h',
  'include/serd/input_stream.h',
  'include/serd/log.h',
  'include/serd/node.h',
  'include/serd/node_flags.h',
  'include/serd/node_type.h',
  'include/serd/object_view.h',
  'include/serd/output_stream.h',
  'include/serd/reader.h',
  'include/serd/serd.h',
  'include/serd/sink.h',
  'include/serd/statement_view.h',
  'include/serd/status.h',
  'include/serd/stream.h',
  'include/serd/stream_result.h',
  'include/serd/string.h',
  'include/serd/string_pair_view.h',
  'include/serd/struct_literal.h',
  'include/serd/syntax.h',
  'include/serd/tee.h',
  'include/serd/token_view.h',
  'include/serd/uri.h',
  'include/serd/version.h',
  'include/serd/world.h',
  'include/serd/writer.h',
)

sources = files(
  'src/block_dumper.c',
  'src/byte_source.c',
  'src/canon.c',
  'src/env.c',
  'src/event.c',
  'src/field.c',
  'src/file_uri.c',
  'src/filter.c',
  'src/handler.c',
  'src/input_stream.c',
  'src/log.c',
  'src/node.c',
  'src/object_view.c',
  'src/output_stream.c',
  'src/read_nquads.c',
  'src/read_ntriples.c',
  'src/read_trig.c',
  'src/read_turtle.c',
  'src/read_utf8.c',
  'src/reader.c',
  'src/sink.c',
  'src/stream_stdio.c',
  'src/string.c',
  'src/string_pair_view.c',
  'src/symbols.c',
  'src/syntax.c',
  'src/tee.c',
  'src/token_view.c',
  'src/uri.c',
  'src/world.c',
  'src/writer.c',
)

# Set appropriate arguments for building against the library type
extra_c_args = []
if get_option('default_library') == 'static'
  extra_c_args = ['-DSERD_STATIC']
endif

# Build shared and/or static library
libserd = library(
  versioned_name,
  sources,
  c_args: [
    '-DSERD_INTERNAL',
  ] + c_suppressions + extra_c_args + platform_c_args,
  darwin_versions: [serd_major_version + '.0.0', meson.project_version()],
  dependencies: [exess_dep, m_dep, zix_dep],
  gnu_symbol_visibility: 'hidden',
  implicit_include_directories: false,
  include_directories: include_dirs,
  install: true,
  soversion: soversion,
  version: meson.project_version(),
)

# Declare dependency for internal meson dependants
serd_dep = declare_dependency(
  compile_args: extra_c_args,
  dependencies: [zix_dep],
  include_directories: include_dirs,
  link_with: libserd,
)

# Generate pkg-config file for external dependants
pkg.generate(
  libserd,
  description: 'Lightweight C library for working with RDF data',
  extra_cflags: extra_c_args,
  filebase: versioned_name,
  name: get_option('title'),
  requires: ['zix-0'],
  subdirs: [versioned_name],
  version: meson.project_version(),
)

# Override pkg-config dependency for internal meson dependants
meson.override_dependency(versioned_name, serd_dep)

# Install header to a versioned include directory
install_headers(c_headers, subdir: versioned_name / 'serd')

#########
# Tools #
#########

# Build serd-pipe command line utility
if not get_option('tools').disabled()
  subdir('tools')
endif

# Display top-level summary (before subdirectories to appear first)
if not meson.is_subproject()
  summary(
    {
      'Tests': not get_option('tests').disabled(),
      'Tools': not get_option('tools').disabled(),
    },
    bool_yn: true,
    section: 'Components',
  )

  summary(
    {
      'Install prefix': get_option('prefix'),
      'Headers': get_option('prefix') / get_option('includedir'),
      'Libraries': get_option('prefix') / get_option('libdir'),
    },
    section: 'Directories',
  )

  if not get_option('tools').disabled()
    summary(
      {
        'Executables': get_option('prefix') / get_option('bindir'),
      },
      section: 'Directories',
    )
  endif
endif

###########
# Support #
###########

subdir('scripts')

if not get_option('tests').disabled()
  subdir('test')
endif

subdir('doc')
