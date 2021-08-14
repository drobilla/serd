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
    'cpp_std=c++17',
    'warning_level=3',
  ],
  license: 'ISC',
  meson_version: '>= 0.56.0',
  version: '1.1.1',
)

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
    template.format('unistd.h',
                    'return isatty(0);'),
    args: system_c_args,
    name: 'isatty')
    platform_c_args += ['-DHAVE_ISATTY']
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
  if cc.links(
    template.format('string.h',
                    'char buf[128]; return strerror_r(0, &buf, sizeof(buf));'),
    args: system_c_args,
    name: 'strerror_r')
    platform_c_args += ['-DHAVE_STRERROR_R']
  endif
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
  'include/serd/caret.h',
  'include/serd/env.h',
  'include/serd/event.h',
  'include/serd/input_stream.h',
  'include/serd/log.h',
  'include/serd/memory.h',
  'include/serd/node.h',
  'include/serd/node_syntax.h',
  'include/serd/output_stream.h',
  'include/serd/reader.h',
  'include/serd/serd.h',
  'include/serd/sink.h',
  'include/serd/statement.h',
  'include/serd/status.h',
  'include/serd/stream.h',
  'include/serd/string.h',
  'include/serd/string_view.h',
  'include/serd/syntax.h',
  'include/serd/uri.h',
  'include/serd/version.h',
  'include/serd/world.h',
  'include/serd/writer.h',
)

sources = files(
  'src/block_dumper.c',
  'src/buffer.c',
  'src/byte_source.c',
  'src/caret.c',
  'src/env.c',
  'src/input_stream.c',
  'src/log.c',
  'src/n3.c',
  'src/node.c',
  'src/node_syntax.c',
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
    '-DSERD_MAJOR_VERSION=@0@'.format(serd_major_version),
    '-DSERD_MICRO_VERSION=@0@'.format(serd_micro_version),
    '-DSERD_MINOR_VERSION=@0@'.format(serd_minor_version),
    '-DSERD_VERSION="@0@"'.format(meson.project_version()),
  ] + c_suppressions + extra_c_args + platform_c_args,
  dependencies: [exess_dep, m_dep, zix_dep],
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
    files(
      'src/console.c',
      'src/serdi.c',
    ),
    c_args: c_suppressions + platform_c_args,
    dependencies: [serd_dep, zix_dep],
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

ttl_metadata_files = files(
  'serd.ttl',
  'test/extra/abbreviate/manifest.ttl',
  'test/extra/bad/manifest.ttl',
  'test/extra/big/manifest.ttl',
  'test/extra/good/manifest.ttl',
  'test/extra/lax/manifest.ttl',
  'test/extra/perfect/manifest.ttl',
  'test/extra/prefix/manifest.ttl',
  'test/extra/pretty/manifest.ttl',
  'test/extra/qualify/manifest.ttl',
  'test/extra/root/manifest.ttl',
)

subdir('scripts')

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
