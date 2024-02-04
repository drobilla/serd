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
    'PATH=@0@;@1@'.format('subprojects' / 'exess', 'subprojects' / 'zix'),
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
  'test_base.py',
  'test_contextual.py',
  'test_empty.py',
  'test_multifile.py',
  'test_quiet.py',
  'test_stdin.py',
  'test_write_error.py',
]

ttl_metadata_file_paths = [
  '../serd.ttl',
  'extra/abbreviate/manifest.ttl',
  'extra/bad/manifest.ttl',
  'extra/big/manifest.ttl',
  'extra/expanded/manifest.ttl',
  'extra/good/manifest.ttl',
  'extra/lax/manifest.ttl',
  'extra/pattern/manifest.ttl',
  'extra/perfect/manifest.ttl',
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

  # Check Turtle formatting with serd-pipe
  foreach ttl_file_path : ttl_metadata_file_paths
    test(
      ttl_file_path.underscorify(),
      check_formatting_py,
      args: [files(ttl_file_path), serd_pipe, '-O', 'turtle'],
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
  'overflow',
  'reader',
  'reader_writer',
  'sink',
  'string',
  'syntax',
  'tee',
  'terse_write',
  'uri',
  'world',
  'writer',
]

foreach unit : unit_tests
  test(
    unit,
    executable(
      'test_@0@'.format(unit),
      files('failing_allocator.c', 'test_@0@.c'.format(unit)),
      c_args: c_suppressions + platform_c_args,
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
  'pipe': {
    'bad': [
      ['-B', 'nonuriorpath'],
      ['-B'],
      ['-I', 'turt'],
      ['-I'],
      ['-Ix'],
      ['-O', '~unknown'],
      ['-O', 'ntripleses'],
      ['-O'],
      ['-Ox'],
      ['-R'],
      ['-b', '-1'],
      ['-b', '1024junk'],
      ['-b'],
      ['-fi'],
      ['-k', '9223372036854775807'],
      ['-k'],
      ['-qi'],
      ['-s', '<foo> a <Bar> .'],
      ['-s'],
      ['-z'],
    ],
    'good': [
      ['--help'],
      ['--version'],
      ['-V'],
      ['-h'],
      ['-k', '512', '-s', '<go:>a<go:> .'],
    ],
  },
}

if is_variable('serd_pipe')
  pipe_script_args = common_script_args + ['--tool', serd_pipe]
  serd_ttl = files('../serd.ttl')[0]
  bad_input_file = files('extra/bad/bad-base.ttl')
  text_input_file = files('extra/bad/README.md')
  small_ttl = files('w3c/turtle/turtle-syntax-number-01.ttl')

  test('serd_ttl', serd_pipe, args: [serd_ttl], env: test_env, suite: 'data')

  # Command line options

  cmd_suite = ['tools', 'pipe', 'options']

  foreach kind, cases : simple_command_tests['pipe']
    foreach args : cases
      test(
        ' '.join(args).substring(1).underscorify(),
        serd_pipe,
        args: args,
        env: test_env,
        should_fail: kind == 'bad',
        suite: cmd_suite,
      )
    endforeach
  endforeach

  # Base URI options

  test(
    'good_rebase',
    serd_pipe,
    args: ['-B', 'rebase', '-o', 'serd_rebased.ttl', serd_ttl],
    env: test_env,
    suite: cmd_suite,
  )
  test(
    'bad_rebase',
    serd_pipe,
    args: ['-B', 'rebase', serd_ttl],
    env: test_env,
    should_fail: true,
    suite: cmd_suite,
  )
  test(
    'base',
    files('test_base.py'),
    args: pipe_script_args,
    env: test_env,
    suite: cmd_suite,
  )
  test(
    'dir_base',
    serd_pipe,
    args: ['-B', './', serd_ttl],
    env: test_env,
    suite: cmd_suite,
  )

  # Log

  test(
    'quiet',
    files('test_quiet.py'),
    args: pipe_script_args + [bad_input_file],
    env: test_env,
    suite: cmd_suite,
  )

  # Inputs

  input_suite = ['tools', 'pipe', 'input']

  good_input_tests = {
    'unknown_extension': [files('trig_unknown_extension.n3')],
    'no_extension': [files('trig_no_extension')],
  }

  foreach name, args : good_input_tests
    test(
      name,
      serd_pipe,
      args: args,
      env: test_env,
      suite: input_suite,
    )
  endforeach

  bad_input_tests = {
    'string': ['-s', '<foo> a <Bar> .'],
    'no_such_file': ['no_such_file'],
    'remote': ['ftp://example.org/unsupported.ttl'],
    'text': [text_input_file],
  }

  foreach name, args : bad_input_tests
    test(
      name,
      serd_pipe,
      args: args,
      env: test_env,
      should_fail: true,
      suite: input_suite,
    )
  endforeach

  test(
    'stdin',
    files('test_stdin.py'),
    args: pipe_script_args,
    env: test_env,
    suite: input_suite,
  )
  test(
    'multifile',
    files('test_multifile.py'),
    args: pipe_script_args + [meson.current_source_dir() / 'multifile'],
    env: test_env,
    suite: input_suite,
  )

  # Output

  test(
    'empty',
    files('test_empty.py'),
    args: pipe_script_args + [serd_ttl],
    env: test_env,
    suite: 'output',
  )

  # IO errors

  io_error_tests = {
    'read_dir_bulk': [serd_src_root],
    'read_dir_bytes': ['-b', '1', serd_src_root],
    'read_dir_uri': ['file://@0@/'.format(serd_src_root)],
    'write_dir': ['-o', meson.current_build_dir(), serd_ttl],
  }

  foreach name, args : io_error_tests
    test(
      name,
      serd_pipe,
      args: args,
      env: test_env,
      should_fail: true,
      suite: 'io',
    )
  endforeach

  test(
    'write_error',
    files('test_write_error.py'),
    args: pipe_script_args + [serd_ttl],
    env: test_env,
    suite: 'io',
  )
  test(
    'deferred_write_error',
    files('test_write_error.py'),
    args: pipe_script_args + [small_ttl, '--', '-b', '1024'],
    env: test_env,
    suite: 'io',
  )
  test(
    'missing_output',
    serd_pipe,
    args: ['-o', '/does/not/exist.ttl', serd_ttl],
    env: test_env,
    should_fail: true,
    suite: 'io',
  )

  if host_machine.system() == 'linux'
    test(
      'unreadable',
      serd_pipe,
      args: ['/sys/bus/pci/rescan'],
      env: test_env,
      should_fail: true,
      suite: 'io',
    )
  endif

  # Write options

  test(
    'contextual',
    files('test_contextual.py'),
    args: pipe_script_args + files('../serd.ttl'),
    env: test_env,
    suite: ['tools', 'pipe', 'output'],
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
    ['-I', 'NQuads'],
    ['-O', 'ascii'],
    ['-k', '8192'],
  ],
  'ntriples': [
    files('w3c/ntriples/manifest.ttl'),
    ns_w3 + 'NTriplesTests/',
    '--',
    ['-I', 'NTriples'],
    ['-O', 'ascii'],
    ['-k', '1024'],
  ],
  'trig': [
    files('w3c/trig/manifest.ttl'),
    ns_w3 + 'TriGTests/',
    '--',
    ['-I', 'TriG'],
    ['-O', 'ascii'],
    ['-k', '1024'],
  ],
  'turtle': [
    files('w3c/turtle/manifest.ttl'),
    ns_w3 + 'TurtleTests/',
    '--',
    ['-I', 'Turtle'],
    ['-O', 'ascii'],
    ['-k', '1024'],
  ],

  'abbreviate': [
    files('extra/abbreviate/manifest.ttl'),
    ns_serdtest + 'abbreviate/',
    '--',
    ['-k', '1024'],
  ],
  'bad': [
    files('extra/bad/manifest.ttl'),
    ns_serdtest + 'bad/',
    '--',
    ['-k', '1024'],
  ],
  'bad_turtle': [
    files('extra/bad/manifest.ttl'),
    ns_serdtest + 'bad/',
    '--',
    ['-O', 'turtle'],
    ['-k', '1024'],
  ],
  'big': [
    files('extra/big/manifest.ttl'),
    ns_serdtest + 'big/',
    ['-k', '1024'],
  ],
  'bulk': [
    files('extra/good/manifest.ttl'),
    ns_serdtest + 'good/',
    '--',
    ['-O', 'ascii'],
    ['-b', '1'],
    ['-k', '16384'],
  ],
  'expanded': [
    files('extra/expanded/manifest.ttl'),
    ns_serdtest + 'expanded/',
    '--',
    ['-O', 'expanded'],
    ['-k', '1024'],
  ],
  'fast': [
    files('extra/perfect/manifest.ttl'),
    ns_serdtest + 'perfect/',
    '--',
    ['-I', 'global'],
    ['-I', 'prefixed'],
    ['-I', 'relative'],
    ['-O', 'verbatim'],
    ['-k', '16384'],
  ],
  'generated': [
    files('extra/generated/manifest.ttl'),
    ns_serdtest + 'generated/',
    '--',
    ['-I', 'generated'],
  ],
  'good': [
    files('extra/good/manifest.ttl'),
    ns_serdtest + 'good/',
    '--',
    ['-O', 'ascii'],
    ['-k', '16384'],
  ],
  'lax_lax': [
    '--lax',
    files('extra/lax/manifest.ttl'),
    ns_serdtest + 'lax/',
    '--',
    ['-I', 'lax'],
    ['-k', '1024'],
  ],
  'lax_strict': [
    files('extra/lax/manifest.ttl'),
    ns_serdtest + 'lax/',
    '--',
    ['-k', '1024'],
  ],
  'longhand_forward': [
    files('extra/longhand/manifest.ttl'),
    ns_serdtest + 'longhand/',
    '--',
    ['-O', 'longhand'],
  ],
  'longhand_reverse': [
    '--reverse',
    files('extra/longhand/manifest.ttl'),
    ns_serdtest + 'longhand/',
  ],
  'pattern': [
    files('extra/pattern/manifest.ttl'),
    ns_serdtest + 'pattern/',
    '--',
    ['-I', 'variables'],
  ],
  'perfect_forward': [
    files('extra/perfect/manifest.ttl'),
    ns_serdtest + 'perfect/',
    '--',
    ['-k', '1024'],
  ],
  'perfect_reverse': [
    '--reverse',
    files('extra/perfect/manifest.ttl'),
    ns_serdtest + 'perfect/',
    '--',
    ['-k', '1024'],
  ],
  'prefixed': [
    files('extra/prefixed/manifest.ttl'),
    ns_serdtest + 'prefixed/',
    '--',
    ['-I', 'prefixed'],
  ],
  'prefixed_bad': [
    files('extra/bad/manifest.ttl'),
    ns_serdtest + 'bad/',
    '--',
    ['-I', 'prefixed'],
  ],
  'prefixed_pretty': [
    files('extra/pretty/manifest.ttl'),
    ns_serdtest + 'pretty/',
    '--',
    ['-I', 'prefixed'],
  ],
  'prefixed_perfect': [
    files('extra/perfect/manifest.ttl'),
    ns_serdtest + 'perfect/',
    '--',
    ['-I', 'prefixed'],
  ],
  'pretty': [
    files('extra/pretty/manifest.ttl'),
    ns_serdtest + 'pretty/',
    '--',
    ['-k', '1024'],
  ],
  'qualify': [
    files('extra/qualify/manifest.ttl'),
    ns_serdtest + 'qualify/',
    '--',
    ['-I', 'turtle'], # Just for coverage
    ['-k', '1024'],
  ],
  'root': [
    files('extra/root/manifest.ttl'),
    ns_serdtest + 'root/',
    '--',
    ['-R', 'http://example.org/top/root/'],
    ['-k', '1024'],
  ],
  'terse': [
    files('extra/terse/manifest.ttl'),
    ns_serdtest + 'terse/',
    '--',
    ['-O', 'terse'],
  ],
}

# Run every test suite with serd-pipe
if is_variable('serd_pipe')
  script_args = common_script_args + ['--tool', serd_pipe]

  foreach name, args : test_suites
    test(
      name,
      run_suite,
      args: pipe_script_args + args,
      env: test_env,
      suite: ['suite'],
      timeout: 240,
    )
  endforeach
endif
