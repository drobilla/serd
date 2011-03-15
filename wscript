#!/usr/bin/env python
import filecmp
import glob
import os
import shutil
import subprocess

import waflib.Logs as Logs, waflib.Options as Options
from waflib.extras import autowaf as autowaf

# Version of this package (even if built as a child)
SERD_VERSION = '0.1.0'

# Library version (UNIX style major, minor, micro)
# major increment <=> incompatible changes
# minor increment <=> compatible changes (additions)
# micro increment <=> no interface changes
# Serd uses the same version number for both library and package
SERD_LIB_VERSION = SERD_VERSION

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
	opt.add_option('--stack-check', action='store_true', default=False, dest='stack_check',
			help="Include runtime stack sanity checks")

def configure(conf):
	conf.line_just = max(conf.line_just, 59)
	autowaf.configure(conf)
	autowaf.display_header('Serd Configuration')

	conf.load('compiler_cc')
	conf.env.append_value('CFLAGS', '-std=c99')

	conf.env['BUILD_TESTS'] = Options.options.build_tests
	conf.env['BUILD_UTILS'] = not Options.options.no_utils

	if Options.options.stack_check:
		autowaf.define(conf, 'SERD_STACK_CHECK', SERD_VERSION)

	conf.env['BUILD_TESTS'] = Options.options.build_tests

	autowaf.define(conf, 'SERD_VERSION', SERD_VERSION)
	conf.write_config_header('serd-config.h', remove=False)

	autowaf.display_msg(conf, "Utilities", str(conf.env['BUILD_UTILS']))
	autowaf.display_msg(conf, "Unit tests", str(conf.env['BUILD_TESTS']))
	print('')

def build(bld):
	# C Headers
	bld.install_files('${INCLUDEDIR}/serd', bld.path.ant_glob('serd/*.h'))

	# Pkgconfig file
	autowaf.build_pc(bld, 'SERD', SERD_VERSION, [])

	lib_source = '''
		src/env.c
		src/node.c
		src/reader.c
		src/uri.c
		src/writer.c
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

	# Man page
	bld.install_files('${MANDIR}/man1', 'doc/serdi.1')

	bld.add_post_fun(autowaf.run_ldconfig)

def lint(ctx):
	subprocess.call('cpplint.py --filter=-whitespace,+whitespace/comments,-build/header_guard,-readability/casting,-readability/todo src/* serd/*', shell=True)

def amalgamate(ctx):
	shutil.copy('serd/serd.h', 'build/serd-%s.h' % SERD_VERSION)
	amalgamation = open('build/serd-%s.c' % SERD_VERSION, 'w')

	serd_internal_h = open('src/serd_internal.h')
	for l in serd_internal_h:
		if l == '#include "serd/serd.h"\n':
			amalgamation.write('#include "serd-%s.h"\n' % SERD_VERSION)
		else:
			amalgamation.write(l)
	serd_internal_h.close()

	for f in 'env.c node.c reader.c uri.c writer.c'.split():
		fd = open('src/' + f)
		amalgamation.write('\n/**\n * @file %s\n */\n' % f)
		header = True
		for l in fd:
			if header:
				if l == '*/\n':
					header = False
			else:
				if l != '#include "serd_internal.h"\n':
					amalgamation.write(l)
		fd.close()
	
	amalgamation.close()

def test(ctx):
	blddir = ""
	top_level = (len(ctx.stack_path) > 1)
	if top_level:
		blddir = 'build/serd/tests'
	else:
		blddir = 'build/tests'

	try:
		os.makedirs(blddir)
	except:
		pass

	for i in glob.glob('build/tests/*.*'):
		os.remove(i)
		
	srcdir   = ctx.path.abspath()
	orig_dir = os.path.abspath(os.curdir)

	os.chdir(srcdir)
	
	good_tests = glob.glob('tests/test-*.ttl')
	good_tests.sort()

	bad_tests = glob.glob('tests/bad-*.ttl')
	bad_tests.sort()

	os.chdir(orig_dir)
		
	autowaf.pre_test(ctx, APPNAME)

	autowaf.run_tests(ctx, APPNAME,
					  ['./serdi_static file:%s/tests/manifest.ttl > /dev/null' % srcdir,
					   './serdi_static file://%s/tests/manifest.ttl > /dev/null' % srcdir,
					   './serdi_static %s/tests/UTF-8.ttl > /dev/null' % srcdir,
					   './serdi_static -v > /dev/null',
					   './serdi_static -s "<foo> a <#Thingie> ." > /dev/null',
					   './serdi_static /dev/null > /dev/null'],
					  0, name='serdi-cmd-good')

	autowaf.run_tests(ctx, APPNAME,
					  ['./serdi_static > /dev/null',
					   './serdi_static ftp://example.org/unsupported.ttl > /dev/null',
					   './serdi_static -o > /dev/null',
					   './serdi_static -z > /dev/null',
					   './serdi_static -o illegal > /dev/null',
					   './serdi_static /no/such/file > /dev/null'],
					  1, name='serdi-cmd-bad')

	commands = []
	for test in good_tests:
		base_uri = 'http://www.w3.org/2001/sw/DataAccess/df1/' + test
		commands += [ './serdi_static %s/%s \'%s\' > %s.out' % (srcdir, test, base_uri, test) ]

	autowaf.run_tests(ctx, APPNAME, commands, 0, name='good')

	Logs.pprint('BOLD', '\nVerifying turtle => ntriples')
	for test in good_tests:
		out_filename = test + '.out'
		if not os.access(out_filename, os.F_OK):
			Logs.pprint('RED', 'FAIL: %s output is missing' % test)
		elif filecmp.cmp(srcdir + '/' + test.replace('.ttl', '.out'),
						 test + '.out',
						 False) != 1:
			Logs.pprint('RED', 'FAIL: %s is incorrect' % out_filename)
		else:
			Logs.pprint('GREEN', 'Pass: %s' % test)
	
	commands = []
	for test in bad_tests:
	    commands += [ './serdi_static %s/%s \'http://www.w3.org/2001/sw/DataAccess/df1/%s\' > %s.out' % (srcdir, test, test, test) ]

	autowaf.run_tests(ctx, APPNAME, commands, 1, name='bad')

	commands = []
	for test in good_tests:
		base_uri = 'http://www.w3.org/2001/sw/DataAccess/df1/' + test
		out_filename = test + '.thru'
		commands += [
			'%s -o turtle %s/%s \'%s\' | %s - \'%s\' > %s.thru' % (
				'./serdi_static', srcdir, test, base_uri,
				'./serdi_static', base_uri, test) ]
		
	autowaf.run_tests(ctx, APPNAME, commands, 0, name='turtle-round-trip')
	Logs.pprint('BOLD', '\nVerifying ntriples => turtle => ntriples')
	for test in good_tests:
		out_filename = test + '.thru'
		if not os.access(out_filename, os.F_OK):
			Logs.pprint('RED', 'FAIL: %s output is missing' % test)
		elif filecmp.cmp(srcdir + '/' + test.replace('.ttl', '.out'),
						 test + '.thru',
						 False) != 1:
			Logs.pprint('RED', 'FAIL: %s is incorrect' % out_filename)
		else:
			Logs.pprint('GREEN', 'Pass: %s' % test)

	autowaf.post_test(ctx, APPNAME)
