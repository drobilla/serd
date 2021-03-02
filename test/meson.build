# Copyright 2020-2023 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

run_suite = find_program('run_suite.py')
wrapper = meson.get_external_property('exe_wrapper', '')

#######################
# Runtime Environment #
#######################

test_env = []
if build_machine.system() == 'windows' and host_machine.system() == 'windows'
  # For Windows, we need to add to PATH so that DLLs are found
  test_env = [
    'PATH=@0@'.format('subprojects' / 'zix'),
  ]
endif

########################
# Scripts and Metadata #
########################

plot_script_paths = [
  '../scripts/serd_bench.py',
]

simple_script_paths = [
  '../scripts/check_formatting.py',
  'serd_test_util/__init__.py',
  'run_suite.py',
  'test_empty.py',
  'test_quiet.py',
  'test_stdin.py',
  'test_write_error.py',
]

ttl_metadata_file_paths = [
  '../serd.ttl',
  'extra/abbreviate/manifest.ttl',
  'extra/bad/manifest.ttl',
  'extra/big/manifest.ttl',
  'extra/full/manifest.ttl',
  'extra/good/manifest.ttl',
  'extra/lax/manifest.ttl',
  'extra/perfect/manifest.ttl',
  'extra/prefix/manifest.ttl',
  'extra/pretty/manifest.ttl',
  'extra/qualify/manifest.ttl',
  'extra/root/manifest.ttl',
  'extra/terse/manifest.ttl',
]

plot_scripts = files(plot_script_paths)
simple_scripts = files(simple_script_paths)
python_script_paths = simple_script_paths + plot_script_paths
python_scripts = plot_scripts + simple_scripts

if get_option('lint')
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
    test(
      'REUSE',
      reuse,
      args: ['--root', serd_src_root, 'lint'],
      suite: 'data',
    )
  endif

  # Check script formatting
  black = find_program('black', required: false)
  if black.found()
    black_opts = ['--check', '-q', '-l', '79']
    foreach script_path : python_script_paths
      script = files(script_path)
      name = script_path.underscorify()
      test(name, black, args: black_opts + [script], suite: 'scripts')
    endforeach
  endif

  # Check scripts for errors with flake8
  flake8 = find_program('flake8', required: false)
  if flake8.found()
    test('flake8', flake8, args: python_scripts, suite: 'scripts')
  endif

  # Check scripts for errors with pylint
  pylint = find_program('pylint', required: false)
  if pylint.found()
    pymod = import('python')
    plot_py = pymod.find_installation(
      'python3',
      modules: ['matplotlib'],
      required: false,
    )

    pylint_args = ['--disable', 'bad-option-value'] + simple_scripts
    if plot_py.found()
      pylint_args += plot_scripts
    endif

    test('pylint', pylint, args: pylint_args, suite: 'scripts')
  endif

  # Check Turtle formatting with serdi
  foreach ttl_file_path : ttl_metadata_file_paths
    test(
      ttl_file_path.underscorify(),
      check_formatting_py,
      args: [files(ttl_file_path), serdi, '-o', 'turtle'],
      env: test_env,
      suite: 'data',
    )
  endforeach
endif

###################
# Header Warnings #
###################

subdir('headers')

##############
# Unit Tests #
##############

unit_tests = [
  'env',
  'free_null',
  'node',
  'reader',
  'reader_writer',
  'sink',
  'string',
  'syntax',
  'terse_write',
  'uri',
  'writer',
]

foreach unit : unit_tests
  test(
    unit,
    executable(
      'test_@0@'.format(unit),
      files('test_@0@.c'.format(unit)),
      c_args: c_suppressions,
      dependencies: [serd_dep, zix_dep],
    ),
    env: test_env,
    suite: 'unit',
  )
endforeach

################
# System Tests #
################

common_script_args = []
if wrapper != ''
  common_script_args += ['--wrapper', wrapper]
endif

simple_command_tests = {
  'serdi': {
    'bad': [
      ['-c'],
      ['-cx'],
      ['-fi'],
      ['-i', 'turtle'],
      ['-i', 'turt'],
      ['-i'],
      ['-ix'],
      ['-o', '~unknown'],
      ['-o', 'ntripleses'],
      ['-o'],
      ['-ox'],
      ['-p'],
      ['-px'],
      ['-r'],
      ['-rx'],
      ['-z'],
    ],
    'good': [
      ['--help'],
      ['--version'],
      ['-h'],
      ['-s', '<go:>a<go:> .'],
      ['-v'],
    ],
  },
}

