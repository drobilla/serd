# Copyright 2020-2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

versioned_cpp_name = 'serdpp' + version_suffix

# Set ultra strict warnings for developers, if requested
cpp_suppressions = []
if cpp.get_id() == 'clang'
  if get_option('warning_level') == 'everything'
    cpp_suppressions += [
      '-Wno-c++98-compat-pedantic',
      '-Wno-documentation-unknown-command',
      '-Wno-format-nonliteral',
      '-Wno-padded',
      '-Wno-unsafe-buffer-usage',
    ]
  endif

  if get_option('warning_level') in ['everything', '3']
    cpp_suppressions += [
      '-Wno-nullability-extension',
    ]
  endif

elif cpp.get_id() == 'gcc'
  if get_option('warning_level') == 'everything'
    cpp_suppressions += [
      '-Wno-abi-tag',
      '-Wno-float-equal',
      '-Wno-multiple-inheritance',
      '-Wno-padded',
      '-Wno-switch-default',
      '-Wno-unused-const-variable',
    ]
  endif

elif cpp.get_id() == 'msvc'
  if get_option('warning_level') == 'everything'
    cpp_suppressions += [
      '/wd4355', # 'this' used in base member initializer list
      '/wd4571', # structured exceptions are no longer caught
      '/wd4623', # default constructor implicitly deleted
      '/wd4625', # copy constructor implicitly deleted
      '/wd4626', # assignment operator implicitly deleted
      '/wd4710', # function not inlined
      '/wd4868', # may not enforce left-to-right evaluation order
      '/wd5026', # move constructor implicitly deleted
      '/wd5027', # move assignment operator implicitly deleted
    ]
  endif
endif

exess_cpp_args = cpp.get_supported_arguments(cpp_suppressions)

cpp_headers = [
  'include/serd/Flags.hpp',
  'include/serd/Optional.hpp',
  'include/serd/StringView.hpp',
  'include/serd/serd.hpp',
]

cpp_detail_headers = [
  'include/serd/detail/Copyable.hpp',
  'include/serd/detail/Wrapper.hpp',
]

cpp_header_files = files(cpp_headers)
cpp_detail_header_files = files(cpp_detail_headers)

serdpp_dep = declare_dependency(
  include_directories: include_directories(['include']),
  link_with: libserd,
)

pkg.generate(
  description: 'C++ bindings for serd',
  filebase: versioned_cpp_name,
  name: 'Serdpp',
  subdirs: [versioned_cpp_name],
  version: meson.project_version(),
)

# Install headers to a versioned include directory
install_headers(cpp_header_files, subdir: versioned_cpp_name / 'serd')
install_headers(
  cpp_detail_header_files,
  subdir: versioned_cpp_name / 'serd/detail',
)

cpp_test_args = (
  cpp_suppressions + platform_c_args + cpp.get_supported_arguments(
    ['-Wno-float-equal'],
  )
)

test(
  'serd.hpp',
  executable(
    'test_serd_hpp',
    'test/test_serd_hpp.cpp',
    include_directories: include_directories(['include']),
    cpp_args: exess_cpp_args + cpp_test_args,
    dependencies: [serd_dep, serdpp_dep],
  ),
  suite: ['bindings', 'cpp'],
)

if not get_option('docs').disabled() and not get_option('docs_cpp').disabled()
  subdir('doc')
endif
