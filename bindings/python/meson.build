# Copyright 2020-2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: CC0-1.0 OR ISC

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

cython_args = ['-3', '--fast-fail', '-Wextra']
if get_option('werror')
  cython_args += ['-Werror']
endif
if get_option('buildtype') == 'debug'
  cython_args += ['--gdb']
endif

# Generate extension module C source code with cython
cython_serd_c = custom_target(
  'serd.c cython extension',
  command: [cython] + cython_args + ['@INPUT0@', '-o', '@OUTPUT@'],
  input: serd_pyx,
  output: 'serd.c')

# Compile extension module
pyserd = py.extension_module('serd',
                             cython_serd_c,
                             c_args: cython_c_args,
                             dependencies: [py_dep, serd_dep],
                             install: true,
                             install_dir: py.get_install_dir())

# Set up an environment for loading the module from the build directory
python_env = environment()
python_env.set('PYTHONPATH', meson.current_build_dir())

# Run API unit tests
test('serd.pyx',
     py,
     args: ['-m', 'unittest', 'discover', '-b', '-v', srcdir],
     env: python_env,
     suite: ['bindings', 'python'])

subdir('doc')