if is_variable('serdi')
  script_args = common_script_args + ['--serdi', serdi]
  serd_ttl = files('../serd.ttl')[0]
  bad_input_file = files('extra/bad/bad-base.ttl')
  text_input_file = files('extra/bad/README.md')

  test('serd_ttl', serdi, args: [serd_ttl], env: test_env, suite: 'data')

  # Command line options

  cmd_suite = ['serdi', 'options']

  foreach kind, cases : simple_command_tests['serdi']
    foreach args : cases
      test(
        ' '.join(args).substring(1).underscorify(),
        serdi,
        args: args,
        env: test_env,
        should_fail: kind == 'bad',
        suite: cmd_suite,
      )
    endforeach
  endforeach

  test('none', serdi, env: test_env, should_fail: true, suite: cmd_suite)

  test(
    'quiet',
    files('test_quiet.py'),
    args: script_args + [bad_input_file],
    env: test_env,
    suite: cmd_suite,
  )

  # Inputs

  input_suite = ['serdi', 'input']

  bad_input_tests = {
    'string': ['-s', '<foo> a <Bar> .'],
    'no_such_file': ['no_such_file'],
    'remote': ['ftp://example.org/unsupported.ttl'],
    'text': [text_input_file],
  }

  foreach name, args : bad_input_tests
    test(
      name,
      serdi,
      args: args,
      env: test_env,
      should_fail: true,
      suite: input_suite,
    )
  endforeach

  test(
    'stdin',
    files('test_stdin.py'),
    args: script_args,
    env: test_env,
    suite: input_suite,
  )

  # Output

  test(
    'empty',
    files('test_empty.py'),
    args: script_args + [serd_ttl],
    env: test_env,
    suite: 'output',
  )

  # IO errors

  io_error_tests = {
    'read_dir_bulk': [serd_src_root],
    'read_dir_bytes': ['-e', serd_src_root],
    'read_dir_uri': ['file://@0@/'.format(serd_src_root)],
  }

  foreach name, args : io_error_tests
    test(name, serdi, args: args, env: test_env, should_fail: true, suite: 'io')
  endforeach

  test(
    'write_error',
    files('test_write_error.py'),
    args: script_args + [serd_ttl],
    env: test_env,
    suite: 'io',
  )
endif

###########################
# Data-Driven Test Suites #
###########################

ns_serdtest = 'http://drobilla.net/sw/serd/test/'
ns_w3 = 'http://www.w3.org/2013/'

test_suites = {
  'nquads': [
    files('w3c/nquads/manifest.ttl'),
    ns_w3 + 'NQuadsTests/',
    '--',
    '-a',
    ['-i', 'NQuads'],
  ],
  'ntriples': [
    files('w3c/ntriples/manifest.ttl'),
    ns_w3 + 'NTriplesTests/',
    '--',
    '-a',
    ['-i', 'NTriples'],
  ],
  'trig': [
    files('w3c/trig/manifest.ttl'),
    ns_w3 + 'TriGTests/',
    '--',
    '-a',
    ['-i', 'TriG'],
  ],
  'turtle': [
    files('w3c/turtle/manifest.ttl'),
    ns_w3 + 'TurtleTests/',
    '--',
    '-a',
    ['-i', 'Turtle'],
  ],

  'abbreviate': [
    files('extra/abbreviate/manifest.ttl'),
    ns_serdtest + 'abbreviate/',
  ],
  'bad': [
    files('extra/bad/manifest.ttl'),
    ns_serdtest + 'bad/',
  ],
  'bad_turtle': [
    files('extra/bad/manifest.ttl'),
    ns_serdtest + 'bad/',
    '--',
    ['-o', 'turtle'],
  ],
  'big': [
    files('extra/big/manifest.ttl'),
    ns_serdtest + 'big/',
  ],
  'bulk': [
    files('extra/good/manifest.ttl'),
    ns_serdtest + 'good/',
    '--',
    '-a',
    '-b',
  ],
  'fast': [
    files('extra/perfect/manifest.ttl'),
    ns_serdtest + 'perfect/',
    '--',
    '-f',
  ],
  'full': [
    files('extra/full/manifest.ttl'),
    ns_serdtest + 'full/',
    '--',
    '-f',
  ],
  'good': [
    files('extra/good/manifest.ttl'),
    ns_serdtest + 'good/',
    '--',
    '-a',
  ],
  'lax_lax': [
    '--lax',
    files('extra/lax/manifest.ttl'),
    ns_serdtest + 'lax/',
    '--',
    '-l',
  ],
  'lax_strict': [
    files('extra/lax/manifest.ttl'),
    ns_serdtest + 'lax/',
  ],
  'perfect_forward': [
    files('extra/perfect/manifest.ttl'),
    ns_serdtest + 'perfect/',
  ],
  'perfect_reverse': [
    '--reverse',
    files('extra/perfect/manifest.ttl'),
    ns_serdtest + 'perfect/',
  ],
  'prefix_add': [
    '--reverse',
    files('extra/prefix/manifest.ttl'),
    ns_serdtest + 'prefix/',
    '--',
    ['-p', 'test'],
  ],
  'prefix_remove': [
    files('extra/prefix/manifest.ttl'),
    ns_serdtest + 'prefix/',
    '--',
    ['-c', 'test'],
  ],
  'pretty': [
    files('extra/pretty/manifest.ttl'),
    ns_serdtest + 'pretty/',
  ],
  'qualify': [
    files('extra/qualify/manifest.ttl'),
    ns_serdtest + 'qualify/',
    '--',
    ['-i', 'turtle'], # Just for coverage
  ],
  'root': [
    files('extra/root/manifest.ttl'),
    ns_serdtest + 'root/',
    '--',
    ['-r', 'http://example.org/top/root/'],
  ],
  'terse': [
    files('extra/terse/manifest.ttl'),
    ns_serdtest + 'terse/',
    '--',
    '-t',
  ],
}

# Run every test suite with serdi
if is_variable('serdi')
  script_args = common_script_args + ['--serdi', serdi]

  foreach name, args : test_suites
    test(
      name,
      run_suite,
      args: script_args + args,
      env: test_env,
      suite: ['suite'],
      timeout: 240,
    )
  endforeach
endif
