base_uri = 'http://drobilla.net/sw/serd/test/terse/'

args = [
  '--osyntax', 'turtle',
  files('manifest.ttl'),
  base_uri,
  '--',
  '-O', 'terse'
]

test('terse',
     run_pipe_suite,
     args: pipe_test_script_args + args,
     env: test_env,
     suite: ['suite', 'extra', 'pipe'],
     timeout: 240)

test('terse',
     run_sort_suite,
     args: sort_test_script_args + args,
     env: test_env,
     suite: ['suite', 'extra', 'sort'],
     timeout: 240)
