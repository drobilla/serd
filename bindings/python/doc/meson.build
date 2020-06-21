# Copyright 2020-2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: CC0-1.0 OR ISC

srcdir = meson.current_source_dir()
blddir = meson.current_build_dir()

# Test all code snippets in the documentation
if sphinx_build.found()
  test('doctest',
       sphinx_build,
       args: ['-W', '-b', 'doctest', srcdir, blddir],
       env: python_env,
       suite: ['bindings', 'python'])
endif
