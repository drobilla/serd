base_uri = 'http://drobilla.net/sw/serd/test/good/'

test('good',
     run_test_suite,
     args: script_args + [files('manifest.ttl'), base_uri],
     env: test_env,
     suite: ['suite', 'extra'],
     timeout: 240)
