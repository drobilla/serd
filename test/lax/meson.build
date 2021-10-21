base_uri = 'http://drobilla.net/sw/serd/test/lax/'

# This suite is run twice

# ... once with strict parsing to test the hard errors
test('lax.strict',
     run_pipe_suite,
     args: pipe_test_script_args + [files('manifest.ttl'), base_uri],
     env: test_env,
     is_parallel: false,
     suite: ['suite', 'extra'],
     timeout: 240)

# ... and once with lax parsing to tolerate them
test('lax.lax',
     run_pipe_suite,
     args: pipe_test_script_args + [
       files('manifest.ttl'),
       base_uri,
       '--',
       '-I', 'lax',
       '-O', 'lax'],
     env: test_env,
     is_parallel: false,
     suite: ['suite', 'extra'],
     timeout: 240)
