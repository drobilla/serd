base_uri = 'https://w3c.github.io/json-ld-api/tests/'

args = [
  '--syntax', 'JSONLD',
  files('toRdf/manifest.ttl'),
  base_uri
]

test('JSONLD',
     run_pipe_suite,
     args: pipe_test_script_args + args,
     env: test_env,
     suite: ['suite', 'w3c', 'pipe'],
     timeout: 240)

# test('JSONLD',
#      run_sort_suite,
#      args: sort_test_script_args + args,
#      env: test_env,
#      suite: ['suite', 'w3c', 'sort'],
#      timeout: 240)
