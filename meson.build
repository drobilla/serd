# Copyright 2021-2024 David Robillard <d@drobilla.net>
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

  # Feature checks have a header to include and code for the body of main()
  template = '#include <@0@>\nint main(void) { @1@; }'
  feature_tests = {
    'fileno': [
      'stdio.h',
      'return fileno(stdin);',
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
  fallback: ['exess', 'exess_dep'],
  include_type: 'system',
  version: '>= 0.0.1',
)

zix_dep = dependency(
  'zix-0',
  fallback: ['zix', 'zix_dep'],
  include_type: 'system',
  version: '>= 0.3.3',
)

###########
# Library #
###########

include_dirs = include_directories('include')

c_headers = files(
  'include/serd/attributes.h',
  'include/serd/buffer.h',
  'include/serd/caret_view.h',
  'include/serd/env.h',
  'include/serd/error.h',
  'include/serd/event.h',
  'include/serd/field.h',
  'include/serd/input_stream.h',
  'include/serd/node.h',
  'include/serd/output_stream.h',
  'include/serd/reader.h',
  'include/serd/serd.h',
  'include/serd/sink.h',
  'include/serd/status.h',
  'include/serd/stream.h',
  'include/serd/stream_result.h',
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
  'src/byte_source.c',
  'src/env.c',
  'src/input_stream.c',
  'src/node.c',
  'src/output_stream.c',
  'src/read_nquads.c',
  'src/read_ntriples.c',
  'src/read_trig.c',
  'src/read_turtle.c',
  'src/read_utf8.c',
  'src/reader.c',
  'src/sink.c',
  'src/string.c',
  'src/syntax.c',
  'src/uri.c',
  'src/value.c',
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
  darwin_versions: [serd_major_version + '.0.0', meson.project_version()],
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
