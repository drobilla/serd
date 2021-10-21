base_uri = 'http://drobilla.net/sw/serd/test/good/'

test('good',
     run_pipe_suite,
     args: pipe_test_script_args + [files('manifest.ttl'), base_uri],
     env: test_env,
     suite: ['suite', 'extra', 'pipe'],
     timeout: 240)

test('good',
     run_sort_suite,
     args: sort_test_script_args + [files('manifest.ttl'), base_uri],
     env: test_env,
     suite: ['suite', 'extra', 'sort'],
     timeout: 240)
