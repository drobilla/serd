autoship = find_program('autoship', required: false)

run_test_suite = find_program('run_test_suite.py')

wrapper = meson.get_cross_property('exe_wrapper', '')

unit_tests = [
  'byte_sink',
  'byte_source',
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
  'read_chunk',
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
                                    'subprojects' / 'rerex')]
endif

foreach unit : unit_tests
  test(unit,
       executable('test_@0@'.format(unit),
                  'test_@0@.c'.format(unit),
                  c_args: c_warnings + platform_args + prog_args,
                  dependencies: serd_dep),
       env: test_env,
       suite: 'unit')
endforeach

if autoship.found()
  test('autoship', autoship, args: ['test', serd_src_root], suite: 'data')
endif

if is_variable('serdi')

  if wrapper != ''
    script_args = ['--wrapper', wrapper, '--serdi', serdi.full_path()]
  else
    script_args = ['--serdi', serdi.full_path()]
  endif

  serd_ttl = files('../serd.ttl')[0]

  test('serd.ttl', serdi, args: [serd_ttl], env: test_env, suite: 'data')

  # Command line options

  good_args = [
    ['-v'],
    ['-h'],
    ['-k', '512', '-s', '<urn:eg:s> a <urn:eg:T> .'],
  ]

  foreach args : good_args
    test(args[0], serdi, args: args, env: test_env, suite: ['serdi', 'options'])
  endforeach

  bad_args = [
    ['/no/such/file'],
    ['ftp://unsupported.org'],
    ['-F', '', '-G', ''],
    ['-F'],
    ['-F', '?s ?p ?o . ?q ?r ?s .', '-s', ''],
    ['-F', '?s ?p ?o .\n?q ?r ?s .\n', '-s', ''],
    ['-F', 'bad_pattern', '-s', ''],
    ['-G'],
    ['-G', '?s ?p ?o . ?q ?r ?s .', '-s', ''],
    ['-G', 'bad_pattern', '-s', ''],
    ['-I'],
    ['-b'],
    ['-b', '-1'],
    ['-b', '9223372036854775807'],
    ['-b', '1024junk'],
    ['-c'],
    ['-i', 'unknown'],
    ['-i', 'turtle'],
    ['-i'],
    ['-fi'],
    ['-k'],
    ['-k', '-1'],
    ['-k', '9223372036854775807'],
    ['-k', '1024junk'],
    ['-o', 'unknown'],
    ['-o'],
    ['-p'],
    ['-r'],
    ['-s'],
    ['-w'],
    ['-z'],
    ['-s', '<foo> a <Bar> .'],
  ]

  foreach args : bad_args
    name = ' '.join(args).underscorify()
    test(name, serdi,
         args: args,
         env: test_env,
         should_fail: true,
         suite: ['serdi', 'options'])
  endforeach

  test('ansi_clicolor_force',
       serdi,
       args: files('bad/bad-lang.ttl'),
       env: test_env + ['CLICOLOR_FORCE=1'],
       should_fail: true)

  test('ansi_clicolor_off',
       serdi,
       args: files('bad/bad-lang.ttl'),
       env: test_env + ['CLICOLOR=0'],
       should_fail: true)

  test('ansi_no_color',
       serdi,
       args: files('bad/bad-lang.ttl'),
       env: test_env + ['NO_COLOR=1'],
       should_fail: true)

  test('none',
       serdi,
       env: test_env,
       should_fail: true,
       suite: ['serdi', 'options'])

  test('quiet', files('test_quiet.py'),
       args: script_args + files('bad/bad-base.ttl'),
       env: test_env,
       suite: ['serdi', 'options'])

  test('filter', files('test_filter.py'),
       args: script_args,
       env: test_env,
       suite: ['serdi', 'options'])

  test('grep', files('test_grep.py'),
       args: script_args,
       env: test_env,
       suite: ['serdi', 'options'])

  # Inputs

  test('stdin', files('test_stdin.py'),
       args: script_args,
       env: test_env,
       suite: ['serdi', 'input'])

  test('multiple', files('test_multifile.py'),
       args: script_args + [meson.current_source_dir() / 'multifile'],
       env: test_env,
       suite: ['serdi', 'input'])

  test('string', serdi,
       args: ['-s', '<foo> a <Bar> .'],
       env: test_env,
       should_fail: true,
       suite: ['serdi', 'input'])

  test('missing', serdi,
       args: ['-i', 'turtle'],
       env: test_env,
       should_fail: true,
       suite: ['serdi', 'input'])

  test('no_such_file', serdi,
       args: ['no_such_file'],
       env: test_env,
       should_fail: true,
       suite: ['serdi', 'input'])

  test('remote', serdi,
       args: ['ftp://example.org/unsupported.ttl'],
       env: test_env,
       should_fail: true,
       suite: ['serdi', 'input'])

  # Output

  test('empty', files('test_empty.py'),
       args: script_args + [serd_ttl],
       env: test_env,
       suite: 'output')

  # FIXME: Old base URI argument?

  # IO errors

  test('read_dir', serdi,
       args: [meson.source_root()],
       env: test_env,
       should_fail: true,
       suite: 'io_errors')

  if host_machine.system() == 'linux'
    test('unreadable', serdi,
         args: ['/sys/bus/pci/rescan'],
         env: test_env,
         should_fail: true,
         suite: 'io_errors')
  endif

  test('write_error', files('test_write_error.py'),
       args: script_args + [serd_ttl],
       env: test_env,
       suite: 'io_errors')

  test('write_bad_file', serdi,
       args: ['-w', '/does/not/exist.ttl', meson.source_root() / 'serd.ttl'],
       env: test_env,
       should_fail: true,
       suite: 'io_errors')

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
  subdir('terse')

endif
