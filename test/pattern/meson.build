base_uri = 'http://drobilla.net/sw/serd/test/pattern/'

test('pattern',
     run_test_suite,
     args: script_args + [
       files('manifest.ttl'),
       base_uri,
       '--',
       '-i',
       'variables',
     ],
     env: test_env,
     suite: ['suite', 'extra'],
     timeout: 240)
