base_uri = 'http://drobilla.net/sw/serd/test/pattern/'

test('pattern',
     run_pipe_suite,
     args: pipe_test_script_args + [
       files('manifest.ttl'),
       base_uri,
       '--',
       '-I',
       'variables',
     ],
     env: test_env,
     suite: ['suite', 'extra', 'pipe'],
     timeout: 240)

test('pattern',
     run_sort_suite,
     args: sort_test_script_args + [
       files('manifest.ttl'),
       base_uri,
       '--',
       '-I',
       'variables',
     ],
     env: test_env,
     suite: ['suite', 'extra', 'sort'],
     timeout: 240)
