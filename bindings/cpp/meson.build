versioned_cpp_name = 'serdxx' + version_suffix

# Set ultra strict warnings for developers, if requested
cpp_suppressions = []
if get_option('strict')
  if cpp.get_id() == 'clang'
    cpp_suppressions += [
      '-Wno-documentation-unknown-command',
      '-Wno-format-nonliteral',
      '-Wno-nullability-extension',
      '-Wno-padded',
    ]
  elif cpp.get_id() == 'gcc'
    cpp_suppressions += [
      '-Wno-abi-tag',
      '-Wno-float-equal',
      '-Wno-multiple-inheritance',
      '-Wno-padded',
      '-Wno-switch-default',
    ]
  elif cpp.get_id() == 'msvc'
    cpp_suppressions += [
      '/wd4355',  # 'this' used in base member initializer list
      '/wd4571',  # structured exceptions are no longer caught
      '/wd4623',  # default constructor implicitly deleted
      '/wd4625',  # copy constructor implicitly deleted
      '/wd4626',  # assignment operator implicitly deleted
      '/wd4710',  # function not inlined
      '/wd4868',  # may not enforce left-to-right evaluation order
      '/wd5026',  # move constructor implicitly deleted
      '/wd5027',  # move assignment operator implicitly deleted
    ]
  endif
endif

exess_cpp_args = cpp.get_supported_arguments(cpp_suppressions)

cpp_headers = [
  'include/serd/serd.hpp',
]

cpp_detail_headers = [
  'include/serd/detail/Copyable.hpp',
  'include/serd/detail/Flags.hpp',
  'include/serd/detail/Optional.hpp',
  'include/serd/detail/StringView.hpp',
  'include/serd/detail/Wrapper.hpp',
]

serdxx_dep = declare_dependency(
  include_directories: include_directories(['include']),
  link_with: libserd)

pkg.generate(
  description: 'C++ bindings for serd',
  filebase: versioned_cpp_name,
  name: 'Serdxx',
  subdirs: [versioned_cpp_name],
  version: meson.project_version())

# Install headers to a versioned include directory
install_headers(cpp_headers, subdir: versioned_cpp_name / 'serd')
install_headers(cpp_detail_headers, subdir: versioned_cpp_name / 'serd/detail')

cpp_test_args = prog_args + cpp.get_supported_arguments(['-Wno-float-equal'])

test('bindings',
     executable('test_serd_hpp',
                'test/test_serd_hpp.cpp',
                include_directories: include_directories(['include']),
                cpp_args: exess_cpp_args + cpp_test_args,
                dependencies: [serd_dep, serdxx_dep]),
     suite: 'cpp')
