base_uri = 'http://www.w3.org/2013/NTriplesTests/'

args = [
  '--syntax', 'NTriples',
  files('manifest.ttl'),
  base_uri
]

test('NTriples',
     run_test_suite,
     args: script_args + args,
     env: test_env,
     suite: ['suite', 'w3c'],
     timeout: 240)
