# Copyright 2021-2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

project('serd', ['c'],
        version: '0.31.0',
        license: 'ISC',
        meson_version: '>= 0.56.0',
        default_options: [
          'b_ndebug=if-release',
          'buildtype=release',
          'warning_level=3',
          'c_std=c99',
          'c_winlibs=[]',
        ])

serd_src_root = meson.current_source_dir()
major_version = meson.project_version().split('.')[0]
version_suffix = '-@0@'.format(major_version)
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

# Use versioned name everywhere to support parallel major version installations
if host_machine.system() == 'windows'
  if get_option('default_library') == 'both'
    error('default_library=both is not supported on Windows')
  endif
  soversion = ''
else
  soversion = meson.project_version().split('.')[0]
endif

###########
# Library #
###########

include_dirs = include_directories('include')

c_headers = files(
  'include/serd/serd.h',
)

sources = files(
  'src/base64.c',
  'src/byte_source.c',
  'src/env.c',
  'src/n3.c',
  'src/node.c',
  'src/reader.c',
  'src/string.c',
  'src/system.c',
  'src/uri.c',
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
  c_args: c_suppressions + extra_c_args + platform_args + [
    '-DSERD_INTERNAL',
    '-DSERD_VERSION="@0@"'.format(meson.project_version()),
  ],
  dependencies: m_dep,
  gnu_symbol_visibility: 'hidden',
  include_directories: include_dirs,
  install: true,
  soversion: soversion,
  version: meson.project_version(),
)

# Declare dependency for internal meson dependants
serd_dep = declare_dependency(
  compile_args: extra_c_args,
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
  tool_link_args = []
  if get_option('static')
    tool_link_args += ['-static']
  endif

  serdi = executable(
    'serdi',
    files('src/serdi.c'),
    c_args: c_suppressions + platform_args,
    dependencies: serd_dep,
    install: true,
    link_args: tool_link_args,
  )

  meson.override_find_program('serdi', serdi)

  if not get_option('docs').disabled()
    install_man(files('doc/serdi.1'))
  endif
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
