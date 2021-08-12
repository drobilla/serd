base_uri = 'http://drobilla.net/sw/serd/test/terse/'

test('terse',
     run_test_suite,
     args: script_args + [
       '--osyntax', 'turtle',
       files('manifest.ttl'),
       base_uri,
       '--',
       '-o',
       'terse',
     ],
     env: test_env,
     is_parallel: false,
     suite: ['suite', 'extra'],
     timeout: 240)
