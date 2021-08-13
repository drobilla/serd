base_uri = 'http://drobilla.net/sw/serd/test/good/'

args = [files('manifest.ttl')]

test('pretty',
     run_pretty_suite,
     args: pipe_test_script_args + args,
     env: test_env,
     suite: ['suite', 'extra', 'pipe'],
     timeout: 240)

test('pretty',
     run_pretty_suite,
     args: sort_test_script_args + args,
     env: test_env,
     suite: ['suite', 'extra', 'sort'],
     timeout: 240)
