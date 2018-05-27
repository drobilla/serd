base_uri = 'http://drobilla.net/sw/serd/test/good/'

args = [
  files('manifest.ttl')
]

test('pretty',
     run_pretty_suite,
     args: common_script_options + [
       '--tool', serd_pipe,
       '--out-dir',
       meson.current_build_dir() / 'pipe'
     ] + args,
     env: test_env,
     suite: ['suite', 'extra', 'pipe'],
     timeout: 240)

test('pretty',
     run_pretty_suite,
     args: common_script_options + [
       '--tool', serd_sort,
       '--out-dir',
       meson.current_build_dir() / 'sort'
     ] + args,
     env: test_env,
     suite: ['suite', 'extra', 'sort'],
     timeout: 240)
