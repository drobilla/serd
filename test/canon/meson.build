base_uri = 'http://drobilla.net/sw/serd/test/canon/'

test('canon',
     run_test_suite,
     args: script_args + [
       files('manifest.ttl'),
       base_uri,
       '--',
       '-C',
     ],
     env: test_env,
     suite: ['suite', 'extra'],
     timeout: 240)
