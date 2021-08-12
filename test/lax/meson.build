base_uri = 'http://drobilla.net/sw/serd/test/lax/'

# This suite is run twice

# ... once with strict parsing to test the hard errors
test('lax.strict',
     run_test_suite,
     args: script_args + [files('manifest.ttl'), base_uri],
     env: test_env,
     is_parallel: false,
     suite: ['suite', 'extra'],
     timeout: 240)

# ... and once with lax parsing to tolerate them
test('lax.lax',
     run_test_suite,
     args: script_args + [
       files('manifest.ttl'),
       base_uri,
       '--',
       '-i',
       'lax',
     ],
     env: test_env,
     is_parallel: false,
     suite: ['suite', 'extra'],
     timeout: 240)
