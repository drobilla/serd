# Copyright 2021-2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

project('serd', ['c'],
        version: '1.1.1',
        license: 'ISC',
        meson_version: '>= 0.56.0',
        default_options: [
          'b_ndebug=if-release',
          'buildtype=release',
          'warning_level=3',
          'c_std=c99',
          'cpp_std=c++17',
        ])

serd_major_version = meson.project_version().split('.')[0]
serd_minor_version = meson.project_version().split('.')[1]
serd_micro_version = meson.project_version().split('.')[2]
serd_src_root = meson.current_source_dir()
version_suffix = '-@0@'.format(serd_major_version)
versioned_name = 'serd' + version_suffix

#######################
# Compilers and Flags #
#######################

# Required tools
pkg = import('pkgconfig')
cc = meson.get_compiler('c')

# Set global warning flags
if get_option('strict') and not meson.is_subproject()
  subdir('meson/warnings')
endif
subdir('meson/suppressions')

################
# Dependencies #
################

m_dep = cc.find_library('m', required: false)

##########################
# Platform Configuration #
##########################

platform_args = []
if host_machine.system() in ['gnu', 'linux']
  platform_args += [
    '-D_POSIX_C_SOURCE=200809L',
  ]
endif

################
# Dependencies #
################

exess_dep = dependency('exess-0',
                       version: '>= 0.0.1',
                       fallback: ['exess', 'exess_dep'])

zix_dep = dependency('zix-0',
                     version: '>= 0.0.1',
                     fallback: ['zix', 'zix_dep'])

###########
# Library #
###########

include_dirs = include_directories('include')

c_headers = files(
  'include/serd/attributes.h',
  'include/serd/buffer.h',
  'include/serd/canon.h',
  'include/serd/caret.h',
  'include/serd/cursor.h',
  'include/serd/env.h',
  'include/serd/event.h',
  'include/serd/filter.h',
  'include/serd/input_stream.h',
  'include/serd/inserter.h',
  'include/serd/log.h',
  'include/serd/model.h',
  'include/serd/node.h',
  'include/serd/node_syntax.h',
  'include/serd/nodes.h',
  'include/serd/output_stream.h',
  'include/serd/range.h',
  'include/serd/reader.h',
  'include/serd/serd.h',
  'include/serd/sink.h',
  'include/serd/statement.h',
  'include/serd/status.h',
  'include/serd/stream.h',
  'include/serd/string.h',
  'include/serd/syntax.h',
  'include/serd/uri.h',
  'include/serd/value.h',
  'include/serd/version.h',
  'include/serd/world.h',
  'include/serd/writer.h',
)

sources = files(
  'src/block_dumper.c',
  'src/buffer.c',
  'src/byte_source.c',
  'src/canon.c',
  'src/caret.c',
  'src/compare.c',
  'src/cursor.c',
  'src/describe.c',
  'src/env.c',
  'src/filter.c',
  'src/input_stream.c',
  'src/inserter.c',
  'src/log.c',
  'src/model.c',
  'src/n3.c',
  'src/node.c',
  'src/node_syntax.c',
  'src/nodes.c',
  'src/output_stream.c',
  'src/read_nquads.c',
  'src/read_ntriples.c',
  'src/read_utf8.c',
  'src/reader.c',
  'src/sink.c',
  'src/statement.c',
  'src/statement.c',
  'src/string.c',
  'src/syntax.c',
  'src/system.c',
  'src/uri.c',
  'src/value.c',
  'src/world.c',
  'src/writer.c',
)

# Set appropriate arguments for building against the library type
extra_c_args = []
subdir('meson/library')
if get_option('default_library') == 'static'
  extra_c_args = ['-DSERD_STATIC']
endif

# Build shared and/or static library
libserd = library(
  meson.project_name() + library_suffix,
  sources,
  c_args: [
    '-DSERD_INTERNAL',
    '-DSERD_MAJOR_VERSION=@0@'.format(serd_major_version),
    '-DSERD_MICRO_VERSION=@0@'.format(serd_micro_version),
    '-DSERD_MINOR_VERSION=@0@'.format(serd_minor_version),
    '-DSERD_VERSION="@0@"'.format(meson.project_version()),
  ] + c_suppressions + extra_c_args + platform_args,
  dependencies: [exess_dep, m_dep, zix_dep],
  gnu_symbol_visibility: 'hidden',
  include_directories: include_directories(['include', 'src']),
  install: true,
  version: meson.project_version(),
)

# Declare dependency for internal meson dependants
serd_dep = declare_dependency(
  compile_args: extra_c_args,
  dependencies: [zix_dep],
  include_directories: include_dirs,
  link_with: libserd,
)

# Generage pkg-config file for external dependants
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

# Build serdi command line utility
if not get_option('tools').disabled()
  subdir('tools')
endif

###########
# Support #
###########

if not get_option('tests').disabled()
  subdir('test')
endif

if not get_option('docs').disabled()
  subdir('doc')
endif

if not meson.is_subproject()
  summary('Tests', not get_option('tests').disabled(), bool_yn: true)
  summary('Tools', not get_option('tools').disabled(), bool_yn: true)

  summary('Install prefix', get_option('prefix'))

  summary('Headers', get_option('prefix') / get_option('includedir'))
  summary('Libraries', get_option('prefix') / get_option('libdir'))

  if not get_option('tools').disabled()
    summary('Executables', get_option('prefix') / get_option('bindir'))
    summary('Man pages', get_option('prefix') / get_option('mandir'))
  endif
endif
