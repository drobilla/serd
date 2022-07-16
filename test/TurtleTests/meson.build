# Copyright 2020-2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

base_uri = 'http://www.w3.org/2013/TurtleTests/'

args = [
  '--syntax', 'Turtle',
  files('manifest.ttl'),
  base_uri
]

test('Turtle',
     run_pipe_suite,
     args: pipe_test_script_args + args,
     env: test_env,
     suite: ['suite', 'w3c', 'pipe'],
     timeout: 240)

test('Turtle',
     run_sort_suite,
     args: sort_test_script_args + args,
     env: test_env,
     suite: ['suite', 'w3c', 'sort'],
     timeout: 240)
