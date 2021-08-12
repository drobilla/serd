base_uri = 'http://drobilla.net/sw/serd/test/bad/'

test('bad',
     run_pipe_suite,
     args: pipe_test_script_args + [files('manifest.ttl'), base_uri],
     env: test_env,
     suite: ['suite', 'extra'],
     timeout: 240)

test('bad',
     run_sort_suite,
     args: sort_test_script_args + [files('manifest.ttl'), base_uri],
     env: test_env,
     suite: ['suite', 'extra', 'sort'],
     timeout: 240)
