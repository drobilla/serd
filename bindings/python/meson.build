srcdir = meson.current_source_dir()
blddir = meson.current_build_dir()

if cc.get_id() == 'clang' or cc.get_id() == 'gcc'
  cython_c_args = ['-Wno-unused-variable']
endif

# Generate extension module C source code with cython
pyserd_c = custom_target(
  'serd.cpython.so',
  command: [cython, '-3', '--fast-fail', '-Wextra', '@INPUT0@', '-o', '@OUTPUT@'],
  input: ['serd.pyx'],
  output: 'pyserd.c',
  install: true,
  install_dir: py.get_install_dir())

# Compile extension module
pyserd = py.extension_module('serd',
                             pyserd_c,
                             c_args: cython_c_args,
                             dependencies: [py_dep, serd_dep])

# Set up an environment for loading the module from the build directory
python_env = environment()
python_env.set('PYTHONPATH', meson.current_build_dir())

# Run API unit tests
test('serd.py',
     py,
     args: ['-m', 'unittest', 'discover', '-b', '-v', srcdir],
     env: python_env,
     suite: ['python'])

if sphinx_build.found()
  # Test all code/output snippets in the documentation
  test('doctest',
       sphinx_build,
       args: ['-M', 'doctest', srcdir, blddir],
       env: python_env,
       suite: ['python'])

  if not get_option('docs').disabled()
    # Build HTML documentation from Python module
    py_docs = custom_target(
      'singlehtml documentation for serd python bindings',
      command: [sphinx_build, '-M', 'singlehtml',
                srcdir, blddir,
                '-E', '-q', '-t', 'singlehtml'],
      input: [pyserd],
      output: 'singlehtml',
      build_by_default: true,
      install: true,
      install_dir: docdir / 'serd-0')

    # Copy static resources
    subdir('_static')
  endif
endif
