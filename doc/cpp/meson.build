config = configuration_data()
config.set('SERD_VERSION', meson.project_version())

conf_py = configure_file(configuration: config,
                         input: files('../conf.py.in'),
                         output: 'conf.py')

configure_file(copy: true,
               input: files('../summary.rst'),
               output: 'summary.rst')

configure_file(copy: true, input: files('overview.cpp'), output: 'overview.cpp')

executable('overview', files('overview.cpp'),
           dependencies: [serd_dep, serdpp_dep])

cpp_rst_files = files(
  'cpp_facilities.rst',
  'index.rst',
  'model.rst',
  'nodes.rst',
  'overview.rst',
  'reading_and_writing.rst',
  'statements.rst',
  'stream_processing.rst',
  'using_serd.rst',
  'world.rst',
)

foreach f : cpp_rst_files
  configure_file(copy: true, input: f, output: '@PLAINNAME@')
endforeach

subdir('xml')
subdir('api')

docs = custom_target(
  'singlehtml C++ documentation for serd',
  command: [sphinx_build, '-M', 'singlehtml',
            meson.current_build_dir(), meson.current_build_dir(),
            '-E', '-q', '-t', 'singlehtml'],
  input: [cpp_rst_files, cpp_serd_rst, cpp_index_xml],
  output: 'singlehtml',
  build_by_default: true,
  install: true,
  install_dir: docdir / 'serd-0')

docs = custom_target(
  'html C++ documentation for serd',
  command: [sphinx_build, '-M', 'html',
            meson.current_build_dir(), meson.current_build_dir(),
            '-E', '-q', '-t', 'html'],
  input: [cpp_rst_files, cpp_serd_rst, cpp_index_xml],
  output: 'html',
  build_by_default: true,
  install: true,
  install_dir: docdir / 'serd-0')
