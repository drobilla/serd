# Copyright 2021-2023 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

project(
  'serd',
  ['c'],
  default_options: [
    'b_ndebug=if-release',
    'buildtype=release',
    'c_std=c99',
    'c_winlibs=[]',
    'warning_level=3',
  ],
  license: 'ISC',
  meson_version: '>= 0.56.0',
  version: '0.31.1',
)

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
subdir('meson/suppressions')

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

  # Define HAVE_SOMETHING symbols for all detected features
  template = '#include <@0@>\nint main(void) { @1@; }'
  if cc.links(
    template.format('stdio.h', 'return fileno(stdin);'),
    args: system_c_args,
    name: 'fileno')
    platform_c_args += ['-DHAVE_FILENO']
  endif
  if cc.links(
    template.format('fcntl.h',
                    'posix_fadvise(0, 0, 4096, POSIX_FADV_SEQUENTIAL)'),
    args: system_c_args,
    name: 'posix_fadvise')
    platform_c_args += ['-DHAVE_POSIX_FADVISE']
  endif
  if cc.links(
    template.format('stdlib.h',
                    'void* mem=NULL; posix_memalign(&mem, 8U, 8U);'),
    args: system_c_args,
    name: 'posix_memalign')
    platform_c_args += ['-DHAVE_POSIX_MEMALIGN']
  endif
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
  c_args: [
    '-DSERD_INTERNAL',
    '-DSERD_VERSION="@0@"'.format(meson.project_version()),
  ] + c_suppressions + extra_c_args + platform_c_args,
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
    c_args: c_suppressions + platform_c_args,
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
