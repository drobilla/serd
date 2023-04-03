# Copyright 2020-2023 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

run_suite = find_program('run_suite.py')

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
    test(
      'REUSE',
      reuse,
      args: ['--root', serd_src_root, 'lint'],
      suite: 'data',
    )
  endif
endif

###########
# Scripts #
###########

python_scripts = files(
  '../scripts/serd_bench.py',
  'run_suite.py',
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

simple_command_tests = {
  'serdi': {
    'bad': [
      ['-c'],
      ['-fi'],
      ['-i', 'turtle'],
      ['-i', 'unknown'],
      ['-i'],
      ['-o', 'unknown'],
      ['-o'],
      ['-p'],
      ['-r'],
      ['-z'],
    ],
    'good': [
      ['-h'],
      ['-s', '<urn:eg:s> a <urn:eg:T> .'],
      ['-v'],
    ],
  },
}

if is_variable('serdi')
  script_args = common_script_args + ['--serdi', serdi]
  serd_ttl = files('../serd.ttl')[0]
  bad_input_file = files('extra/bad/bad-base.ttl')

  test('serd.ttl', serdi, args: [serd_ttl], suite: 'data')

  # Command line options

  cmd_suite = ['serdi', 'options']

  foreach kind, cases : simple_command_tests['serdi']
    foreach args : cases
      test(
        ' '.join(args).underscorify(),
        serdi,
        args: args,
        should_fail: kind == 'bad',
        suite: cmd_suite,
      )
    endforeach
  endforeach

  test('none', serdi, should_fail: true, suite: cmd_suite)

  test(
    'quiet',
    files('test_quiet.py'),
    args: script_args + [bad_input_file],
    suite: cmd_suite,
  )

  # Inputs

  input_suite = ['serdi', 'input']

  bad_input_tests = {
    'string': ['-s', '<foo> a <Bar> .'],
    'no_such_file': ['no_such_file'],
    'remote': ['ftp://example.org/unsupported.ttl'],
  }

  foreach name, args : bad_input_tests
    test(name, serdi, args: args, should_fail: true, suite: input_suite)
  endforeach

  test('stdin', files('test_stdin.py'), args: script_args, suite: input_suite)

  # IO errors

  io_error_tests = {
    'read_dir_bulk': [serd_src_root],
    'read_dir_bytes': ['-e', serd_src_root],
    'read_dir_uri': ['file://@0@/'.format(serd_src_root)],
  }

  foreach name, args : io_error_tests
    test(name, serdi, args: args, should_fail: true, suite: 'io')
  endforeach

  test(
    'write_error',
    files('test_write_error.py'),
    args: script_args + [serd_ttl],
    suite: 'io',
  )
endif

###########################
# Data-Driven Test Suites #
###########################

ns_serdtest = 'http://drobilla.net/sw/serd/test/'
ns_w3 = 'http://www.w3.org/2013/'

test_suites = {
  'NQuads': [
    files('w3c/NQuadsTests/manifest.ttl'), ns_w3 + 'NQuadsTests/',
    '--', '-a', '-i', 'NQuads',
  ],
  'NTriples': [
    files('w3c/NTriplesTests/manifest.ttl'), ns_w3 + 'NTriplesTests/',
    '--', '-a', '-i', 'NTriples',
  ],
  'TriG': [
    files('w3c/TriGTests/manifest.ttl'), ns_w3 + 'TriGTests/',
    '--', '-a', '-f', '-i', 'TriG',
  ],
  'Turtle': [
    files('w3c/TurtleTests/manifest.ttl'), ns_w3 + 'TurtleTests/',
    '--', '-a', '-i', 'Turtle',
  ],
  'bad': [
    files('extra/bad/manifest.ttl'), ns_serdtest + 'bad/',
  ],
  'bulk': [
    files('extra/good/manifest.ttl'), ns_serdtest + 'good/',
    '--', '-b',
  ],
  'full': [
    files('extra/full/manifest.ttl'), ns_serdtest + 'full/',
    '--', '-f',
  ],
  'good': [
    files('extra/good/manifest.ttl'), ns_serdtest + 'good/',
  ],
  'lax.lax': [
    '--lax',
    files('extra/lax/manifest.ttl'), ns_serdtest + 'lax/',
    '--', '-l',
  ],
  'lax.strict': [
    files('extra/lax/manifest.ttl'), ns_serdtest + 'lax/',
  ],
  'perfect.forward': [
    files('extra/perfect/manifest.ttl'), ns_serdtest + 'perfect/',
  ],
  'perfect.reverse': [
    '--reverse',
    files('extra/perfect/manifest.ttl'), ns_serdtest + 'perfect/',
  ],
  'prefix.add': [
    '--reverse',
    files('extra/prefix/manifest.ttl'), ns_serdtest + 'prefix/',
    '--', '-p', 'test',
  ],
  'prefix.remove': [
    files('extra/prefix/manifest.ttl'), ns_serdtest + 'prefix/',
    '--', '-c', 'test',
  ],
  'pretty': [
    files('extra/pretty/manifest.ttl'), ns_serdtest + 'pretty/',
  ],
  'qualify': [
    files('extra/qualify/manifest.ttl'), ns_serdtest + 'qualify/',
    '--', '-i', 'turtle', # Just for coverage
  ],
  'root': [
    files('extra/root/manifest.ttl'), ns_serdtest + 'root/',
    '--', '-r', 'http://example.org/top/root/',
  ],
}

# Run every test suite with serdi
if is_variable('serdi')
  script_args = common_script_args + ['--serdi', serdi]

  foreach name, args : test_suites
    test(
      name, run_suite,
      args: script_args + args,
      suite: ['suite'],
      timeout: 240,
    )
  endforeach
endif
