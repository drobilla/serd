# Copyright 2020-2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

run_test_suite = files('run_test_suite.py')

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
  'env',
  'free_null',
  'node',
  'overflow',
  'reader_writer',
  'sink',
  'statement',
  'string',
  'terse_write',
  'uri',
  'writer',
]

test_env = []
if build_machine.system() == 'windows' and host_machine.system() == 'windows'
  # For Windows, we need to add to PATH so that DLLs are found
  test_env = ['PATH=@0@;@1@'.format('subprojects' / 'exess')]
endif

foreach unit : unit_tests
  test(
    unit,
    executable(
      'test_@0@'.format(unit),
      files('test_@0@.c'.format(unit)),
      c_args: c_suppressions + platform_args,
      dependencies: serd_dep,
    ),
    env: test_env,
    suite: 'unit',
  )
endforeach

################
# System Tests #
################

if not get_option('tools').disabled()
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

  test('none',
       serdi,
       env: test_env,
       should_fail: true,
       suite: ['serdi', 'options'])

  test('quiet', files('test_quiet.py'),
       args: script_args + files('bad/bad-base.ttl'),
       env: test_env,
       suite: ['serdi', 'options'])

  # Inputs

  test('stdin', files('test_stdin.py'),
       args: script_args,
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
       args: ['-e', 'file://@0@/'.format(serd_src_root)],
       env: test_env,
       should_fail: true,
       suite: 'io_errors')

  test('bulk_read_dir', serdi,
       args: ['file://@0@/'.format(serd_src_root)],
       env: test_env,
       should_fail: true,
       suite: 'io_errors')

  test('write_error', files('test_write_error.py'),
       args: script_args + [serd_ttl],
       env: test_env,
       suite: 'io_errors')

  # RDF test suites

  ## Serd-specific test suites

  serd_suites = ['good', 'bad']
  serd_base = 'http://drobilla.net/sw/serd/test/'

  ### Run basic suites with no special arguments
  foreach name : serd_suites
    manifest = files(name / 'manifest.ttl')
    base_uri = serd_base + name + '/'
    test(name, run_test_suite,
         args: script_args + [manifest, base_uri],
         env: test_env,
         suite: ['rdf', 'serd'],
         timeout: 240)
  endforeach

  ### The terse suite needs to be run with -t
  test('terse', run_test_suite,
       args: script_args + ['--osyntax', 'turtle',
                            files('terse/manifest.ttl'),
                            serd_base + 'terse/',
                            '--', '-t'],
       env: test_env,
       suite: ['rdf', 'serd'],
       timeout: 240)

  ### The lax suite is special because it is run twice...
  lax_manifest = files('lax/manifest.ttl')
  lax_base_uri = serd_base + name + '/'

  ### ... once with strict parsing to test the hard errors
  test('lax.strict', run_test_suite,
       args: script_args + [lax_manifest, lax_base_uri],
       env: test_env,
       is_parallel: false,
       suite: ['rdf', 'serd'],
       timeout: 240)

  ### ... and once with lax parsing to tolerate them
  test('lax.lax', run_test_suite,
       args: script_args + [lax_manifest, lax_base_uri, '--', '-l'],
       env: test_env,
       is_parallel: false,
       suite: ['rdf', 'serd'],
       timeout: 240)

  ## Standard W3C test suites

  w3c_suites = ['Turtle', 'NTriples', 'NQuads', 'TriG']
  w3c_base = 'http://www.w3.org/2013/'

  foreach syntax : w3c_suites
    manifest = files(syntax + 'Tests' / 'manifest.ttl')
    base_uri = w3c_base + syntax + 'Tests/'

    args = ['--syntax', syntax, manifest, base_uri]

    test(syntax, run_test_suite,
         args: script_args + args,
         env: test_env,
         suite: ['rdf', 'w3c'],
         timeout: 240)
  endforeach
endif
