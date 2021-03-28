config = configuration_data()
config.set('SERD_VERSION', meson.project_version())

if mandoc.found()
  config.set('SERD_COMMAND_LINE_INDEX_ENTRY', '\n   command_line_tools\n')
  config.set('SERD_PIPE_LINK', '`serd-pipe <../../man/serd-pipe.html>`_')
  config.set('SERD_SORT_LINK', '`serd-sort <../../man/serd-sort.html>`_')
  config.set('SERD_FILTER_LINK', '`serd-filter <../../man/serd-filter.html>`_')
  config.set('SERD_VALIDATE_LINK', '`serd-validate <../../man/serd-validate.html>`_')
else
  config.set('SERD_COMMAND_LINE_INDEX_ENTRY', '')
  config.set('SERD_PIPE_LINK', '``serd-pipe``')
  config.set('SERD_SORT_LINK', '``serd-sort``')
  config.set('SERD_FILTER_LINK', '``serd-filter``')
  config.set('SERD_VALIDATE_LINK', '``serd-validate``')
endif

conf_py = configure_file(configuration: config,
                         input: files('../conf.py.in'),
                         output: 'conf.py')

configure_file(copy: true,
               input: files('../summary.rst'),
               output: 'summary.rst')

configure_file(copy: true,
               input: files('overview_code.c'),
               output: 'overview_code.c')

executable('overview_code', files('overview_code.c'), dependencies: [serd_dep])

c_rst_files = files(
  '../data_model.rst',
  '../getting_started.rst',
  'index.rst',
  'model.rst',
  'nodes.rst',
  'overview.rst',
  'reading_and_writing.rst',
  'statements.rst',
  'stream_processing.rst',
  'string_views.rst',
  'using_serd.rst',
  'world.rst',
)

configure_file(configuration: config,
               input: files('../command_line_tools.rst.in'),
               output: 'command_line_tools.rst')

foreach f : c_rst_files
  configure_file(copy: true, input: f, output: '@PLAINNAME@')
endforeach

subdir('xml')
subdir('api')

docs = custom_target(
  'singlehtml C documentation for serd',
  command: [sphinx_build, '-M', 'singlehtml',
            meson.current_build_dir(), meson.current_build_dir(),
            '-W', '-E', '-a', '-q', '-t', 'singlehtml'],
  input: [c_rst_files, c_serd_rst, c_index_xml],
  output: 'singlehtml',
  build_by_default: true,
  install: true,
  install_dir: docdir / versioned_name)

docs = custom_target(
  'html C documentation for serd',
  command: [sphinx_build, '-M', 'html',
            meson.current_build_dir(), meson.current_build_dir(),
            '-W', '-E', '-a', '-q', '-t', 'html'],
  input: [c_rst_files, c_serd_rst, c_index_xml],
  output: 'html',
  build_by_default: true,
  install: true,
  install_dir: docdir / versioned_name)

docs = custom_target(
  'epub C documentation for serd',
  command: [sphinx_build, '-M', 'epub',
            meson.current_build_dir(), meson.current_build_dir(),
            '-W', '-E', '-a', '-q', '-t', 'epub'],
  input: [c_rst_files, c_serd_rst, c_index_xml],
  output: 'epub',
  build_by_default: true,
  install: true,
  install_dir: docdir / versioned_name)
