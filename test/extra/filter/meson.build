base_uri = 'http://drobilla.net/sw/serd/test/filter/'

test(
  'filter',
  run_filter_suite,
  args: common_script_args + [
    '--pipe',
    serd_pipe,
    '--filter',
    serd_filter,
    files('manifest.ttl'),
    base_uri,
  ],
  env: test_env,
  suite: ['suite', 'extra'],
  timeout: 240,
)
