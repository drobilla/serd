# Copyright 2020-2023 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

run_suite = find_program('run_suite.py')
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

###########
# Scripts #
###########

python_scripts = files(
  '../scripts/serd_bench.py',
  'run_suite.py',
  'run_test_suite.py',
  'serd_test_util/__init__.py',
  'test_quiet.py',
  'test_stdin.py',
  'test_write_error.py',
)

if get_option('strict')
  # Check script formatting
  black = find_program('black', required: false)
  if black.found()
    black_opts = ['--check', '-q', '-l', '79']
    foreach script : python_scripts
      name = '@0@'.format(script)
      test(name, black, args: black_opts + [script], suite: 'scripts')
    endforeach
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
  'env',
  'free_null',
  'node',
  'reader_writer',
  'string',
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
      dependencies: serd_dep,
    ),
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

if not get_option('tools').disabled()
  script_args = common_script_args + ['--serdi', serdi]
  serd_ttl = files('../serd.ttl')[0]

  test('serd.ttl', serdi, args: [serd_ttl], suite: 'data')

  # Command line options

  good_args = [
    ['-v'],
    ['-h'],
    ['-s', '<urn:eg:s> a <urn:eg:T> .'],
  ]

  foreach args : good_args
    test(args[0], serdi, args: args, suite: ['serdi', 'options'])
  endforeach

  bad_args = [
    ['/no/such/file'],
    ['ftp://unsupported.org'],
    ['-c'],
    ['-i', 'unknown'],
    ['-i', 'turtle'],
    ['-i'],
    ['-fi'],
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
         should_fail: true,
         suite: ['serdi', 'options'])
  endforeach

  test('none', serdi, should_fail: true, suite: ['serdi', 'options'])

  test('quiet', files('test_quiet.py'),
       args: script_args + files('bad/bad-base.ttl'),
       suite: ['serdi', 'options'])

  # Inputs

  test('stdin', files('test_stdin.py'),
       args: script_args,
       suite: ['serdi', 'input'])

  test('string', serdi,
       args: ['-s', '<foo> a <Bar> .'],
       should_fail: true,
       suite: ['serdi', 'input'])

  test('missing', serdi,
       args: ['-i', 'turtle'],
       should_fail: true,
       suite: ['serdi', 'input'])

  test('no_such_file', serdi,
       args: ['no_such_file'],
       should_fail: true,
       suite: ['serdi', 'input'])

  test('remote', serdi,
       args: ['ftp://example.org/unsupported.ttl'],
       should_fail: true,
       suite: ['serdi', 'input'])

  # IO errors

  test('read_dir', serdi,
       args: ['-e', 'file://@0@/'.format(serd_src_root)],
       should_fail: true,
       suite: 'io_errors')

  test('bulk_read_dir', serdi,
       args: ['file://@0@/'.format(serd_src_root)],
       should_fail: true,
       suite: 'io_errors')

  test('write_error', files('test_write_error.py'),
       args: script_args + [serd_ttl],
       suite: 'io_errors')
endif

###########################
# Data-Driven Test Suites #
###########################

ns_serdtest = 'http://drobilla.net/sw/serd/test/'
ns_w3 = 'http://www.w3.org/2013/'

test_suites = {
  'lax.lax': [
    '--lax',
    files('lax/manifest.ttl'), ns_serdtest + 'lax/',
    '--', '-l',
  ],
  'lax.strict': [
    files('lax/manifest.ttl'), ns_serdtest + 'lax/',
  ],
  'prefix.add': [
    '--reverse',
    files('prefix/manifest.ttl'), ns_serdtest + 'prefix/',
    '--', '-p', 'test',
  ],
  'prefix.remove': [
    files('prefix/manifest.ttl'), ns_serdtest + 'prefix/',
    '--', '-c', 'test',
  ],
  'pretty': [
    files('pretty/manifest.ttl'), ns_serdtest + 'pretty/',
  ],
  'qualify': [
    files('qualify/manifest.ttl'), ns_serdtest + 'qualify/',
  ],
  'root': [
    files('root/manifest.ttl'), ns_serdtest + 'root/',
    '--', '-r', 'http://example.org/top/root/',
  ],
}

if not get_option('tools').disabled()
  script_args = common_script_args + ['--serdi', serdi]

  foreach name, args : test_suites
    test(
      name, run_suite,
      args: script_args + args,
      suite: ['rdf'],
      timeout: 240,
    )
  endforeach

  ## Serd-specific test suites

  serd_suites = ['good', 'bad']

  ### Run all suites with no special arguments
  foreach name : serd_suites
    manifest = files(name / 'manifest.ttl')
    base_uri = ns_serdtest + name + '/'
    test(name, run_test_suite,
         args: script_args + [manifest, base_uri],
         suite: ['rdf', 'serd'],
         timeout: 240)
  endforeach

  test('good.bulk', run_test_suite,
       args: script_args + [
         files('good/manifest.ttl'),
         ns_serdtest + 'good/',
         '--',
         '-b'
       ],
       is_parallel: false,
       suite: ['rdf', 'serd'],
       timeout: 240)

  ## Standard W3C test suites

  w3c_suites = ['Turtle', 'NTriples', 'NQuads', 'TriG']

  foreach syntax : w3c_suites
    manifest = files(syntax + 'Tests' / 'manifest.ttl')
    base_uri = ns_w3 + syntax + 'Tests/'

    args = [manifest, base_uri]
    if syntax == 'TriG'
      args += ['--', '-a']
    endif

    test(syntax, run_test_suite,
         args: script_args + args,
         suite: ['rdf', 'w3c'],
         timeout: 240)
  endforeach
endif
