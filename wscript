#!/usr/bin/env python
import Logs
import Options
import autowaf
import filecmp
import glob
import os
import subprocess

# Version of this package (even if built as a child)
SERD_VERSION = '0.1.0'

# Library version (UNIX style major, minor, micro)
# major increment <=> incompatible changes
# minor increment <=> compatible changes (additions)
# micro increment <=> no interface changes
# Version history:
#   0.0.1 = 1,0,0
SERD_LIB_VERSION = '1.0.0'

# Variables for 'waf dist'
APPNAME = 'serd'
VERSION = SERD_VERSION

# Mandatory variables
top = '.'
out = 'build'

def options(opt):
	autowaf.set_options(opt)
	opt.add_option('--no-utils', action='store_true', default=False, dest='no_utils',
			help="Do not build command line utilities")
	opt.add_option('--test', action='store_true', default=False, dest='build_tests',
			help="Build unit tests")

def configure(conf):
	conf.line_just = max(conf.line_just, 59)
	autowaf.configure(conf)
	autowaf.display_header('Serd Configuration')

	conf.check_tool('compiler_cc')
	conf.env.append_value('CFLAGS', '-std=c99')

	autowaf.define(conf, 'SERD_VERSION', SERD_VERSION)
	conf.env['BUILD_TESTS'] = Options.options.build_tests
	conf.env['BUILD_UTILS'] = not Options.options.no_utils
	conf.write_config_header('serd-config.h', remove=False)
	print

def build(bld):
	# C Headers
	bld.install_files('${INCLUDEDIR}/serd', bld.path.ant_glob('serd/*.h'))

	# Pkgconfig file
	autowaf.build_pc(bld, 'SERD', SERD_VERSION, ['REDLAND'])

	lib_source = '''
		src/namespaces.c
		src/reader.c
		src/string.c
		src/uri.c
		src/write.c
	'''

	# Library
	obj = bld(features = 'c cshlib')
	obj.export_includes = ['.']
	obj.source          = lib_source
	obj.includes        = ['.', './src']
	obj.name            = 'libserd'
	obj.target          = 'serd'
	obj.vnum            = SERD_LIB_VERSION
	obj.install_path    = '${LIBDIR}'
	obj.cflags          = [ '-fvisibility=hidden', '-DSERD_SHARED', '-DSERD_INTERNAL' ]

	if bld.env['BUILD_TESTS']:
		# Static library (for unit test code coverage)
		obj = bld(features = 'c cstlib')
		obj.source       = lib_source
		obj.includes     = ['.', './src']
		obj.name         = 'libserd_static'
		obj.target       = 'serd_static'
		obj.install_path = ''
		obj.cflags       = [ '-fprofile-arcs',  '-ftest-coverage' ]

		# Unit test program
		obj = bld(features = 'c cprogram')
		obj.source       = 'src/serdi.c'
		obj.includes     = ['.', './src']
		obj.use          = 'libserd_static'
		obj.linkflags    = '-lgcov'
		obj.target       = 'serdi_static'
		obj.install_path = ''
		obj.cflags       = [ '-fprofile-arcs',  '-ftest-coverage' ]

	# Utilities
	if bld.env['BUILD_UTILS']:
		obj = bld(features = 'c cprogram')
		obj.source       = 'src/serdi.c'
		obj.includes     = ['.', './src']
		obj.use          = 'libserd'
		obj.target       = 'serdi'
		obj.install_path = '${BINDIR}'

	# Documentation
	autowaf.build_dox(bld, 'SERD', SERD_VERSION, top, out)

	bld.add_post_fun(autowaf.run_ldconfig)

def test(ctx):
	try:
		os.makedirs('build/tests')
	except:
		pass

	for i in glob.glob('build/tests/*.*'):
		os.remove(i)

	commands = []
	good_tests = glob.glob('tests/test-*.ttl')
	good_tests.sort()
	for test in good_tests:
		base_uri = 'http://www.w3.org/2001/sw/DataAccess/df1/' + test
		commands = commands + [ './serdi_static ../%s \'%s\' > %s.out' % (test, base_uri, test) ]

	autowaf.run_tests(ctx, APPNAME, commands, 0)

	Logs.pprint('BOLD', '\nVerifying output')
	for test in good_tests:
		out_filename = 'build/' + test + '.out'
		if not os.access(out_filename, os.F_OK):
			Logs.pprint('RED', 'FAIL: %s output is missing' % test)
		elif filecmp.cmp(test.replace('.ttl', '.out'),
						 'build/' + test + '.out',
						 False) != 1:
			Logs.pprint('RED', 'FAIL: %s output is incorrect' % test)
		else:
			Logs.pprint('GREEN', 'PASS: %s output is correct' % test)
	print
	
#	commands = []
#	bad_tests = glob.glob('tests/bad-*.ttl')
#	bad_tests.sort()
#	for test in bad_tests:
#	    commands = commands + [ './serdi_static ../%s \'http://www.w3.org/2001/sw/DataAccess/df1/%s\' > %s.out' % (test, test, test) ]
#
#	autowaf.run_tests(ctx, APPNAME, commands, 1)
#
