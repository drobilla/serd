config = configuration_data()
config.set('SERD_VERSION', meson.project_version())

conf_py = configure_file(configuration: config,
                         input: files('../conf.py.in'),
                         output: 'conf.py')

configure_file(copy: true,
               input: files('../summary.rst'),
               output: 'summary.rst')

c_rst_files = files(
  'index.rst',
  'overview.rst',
)

foreach f : c_rst_files
  configure_file(copy: true, input: f, output: '@PLAINNAME@')
endforeach

subdir('xml')
subdir('api')

docs = custom_target(
  'singlehtml documentation for serd',
  command: [sphinx_build, '-M', 'singlehtml',
            meson.current_build_dir(), meson.current_build_dir(),
            '-E', '-q', '-t', 'singlehtml'],
  input: [c_rst_files, c_serd_rst, c_index_xml],
  output: 'singlehtml',
  build_by_default: true,
  install: true,
  install_dir: docdir / 'serd-0')

docs = custom_target(
  'html documentation for serd',
  command: [sphinx_build, '-M', 'html',
            meson.current_build_dir(), meson.current_build_dir(),
            '-E', '-q', '-t', 'html'],
  input: [c_rst_files, c_serd_rst, c_index_xml],
  output: 'html',
  build_by_default: true,
  install: true,
  install_dir: docdir / 'serd-0')
