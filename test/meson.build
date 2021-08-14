# Copyright 2020-2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

run_filter_suite = find_program('run_filter_suite.py')
run_pipe_suite = find_program('run_pipe_suite.py')
run_pretty_suite = find_program('run_pretty_suite.py')
run_sort_suite = find_program('run_sort_suite.py')

wrapper = meson.get_external_property('exe_wrapper', '')

####################
# Project Metadata #
####################

if get_option('strict')
  # Check release metadata
  if not meson.is_subproject()
    autoship = find_program('autoship', required: false)
    if autoship.found()
      test('autoship', autoship, args: ['test', serd_src_root], suite: 'data')
    endif
  endif

  # Check licensing metadata
  reuse = find_program('reuse', required: false)
  if reuse.found()
    test('REUSE', reuse,
         args: ['--root', serd_src_root, 'lint'],
         suite: 'data')
  endif
endif

###################
# Header Warnings #
###################

subdir('headers')

##############
# Unit Tests #
##############

unit_tests = [
  'caret',
  'cursor',
  'env',
  'free_null',
  'log',
  'model',
  'node',
  'node_syntax',
  'nodes',
  'overflow',
  'reader',
  'reader_writer',
  'sink',
  'statement',
  'string',
  'terse_write',
  'uri',
  'world',
  'writer',
]

test_env = []
if build_machine.system() == 'windows' and host_machine.system() == 'windows'
  # For Windows, we need to add to PATH so that DLLs are found
  test_env = ['PATH=@0@;@1@'.format('subprojects' / 'exess',
                                    'subprojects' / 'zix')]
endif

foreach unit : unit_tests
  test(
    unit,
    executable(
      'test_@0@'.format(unit),
      files('test_@0@.c'.format(unit)),
      c_args: c_suppressions + platform_args,
      dependencies: [serd_dep, zix_dep],
    ),
    env: test_env,
    suite: 'unit',
  )
endforeach

serd_ttl = files('../serd.ttl')[0]
common_script_options = []
if wrapper != ''
  common_script_options = ['--wrapper', wrapper]
endif

# Test serd-pipe as the main entry point to the common tool code
if is_variable('serd_pipe')
  tool = serd_pipe
  pipe_test_script_args = common_script_options + ['--tool', serd_pipe]

  # Basic valid arguments

  good_args = [
    ['-V'],
    ['-h'],
  ]

  foreach args : good_args
    test(args[0],
         tool,
         args: args,
         env: test_env,
         suite: ['tools', 'pipe', 'options'])
  endforeach

  # Basic invalid arguments

  bad_args = [
    ['-B', 'nonuriorpath'],
    ['-B'],
    ['-I', 'unknown'],
    ['-I'],
    ['-O', 'unknown'],
    ['-O'],
    ['-b', '-1'],
    ['-b', '1024junk'],
    ['-b', '9223372036854775807'],
    ['-b'],
    ['-k', '-1'],
    ['-k', '1024junk'],
    ['-k', '9223372036854775807'],
    ['-k'],
    ['-o'],
    ['-s', '<foo> a <Bar> .'],
    ['-s'],
    ['-z'],
    ['/no/such/file'],
  ]

  foreach args : bad_args
    test(' '.join(args),
         tool,
         args: args,
         env: test_env,
         should_fail: true,
         suite: ['tools', 'pipe', 'options'])
  endforeach

  test('remote',
       tool,
       args: ['ftp://unsupported.org'],
       env: test_env,
       should_fail: true,
       suite: ['tools', 'pipe', 'options'])

  test('bad_rebase',
       tool,
       args: ['-B', 'rebase', serd_ttl],
       env: test_env,
       should_fail: true,
       suite: ['tools', 'pipe', 'options'])

  test('base',
       files('test_base.py'),
       args: pipe_test_script_args,
       env: test_env,
       suite: ['tools', 'pipe', 'options'])

  test('dir_base',
       tool,
       args: ['-B', serd_src_root / '', serd_ttl],
       env: test_env,
       suite: ['tools', 'pipe', 'options'])

  # Smoke test common handling code for environment color configuration

  test('CLICOLOR_FORCE',
       tool,
       args: files('bad/bad-lang.ttl'),
       env: test_env + ['CLICOLOR_FORCE=1'],
       should_fail: true,
       suite: ['color'])

  test('CLICOLOR',
       tool,
       args: files('bad/bad-lang.ttl'),
       env: test_env + ['CLICOLOR=0'],
       should_fail: true,
       suite: ['color'])

  test('NO_COLOR',
       tool,
       args: files('bad/bad-lang.ttl'),
       env: test_env + ['NO_COLOR=1'],
       should_fail: true,
       suite: ['color'])

  # Different input sources

  test('stdin',
       files('test_stdin.py'),
       args: pipe_test_script_args,
       env: test_env,
       suite: ['tools', 'pipe', 'input'])

  test('multifile',
       files('test_multifile.py'),
       args: pipe_test_script_args + [meson.current_source_dir() / 'multifile'],
       env: test_env,
       suite: ['tools', 'pipe', 'input'])

  test('serd.ttl',
       tool,
       args: [serd_ttl],
       env: test_env,
       suite: ['tools', 'pipe', 'input'])

  test('good_string',
       tool,
       args: ['-I', 'turtle', '-s', '[] a [] .'],
       env: test_env,
       suite: ['tools', 'pipe', 'input'])

  test('baseless_string',
       tool,
       args: ['-s', '<foo> a <Bar> .'],
       env: test_env,
       should_fail: true,
       suite: ['tools', 'pipe', 'input'])

  test('unknown_type',
       tool,
       args: files('../README.md'),
       env: test_env,
       should_fail: true,
       suite: ['tools', 'pipe', 'input'])

  # Suppressed output

  test('empty',
       files('test_empty.py'),
       args: pipe_test_script_args + files('../serd.ttl'),
       env: test_env,
       suite: ['tools', 'pipe', 'output'])

  test('quiet',
       files('test_quiet.py'),
       args: pipe_test_script_args + files('bad/bad-base.ttl'),
       env: test_env,
       suite: ['tools', 'pipe', 'output'])

  # IO errors

  test('read_dir',
       tool,
       args: [serd_src_root],
       env: test_env,
       should_fail: true,
       suite: ['tools', 'pipe', 'input'])

  if host_machine.system() == 'linux'
    test('unreadable',
         tool,
         args: ['/sys/bus/pci/rescan'],
         env: test_env,
         should_fail: true,
         suite: ['tools', 'pipe', 'input'])
  endif

  test('write_error',
       files('test_write_error.py'),
       args: pipe_test_script_args + [serd_ttl],
       env: test_env,
       suite: ['tools', 'pipe', 'output'])

  test('missing_output',
       tool,
       args: ['-o', '/does/not/exist.ttl', serd_src_root / 'serd.ttl'],
       env: test_env,
       should_fail: true,
       suite: ['tools', 'pipe', 'output'])
