project('serd', ['c'],
        version: '1.0.1',
        license: 'ISC',
        meson_version: '>= 0.49.2',
        default_options: [
          'b_ndebug=if-release',
          'buildtype=release',
          'c_std=c99',
          'cpp_std=c++14',
          'default_library=shared',
          'warning_level=1',
        ])

serd_major_version = meson.project_version().split('.')[0]
serd_minor_version = meson.project_version().split('.')[1]
serd_micro_version = meson.project_version().split('.')[2]
serd_src_root = meson.current_source_dir()
version_suffix = '-@0@'.format(serd_major_version)
versioned_name = 'serd' + version_suffix

# Load build tools
pkg = import('pkgconfig')
cc = meson.get_compiler('c')

if not get_option('bindings_cpp').disabled()
  add_languages('cpp', required: get_option('bindings_cpp').enabled())
  cpp = meson.get_compiler('cpp')
endif

# Set ultra strict warnings for developers, if requested
c_warnings = []
c_suppressions = []
if get_option('strict')
  subdir('meson')

  c_warnings = all_c_warnings

  if cc.get_id() == 'clang'
    c_suppressions += [
      '-Wno-format-nonliteral',
      '-Wno-nullability-extension',
      '-Wno-nullable-to-nonnull-conversion',
      '-Wno-padded',
      '-Wno-reserved-id-macro',
    ]
  elif cc.get_id() == 'gcc'
    c_suppressions += [
      '-Wno-float-conversion', # MinGW
      '-Wno-format-nonliteral',
      '-Wno-inline',
      '-Wno-null-dereference', # ARM32
      '-Wno-padded',
      '-Wno-strict-overflow',
      '-Wno-switch-default',
      '-Wno-unsuffixed-float-constants',
      '-Wno-unused-const-variable',
    ]
    if host_machine.system() == 'windows'
      c_warnings += [
        '-Wno-format', # MinGW
        '-Wno-suggest-attribute=format', # MinGW
      ]
    endif
  elif cc.get_id() == 'msvc'
    c_suppressions += [
      '/wd4061',  # enumerator in switch is not explicitly handled
      '/wd4200',  # nonstandard: zero-sized array in struct/union
      '/wd4514',  # unreferenced inline function has been removed
      '/wd4706',  # assignment within conditional expression
      '/wd4710',  # function not inlined
      '/wd4711',  # function selected for automatic inline expansion
      '/wd4774',  # format string is not a string literal
      '/wd4800',  # implicit conversion from int to bool
      '/wd4820',  # padding added after construct
      '/wd4996',  # POSIX name for this item is deprecated
      '/wd5045',  # will insert Spectre mitigation for memory load
    ]
  endif

endif

c_warnings += cc.get_supported_arguments(c_suppressions)

# Add special arguments for MSVC
if cc.get_id() == 'msvc'
  msvc_args = [
    '/D_CRT_SECURE_NO_WARNINGS',
    '/TP',
    '/experimental:external',
    '/external:W0',
    '/external:anglebrackets',
  ]

  add_project_arguments(msvc_args, language: ['c'])
endif

c_headers = ['include/serd/serd.h']
c_header_files = files(c_headers)
c_header = files('include/serd/serd.h')

sources = [
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
  'src/memory.c',
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
]

# System libraries
m_dep = cc.find_library('m', required: false)

# System defines needed to expose platform APIs
platform_args = []
if host_machine.system() != 'windows'
  platform_args += [
    '-D_POSIX_C_SOURCE=200809L',
    '-D_XOPEN_SOURCE=600',
  ]
endif

# Programs used by the build
cython = find_program('cython', required: get_option('bindings_py').enabled())
doxygen = find_program('doxygen', required: get_option('docs').enabled())
mandoc = find_program('mandoc', required: false)
sphinx_build = find_program(
  'sphinx-build',
  required: (get_option('docs').enabled() or
             (get_option('bindings_py').enabled() and
              get_option('tests'))))

# Determine library type and the flags needed to build it
library_name = versioned_name
if get_option('default_library') == 'both'
  if host_machine.system() == 'windows'
    error('default_library=both is not supported on Windows')
  endif

  prog_args = []
  library_type = 'both_libraries'
  library_args = ['-DSERD_INTERNAL', '-DZIX_API=']
elif get_option('default_library') == 'shared'
  if host_machine.system() == 'windows'
    # Meson annoyingly adds its own suffix, so remove this one
    library_name = 'serd'
  endif

  prog_args = []
  library_type = 'shared_library'
  library_args = ['-DSERD_INTERNAL', '-DZIX_API=']
else
  prog_args = ['-DSERD_STATIC']
  library_type = 'static_library'
  library_args = [
    '-DEXESS_STATIC',
    '-DREREX_STATIC',
    '-DSERD_INTERNAL',
    '-DSERD_STATIC',
    '-DZIX_STATIC',
  ]
endif

# Define version constants to trigger a warning if those in the code don't mwatch
library_args += ['-DSERD_MAJOR_VERSION=@0@'.format(serd_major_version),
                 '-DSERD_MINOR_VERSION=@0@'.format(serd_minor_version),
                 '-DSERD_MICRO_VERSION=@0@'.format(serd_micro_version)]

zix_dep = dependency('zix-0',
                     version: '>= 0.0.1',
                     fallback: ['zix', 'zix_dep'])

exess_dep = dependency('exess-0',
                       version: '>= 0.0.1',
                       fallback: ['exess', 'exess_dep'])

# Build shared and/or static library/libraries
libserd = build_target(
  library_name,
  sources,
  version: meson.project_version(),
  include_directories: include_directories('include', 'src'),
  c_args: c_warnings + platform_args + library_args,
  dependencies: [exess_dep, m_dep, zix_dep],
  gnu_symbol_visibility: 'hidden',
  install: true,
  target_type: library_type)

serd_dep = declare_dependency(
  include_directories: include_directories(['include']),
  link_with: libserd)

pkg.generate(
  libserd,
  name: 'Serd',
  filebase: versioned_name,
  subdirs: [versioned_name],
  version: meson.project_version(),
  description: 'A lightweight library for working with RDF')

# Build command line tools
if get_option('tools')
  subdir('tools')
endif

# Install header to a versioned include directory
install_headers(c_headers, subdir: versioned_name / 'serd')

# C++ bindings
if is_variable('cpp')
  subdir('bindings/cpp')
endif

docdir = get_option('datadir') / 'doc'
if not get_option('docs').disabled()
  subdir('doc')
endif

if get_option('tests')
  subdir('test')
endif

# Python bindings
py_dep = disabler()
if not get_option('bindings_py').disabled()
  pymod = import('python')
  py = pymod.find_installation('python3')
  py_dep = py.dependency(required: get_option('bindings_py').enabled())
  if py_dep.found() and cython.found()
    subdir('bindings/python')
  endif
endif

if not meson.is_subproject() and meson.version().version_compare('>=0.53.0')
  summary('Tests', get_option('tests'), bool_yn: true)
  summary('Tools', get_option('tools'), bool_yn: true)
  summary('Python bindings', py_dep.found() and cython.found(), bool_yn: true)

  summary('Install prefix', get_option('prefix'))

  summary('Headers', get_option('prefix') / get_option('includedir'))
  summary('Libraries', get_option('prefix') / get_option('libdir'))

  if get_option('tools')
    summary('Executables', get_option('prefix') / get_option('bindir'))
    summary('Man pages', get_option('prefix') / get_option('mandir'))
  endif
endif
