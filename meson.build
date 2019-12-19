project('serd', ['c'],
        version: '1.0.1',
        license: 'ISC',
        meson_version: '>= 0.49.2',
        default_options: [
          'b_ndebug=if-release',
          'buildtype=release',
          'c_std=c99',
          'default_library=shared',
          'warning_level=2',
        ])

serd_src_root = meson.current_source_dir()
major_version = meson.project_version().split('.')[0]
version_suffix = '-@0@'.format(major_version)
versioned_name = 'serd' + version_suffix

# Load build tools
pkg = import('pkgconfig')
cc = meson.get_compiler('c')

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
      '-Wno-padded',
      '-Wno-strict-overflow',
      '-Wno-switch-default',
      '-Wno-unsuffixed-float-constants',
      '-Wno-unused-const-variable',
    ]
  elif cc.get_id() == 'msvc'
    c_suppressions += [
      '/wd4061',  # enumerator in switch is not explicitly handled
      '/wd4200',  # nonstandard: zero-sized array in struct/union
      '/wd4514',  # unreferenced inline function has been removed
      '/wd4706',  # assignment within conditional expression
      '/wd4710',  # function not inlined
      '/wd4711',  # function selected for automatic inline expansion
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
  'src/base64.c',
  'src/byte_sink.c',
  'src/byte_source.c',
  'src/canon.c',
  'src/cursor.c',
  'src/env.c',
  'src/filter.c',
  'src/inserter.c',
  'src/iter.c',
  'src/model.c',
  'src/n3.c',
  'src/node.c',
  'src/node_syntax.c',
  'src/nodes.c',
  'src/range.c',
  'src/reader.c',
  'src/sink.c',
  'src/statement.c',
  'src/string.c',
  'src/syntax.c',
  'src/system.c',
  'src/uri.c',
  'src/validate.c',
  'src/world.c',
  'src/writer.c',
  'src/zix/btree.c',
  'src/zix/digest.c',
  'src/zix/hash.c',
]

# System libraries
m_dep = cc.find_library('m', required: false)

# Determine library type and the flags needed to build it
if get_option('default_library') == 'both'
  if host_machine.system() == 'windows'
    error('default_library=both is not supported on Windows')
  endif

  prog_args = []
  library_type = 'both_libraries'
  library_args = ['-DSERD_INTERNAL']
elif get_option('default_library') == 'shared'
  prog_args = []
  library_type = 'shared_library'
  library_args = ['-DSERD_INTERNAL']
else
  prog_args = ['-DSERD_STATIC']
  library_type = 'static_library'
  library_args = [
    '-DEXESS_STATIC',
    '-DREREX_STATIC',
    '-DSERD_INTERNAL',
    '-DSERD_STATIC',
    '-DZIX_INTERNAL',
    '-DZIX_STATIC',
  ]
endif

exess_dep = dependency('exess-0',
                       version: '>= 0.0.1',
                       fallback: ['exess', 'exess_dep'])

rerex_dep = dependency('rerex-0',
                       version: '>= 0.0.1',
                       fallback: ['rerex', 'rerex_dep'])

# Build shared and/or static library/libraries
libserd = build_target(
  versioned_name,
  sources,
  version: meson.project_version(),
  include_directories: include_directories('include', 'src'),
  c_args: c_warnings + library_args,
  dependencies: [exess_dep, m_dep, rerex_dep],
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

# Build serdi command line utility
if get_option('utils')

  serdi = executable('serdi', 'src/serdi.c',
                     c_args: c_warnings + prog_args,
                     install: true,
                     dependencies: serd_dep)

  if not get_option('docs').disabled()
    install_man('doc/serdi.1')
  endif
endif

# Install header to a versioned include directory
install_headers(c_headers, subdir: versioned_name / 'serd')

if not get_option('docs').disabled()
  subdir('doc')
endif

if get_option('tests')
  subdir('test')
endif

if not meson.is_subproject() and meson.version().version_compare('>=0.53.0')
  summary('Tests', get_option('tests'), bool_yn: true)
  summary('Utilities', get_option('utils'), bool_yn: true)

  summary('Install prefix', get_option('prefix'))

  summary('Headers', get_option('prefix') / get_option('includedir'))
  summary('Libraries', get_option('prefix') / get_option('libdir'))

  if get_option('utils')
    summary('Executables', get_option('prefix') / get_option('bindir'))
    summary('Man pages', get_option('prefix') / get_option('mandir'))
  endif
endif