endif

# Test specifics for serd-sort
if is_variable('serd_sort')
  tool = serd_sort
  sort_test_script_args = common_script_options + ['--tool', serd_sort]

  # Basic valid arguments

  good_args = [
    ['-V'],
    ['-h'],
  ]

  foreach args : good_args
    test(args[0],
         tool,
         args: args,
         env: test_env,
         suite: ['tools', 'sort', 'options'])
  endforeach

  # Basic invalid arguments

  bad_args = [
    ['-c', 'CHAOS', '-'],
    ['-o'],
    ['-s'],
    ['-z', '-'],
    ['-z', '-'],
    ['/no/such/file'],
  ]

  foreach args : bad_args
    test(' '.join(args),
         tool,
         args: args,
         env: test_env,
         should_fail: true,
         suite: ['tools', 'sort', 'options'])
  endforeach

  # IO errors

  test('read_dir',
       tool,
       args: [serd_src_root],
       env: test_env,
       should_fail: true,
       suite: ['tools', 'sort', 'input'])

  test('missing_output',
       tool,
       args: ['-o', '/does/not/exist.ttl', serd_src_root / 'serd.ttl'],
       env: test_env,
       should_fail: true,
       suite: ['tools', 'sort', 'output'])

  # Different input sources

  test('stdin',
       files('test_stdin.py'),
       args: sort_test_script_args,
       env: test_env,
       suite: ['tools', 'sort', 'input'])

  # Collation test suite

  test('sort',
       files('test_sort.py'),
       args: sort_test_script_args + files('sort/input.trig'),
       env: test_env,
       suite: ['tools', 'sort'])

endif

# Test specifics for serd-filter
if is_variable('serd_filter')
  tool = serd_filter
  filter_test_script_args = common_script_options + ['--tool', serd_filter]

  # Basic valid arguments

  good_args = [
    ['-V'],
    ['-h'],
  ]

  foreach args : good_args
    test(args[0],
         tool,
         args: args,
         env: test_env,
         suite: ['tools', 'filter', 'options'])
  endforeach

  # Basic invalid arguments

  bad_args = [
    ['-f', '/no/such/file.nt', '-'],
    ['-z'],
  ]

  foreach args : bad_args
    test(' '.join(args),
         tool,
         args: args,
         env: test_env,
         should_fail: true,
         suite: ['tools', 'filter', 'options'])
  endforeach

  test('garbage_pattern',
       tool,
       args: ['junk', serd_ttl],
       env: test_env,
       should_fail: true,
       suite: ['tools', 'filter', 'options'])

  test('multiple_patterns',
       tool,
       args: ['?s ?p ?o .\n?t ?u ?v .\n',
              serd_src_root / 'serd.ttl'],
       env: test_env,
       should_fail: true,
       suite: ['tools', 'filter', 'output'])

  test('missing_output',
       tool,
       args: ['-o', '/does/not/exist.ttl',
              '?s ?p ?o .',
              serd_src_root / 'serd.ttl'],
       env: test_env,
       should_fail: true,
       suite: ['tools', 'filter', 'output'])

  # Different input sources

  test('filter_dir',
       tool,
       args: ['?s ?p ?o .', serd_src_root],
       env: test_env,
       should_fail: true,
       suite: ['tools', 'filter', 'input'])

  # Filtering

  test('filter',
       files('test_filter.py'),
       args: filter_test_script_args,
       env: test_env,
       suite: ['tools'])

  test('grep',
       files('test_grep.py'),
       args: filter_test_script_args,
       env: test_env,
       suite: ['tools'])

  # RDF-driven test suite
  subdir('filter')

endif

# Run RDF-driven test suites using serd-pipe and serd-sort
if is_variable('serd_pipe') and is_variable('serd_sort')
  # RDF-driven test suites from the W3C
  subdir('NQuadsTests')
  subdir('NTriplesTests')
  subdir('TriGTests')
  subdir('TurtleTests')

  # Extra RDF-driven test suites for serd
  subdir('bad')
  subdir('canon')
  subdir('good')
  subdir('lax')
  subdir('pattern')
  subdir('pretty')
  subdir('terse')

endif
