#!/usr/bin/env python
import filecmp
import glob
import os
import shutil
import subprocess

from waflib.extras import autowaf as autowaf
import waflib.Logs as Logs, waflib.Options as Options

# Version of this package (even if built as a child)
SERD_VERSION       = '0.4.0'
SERD_MAJOR_VERSION = '0'

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
    autowaf.configure(conf)
    conf.line_just = 13
    autowaf.display_header('Serd Configuration')

    conf.load('compiler_cc')
    conf.env.append_value('CFLAGS', '-std=c99')

    conf.env['BUILD_TESTS'] = Options.options.build_tests
    conf.env['BUILD_UTILS'] = not Options.options.no_utils

    if Options.options.stack_check:
        autowaf.define(conf, 'SERD_STACK_CHECK', SERD_VERSION)

    autowaf.define(conf, 'SERD_VERSION', SERD_VERSION)
    conf.write_config_header('serd-config.h', remove=False)

    conf.env['INCLUDES_SERD'] = ['%s/serd-%s' % (
        conf.env['INCLUDEDIR'], SERD_MAJOR_VERSION)]
    conf.env['LIBPATH_SERD'] = [conf.env['LIBDIR']]
    conf.env['LIB_SERD'] = ['serd-%s' % SERD_MAJOR_VERSION];

    autowaf.display_msg(conf, "Utilities", str(conf.env['BUILD_UTILS']))
    autowaf.display_msg(conf, "Unit tests", str(conf.env['BUILD_TESTS']))
    print('')

def build(bld):
    # C Headers
    includedir = '${INCLUDEDIR}/serd-%s/serd' % SERD_MAJOR_VERSION
    bld.install_files(includedir, bld.path.ant_glob('serd/*.h'))

    # Pkgconfig file
    autowaf.build_pc(bld, 'SERD', SERD_VERSION, SERD_MAJOR_VERSION, [],
                     {'SERD_MAJOR_VERSION' : SERD_MAJOR_VERSION})

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
    obj.target          = 'serd-%s' % SERD_MAJOR_VERSION
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
    if bld.env['DOCS']:
        bld.add_post_fun(fix_docs)

def lint(ctx):
    subprocess.call('cpplint.py --filter=+whitespace/comments,-whitespace/tab,-whitespace/braces,-whitespace/labels,-build/header_guard,-readability/casting,-readability/todo,-build/include src/* serd/*', shell=True)

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

def fix_docs(ctx):
    try:
        top = os.getcwd()
        os.chdir('build/doc/html')
        os.system("sed -i 's/SERD_API //' group__serd.html")
        os.system("sed -i 's/SERD_DEPRECATED //' group__serd.html")
        os.remove('index.html')
        os.symlink('group__serd.html',
                   'index.html')
        os.chdir(top)
        os.chdir('build/doc/man/man3')
        os.system("sed -i 's/SERD_API //' serd.3")
    except:
        Logs.error("Failed to fix up documentation")

def upload_docs(ctx):
    os.system("rsync -ravz --delete -e ssh build/doc/html/ drobilla@drobilla.net:~/drobilla.net/docs/serd/")

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

    autowaf.run_tests(ctx, APPNAME, [
            './serdi_static file:%s/tests/manifest.ttl > /dev/null' % srcdir,
            './serdi_static file://%s/tests/manifest.ttl > /dev/null' % srcdir,
            './serdi_static %s/tests/UTF-8.ttl > /dev/null' % srcdir,
            './serdi_static -v > /dev/null',
            './serdi_static -h > /dev/null',
            './serdi_static -s "<foo> a <#Thingie> ." > /dev/null',
            './serdi_static /dev/null > /dev/null'],
                      0, name='serdi-cmd-good')

    autowaf.run_tests(ctx, APPNAME, [
            './serdi_static > /dev/null',
            './serdi_static ftp://example.org/unsupported.ttl > /dev/null',
            './serdi_static -i > /dev/null',
            './serdi_static -o > /dev/null',
            './serdi_static -z > /dev/null',
            './serdi_static -p > /dev/null',
            './serdi_static -c > /dev/null',
            './serdi_static -i illegal > /dev/null',
            './serdi_static -o illegal > /dev/null',
            './serdi_static -i turtle > /dev/null',
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

    thru_tests = good_tests
    thru_tests.remove('tests/test-id.ttl') # IDs are mapped so files won't be identical

    commands = []
    for test in thru_tests:
        base_uri = 'http://www.w3.org/2001/sw/DataAccess/df1/' + test
        out_filename = test + '.thru'
        commands += [
            '%s -o turtle -p foo %s/%s \'%s\' | %s -i turtle -c foo - \'%s\' | sed \'s/_:docid/_:genid/g\' > %s.thru' % (
                './serdi_static', srcdir, test, base_uri,
                './serdi_static', base_uri, test) ]
        
    autowaf.run_tests(ctx, APPNAME, commands, 0, name='turtle-round-trip')
    Logs.pprint('BOLD', '\nVerifying ntriples => turtle => ntriples')
    for test in thru_tests:
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
