srcdir = meson.current_source_dir()
blddir = meson.current_build_dir()

if cc.get_id() == 'clang' or cc.get_id() == 'gcc'
  cython_c_args = [
    '-Wno-deprecated-declarations',
    '-Wno-unused-variable',
  ]
endif

# Copy files to make this build directory a Python package
configure_file(copy: true, input: files('../../COPYING'), output: 'LICENSE')
configure_file(copy: true, input: files('../../NEWS'), output: 'NEWS')
configure_file(copy: true, input: files('../../README.md'), output: 'README.md')
configure_file(copy: true, input: files('MANIFEST.in'), output: 'MANIFEST.in')
configure_file(copy: true, input: files('setup.py'), output: 'setup.py')
configure_file(copy: true, input: files('../../include/serd/serd.h'), output: 'serd.h')

serd_pyx = configure_file(copy: true, input: files('serd.pyx'), output: 'serd.pyx')

# Generate extension module C source code with cython
cython_serd_c = custom_target(
  'serd.c cython extension',
  command: [cython, '-3', '--fast-fail', '-Wextra', '@INPUT0@', '-o', '@OUTPUT@'],
  input: serd_pyx,
  output: 'serd.c',
  install: true,
  install_dir: py.get_install_dir())

# Compile extension module
pyserd = py.extension_module('serd',
                             cython_serd_c,
                             c_args: cython_c_args,
                             dependencies: [py_dep, serd_dep])

# Set up an environment for loading the module from the build directory
python_env = environment()
python_env.set('PYTHONPATH', meson.current_build_dir())

# Run API unit tests
test('serd.pyx',
     py,
     args: ['-m', 'unittest', 'discover', '-b', '-v', srcdir],
     env: python_env,
     suite: ['bindings', 'python'])

if sphinx_build.found()
  # Test all code/output snippets in the documentation
  test('doctest',
       sphinx_build,
       args: ['-W', '-b', 'doctest', srcdir, blddir],
       env: python_env,
       suite: ['bindings', 'python'])

  if not get_option('docs').disabled()
    py_html_docs = custom_target(
      'html documentation for serd python bindings',
      command: [sphinx_build,
                '-b', 'html', '-W', '-E', '-a', '-q', '-t', 'html',
                srcdir, blddir],
      env: python_env,
      input: [pyserd],
      output: 'html',
      build_by_default: true,
      install: true,
      install_dir: docdir / 'serd-0')

    py_singlehtml_docs = custom_target(
      'singlehtml documentation for serd python bindings',
      command: [sphinx_build,
                '-b', 'singlehtml', '-W', '-E', '-a', '-q', '-t', 'singlehtml',
                srcdir, blddir],
      env: python_env,
      input: [pyserd],
      output: 'singlehtml',
      build_by_default: true,
      install: true,
      install_dir: docdir / 'serd-0')

    # Copy static resources
    subdir('_static')
  endif
endif
