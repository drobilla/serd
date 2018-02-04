#!/usr/bin/env python
import glob
import io
import os
import waflib.Logs as Logs
import waflib.Options as Options
import waflib.extras.autowaf as autowaf

# Library and package version (UNIX style major, minor, micro)
# major increment <=> incompatible changes
# minor increment <=> compatible changes (additions)
# micro increment <=> no interface changes
SERD_VERSION       = '0.29.2'
SERD_MAJOR_VERSION = '0'

# Mandatory waf variables
APPNAME = 'serd'        # Package name for waf dist
VERSION = SERD_VERSION  # Package version for waf dist
top     = '.'           # Source directory
out     = 'build'       # Build directory

def options(ctx):
    ctx.load('compiler_c')
    autowaf.set_options(ctx, test=True)
    opt = ctx.get_option_group('Configuration options')
    flags = {'no-utils':     'do not build command line utilities',
             'stack-check':  'include runtime stack sanity checks',
             'static':       'build static library',
             'no-shared':    'do not build shared library',
             'static-progs': 'build programs as static binaries',
             'largefile':    'build with large file support on 32-bit systems',
             'no-posix':     'do not use POSIX functions, even if present'}
    for name, desc in flags.items():
        opt.add_option('--' + name, action='store_true',
                       dest=name.replace('-', '_'), help=desc)

def configure(conf):
    autowaf.display_header('Serd Configuration')
    conf.load('compiler_c', cache=True)
    conf.load('autowaf', cache=True)

    conf.env.update({
        'BUILD_UTILS':  not Options.options.no_utils,
        'BUILD_SHARED': not Options.options.no_shared,
        'STATIC_PROGS': Options.options.static_progs,
        'BUILD_STATIC': Options.options.static or Options.options.static_progs})

    if not conf.env.BUILD_SHARED and not conf.env.BUILD_STATIC:
        conf.fatal('Neither a shared nor a static build requested')

    if Options.options.stack_check:
        autowaf.define(conf, 'SERD_STACK_CHECK', SERD_VERSION)

    if Options.options.largefile:
        conf.env.append_unique('DEFINES', ['_FILE_OFFSET_BITS=64'])

    if not Options.options.no_posix:
        for name, header in {'posix_memalign': 'stdlib.h',
                             'posix_fadvise':  'fcntl.h',
                             'fileno':         'stdio.h'}.items():
            autowaf.check_function(conf, 'c', name,
                                   header_name = header,
                                   define_name = 'HAVE_' + name.upper(),
                                   defines     = ['_POSIX_C_SOURCE=200809L'],
                                   mandatory   = False)

    autowaf.define(conf, 'SERD_VERSION', SERD_VERSION)
    autowaf.set_lib_env(conf, 'serd', SERD_VERSION)
    conf.write_config_header('serd_config.h', remove=False)

    autowaf.display_summary(conf)
    autowaf.display_msg(conf, 'Static library', bool(conf.env.BUILD_STATIC))
    autowaf.display_msg(conf, 'Shared library', bool(conf.env.BUILD_SHARED))
    autowaf.display_msg(conf, 'Utilities', bool(conf.env.BUILD_UTILS))
    autowaf.display_msg(conf, 'Unit tests', bool(conf.env.BUILD_TESTS))
    print('')

lib_source = ['src/byte_source.c',
              'src/env.c',
              'src/n3.c',
              'src/node.c',
              'src/reader.c',
              'src/string.c',
              'src/uri.c',
              'src/writer.c']

def build(bld):
    # C Headers
    includedir = '${INCLUDEDIR}/serd-%s/serd' % SERD_MAJOR_VERSION
    bld.install_files(includedir, bld.path.ant_glob('serd/*.h'))

    # Pkgconfig file
    autowaf.build_pc(bld, 'SERD', SERD_VERSION, SERD_MAJOR_VERSION, [],
                     {'SERD_MAJOR_VERSION' : SERD_MAJOR_VERSION})

    defines = []
    lib_args = {'export_includes': ['.'],
                'includes':        ['.', './src'],
                'cflags':          ['-fvisibility=hidden'],
                'lib':             ['m'],
                'vnum':            SERD_VERSION,
                'install_path':    '${LIBDIR}'}
    if bld.env.MSVC_COMPILER:
        lib_args['cflags'] = []
        lib_args['lib']    = []
        defines            = []

    # Shared Library
    if bld.env.BUILD_SHARED:
        bld(features        = 'c cshlib',
            source          = lib_source,
            name            = 'libserd',
            target          = 'serd-%s' % SERD_MAJOR_VERSION,
            defines         = defines + ['SERD_SHARED', 'SERD_INTERNAL'],
            **lib_args)

    # Static library
    if bld.env.BUILD_STATIC:
        bld(features        = 'c cstlib',
            source          = lib_source,
            name            = 'libserd_static',
            target          = 'serd-%s' % SERD_MAJOR_VERSION,
            defines         = defines + ['SERD_INTERNAL'],
            **lib_args)

    if bld.env.BUILD_TESTS:
        test_args = {'includes':     ['.', './src'],
                     'cflags':       [''] if bld.env.NO_COVERAGE else ['--coverage'],
                     'linkflags':    [''] if bld.env.NO_COVERAGE else ['--coverage'],
                     'lib':          lib_args['lib'],
                     'install_path': ''}

        # Profiled static library for test coverage
        bld(features     = 'c cstlib',
            source       = lib_source,
            name         = 'libserd_profiled',
            target       = 'serd_profiled',
            defines      = defines + ['SERD_INTERNAL'],
            **test_args)

        # Test programs
        for prog in [('serdi_static', 'src/serdi.c'),
                     ('serd_test', 'tests/serd_test.c')]:
            bld(features     = 'c cprogram',
                source       = prog[1],
                use          = 'libserd_profiled',
                target       = prog[0],
                defines      = defines,
                **test_args)

    # Utilities
    if bld.env.BUILD_UTILS:
        obj = bld(features     = 'c cprogram',
                  source       = 'src/serdi.c',
                  target       = 'serdi',
                  includes     = ['.', './src'],
                  use          = 'libserd',
                  lib          = lib_args['lib'],
                  install_path = '${BINDIR}')
        if not bld.env.BUILD_SHARED or bld.env.STATIC_PROGS:
            obj.use = 'libserd_static'
        if bld.env.STATIC_PROGS:
            obj.env.SHLIB_MARKER = obj.env.STLIB_MARKER
            obj.linkflags        = ['-static']

    # Documentation
    autowaf.build_dox(bld, 'SERD', SERD_VERSION, top, out)

    # Man page
    bld.install_files('${MANDIR}/man1', 'doc/serdi.1')

    bld.add_post_fun(autowaf.run_ldconfig)
    if bld.env.DOCS:
        bld.add_post_fun(lambda ctx: autowaf.make_simple_dox(APPNAME))

def lint(ctx):
    "checks code for style issues"
    import subprocess
    cmd = ("clang-tidy -p=. -header-filter=.* -checks=\"*," +
           "-clang-analyzer-alpha.*," +
           "-google-readability-todo," +
           "-llvm-header-guard," +
           "-misc-unused-parameters," +
           "-readability-else-after-return\" " +
           "../src/*.c")
    subprocess.call(cmd, cwd='build', shell=True)

def amalgamate(ctx):
    "builds single-file amalgamated source"
    import shutil
    shutil.copy('serd/serd.h', 'build/serd.h')
    with open('build/serd.c', 'w') as amalgamation:
        with open('src/serd_internal.h') as serd_internal_h:
            for l in serd_internal_h:
                amalgamation.write(l.replace('serd/serd.h', 'serd.h'))

        for f in lib_source:
            with open(f) as fd:
                amalgamation.write('\n/**\n   @file %s\n*/' % f)
                header = True
                for l in fd:
                    if header:
                        if l == '*/\n':
                            header = False
                    else:
                        if l != '#include "serd_internal.h"\n':
                            amalgamation.write(l)

    for i in ['c', 'h']:
        Logs.info('Wrote build/serd.%s' % i)

def upload_docs(ctx):
    os.system('rsync -ravz --delete -e ssh build/doc/html/ drobilla@drobilla.net:~/drobilla.net/docs/serd/')
    for page in glob.glob('doc/*.[1-8]'):
        os.system('soelim %s | pre-grohtml troff -man -wall -Thtml | post-grohtml > build/%s.html' % (page, page))
        os.system('rsync -avz --delete -e ssh build/%s.html drobilla@drobilla.net:~/drobilla.net/man/' % page)

def file_equals(patha, pathb, subst_from='', subst_to=''):
    with io.open(patha, 'rU', encoding='utf-8') as fa:
        with io.open(pathb, 'rU', encoding='utf-8') as fb:
            for linea in fa:
                lineb = fb.readline()
                if (linea.replace(subst_from, subst_to) !=
                    lineb.replace(subst_from, subst_to)):
                    return False
    return True

def earl_assertion(test, passed, asserter):
    import datetime

    asserter_str = ''
    if asserter is not None:
        asserter_str = '\n\tearl:assertedBy <%s> ;' % asserter

    passed_str = 'earl:failed'
    if passed:
        passed_str = 'earl:passed'

    return '''
[]
	a earl:Assertion ;%s
	earl:subject <http://drobilla.net/sw/serd> ;
	earl:test <%s> ;
	earl:result [
		a earl:TestResult ;
		earl:outcome %s ;
		dc:date "%s"^^xsd:dateTime
	] .
''' % (asserter_str,
       test,
       passed_str,
       datetime.datetime.now().replace(microsecond=0).isoformat())

def check_output(out_filename, check_filename, subst_from='', subst_to=''):
    import difflib
    import sys
    if not os.access(out_filename, os.F_OK):
        Logs.pprint('RED', 'FAIL: output %s is missing' % out_filename)
    elif not file_equals(check_filename, out_filename, subst_from, subst_to):
        Logs.pprint('RED', 'FAIL: %s != %s' % (os.path.abspath(out_filename),
                                               check_filename))
        with io.open(out_filename, encoding='utf-8') as out_file:
            with io.open(check_filename, encoding='utf-8') as check_file:
                diff = difflib.unified_diff(check_file.readlines(),
                                            out_file.readlines(),
                                            fromfile=check_filename,
                                            tofile=out_filename)
                for line in diff:
                    sys.stderr.write(line)
    else:
        return True

    return False

def test_thru(ctx, base, path, check_filename, flags, isyntax, osyntax,
              options='', quiet=False):
    in_filename = os.path.join(ctx.path.abspath(), path);
    out_filename = path + '.thru'

    command = ('serdi_static %s %s -i %s -o %s -p foo "%s" "%s" | '
               'serdi_static %s -i %s -o %s -c foo - "%s" > %s') % (
                   options, flags.ljust(5),
                   isyntax, isyntax, in_filename, base,
                   options, isyntax, osyntax, base, out_filename)

    if autowaf.run_test(ctx, APPNAME, command, 0, name='  to ' + out_filename, quiet=quiet):
        autowaf.run_test(
            ctx, APPNAME,
            lambda: check_output(out_filename, check_filename, '_:docid', '_:genid'),
            True,
            name='from ' + out_filename,
            quiet=quiet)
    else:
        Logs.pprint('RED', 'FAIL: error running %s' % command)

def file_uri_to_path(uri):
    try:
        from urlparse import urlparse # Python 2
    except:
        from urllib.parse import urlparse # Python 3

    path  = urlparse(uri).path
    drive = os.path.splitdrive(path[1:])[0]
    return path if not drive else path[1:]

def test_suite(ctx, base_uri, testdir, report, isyntax, osyntax, options=''):
    import itertools

    srcdir = ctx.path.abspath()

    def load_rdf(filename):
        "Load an RDF file into python dictionaries via serdi.  Only supports URIs."
        import subprocess
        import re
        model = {}
        proc  = subprocess.Popen(['./serdi_static', filename], stdout=subprocess.PIPE)
        for line in proc.communicate()[0].splitlines():
            matches = re.match('<([^ ]*)> <([^ ]*)> <([^ ]*)> \.', line.decode('utf-8'))
            if matches:
                if matches.group(1) not in model:
                    model[matches.group(1)] = {}
                if matches.group(2) not in model[matches.group(1)]:
                    model[matches.group(1)][matches.group(2)] = []
                model[matches.group(1)][matches.group(2)] += [matches.group(3)]
        return model

    mf = 'http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#'
    model = load_rdf(os.path.join(srcdir, 'tests', testdir, 'manifest.ttl'))

    asserter = ''
    if os.getenv('USER') == 'drobilla':
        asserter = 'http://drobilla.net/drobilla#me'

    def run_test(command, expected_return, name, quiet=False):
        result = autowaf.run_test(ctx, APPNAME, command, expected_return, name=name, quiet=quiet)
        if not result[0]:
            autowaf.run_test(ctx, APPNAME,
                             lambda: result[1][1] != '',
                             True, name=name + ' has error message', quiet=True)
        return result

    def run_tests(test_class, expected_return):
        tests = []
        for s, desc in model.items():
            if str(test_class) in desc['http://www.w3.org/1999/02/22-rdf-syntax-ns#type']:
                tests += [s]
        if len(tests) == 0:
            return

        thru_flags   = ['-e', '-f', '-b', '-r http://example.org/']
        thru_options = []
        for n in range(len(thru_flags) + 1):
            for flags in itertools.combinations(thru_flags, n):
                thru_options += [flags]
        thru_options_iter = itertools.cycle(thru_options)

        with autowaf.begin_tests(ctx, APPNAME, str(test_class)):
            for (num, test) in enumerate(sorted(tests)):
                action_node = model[test][mf + 'action'][0]
                action      = os.path.join('tests', testdir, os.path.basename(action_node))
                abs_action  = os.path.join(srcdir, action)
                uri         = base_uri + os.path.basename(action)
                command     = 'serdi_static %s -f "%s" "%s" > %s' % (
                    options, abs_action, uri, action + '.out')

                # Run strict test
                result = run_test(command, expected_return, action)
                if (mf + 'result') in model[test]:
                    # Check output against test suite
                    check_uri  = model[test][mf + 'result'][0]
                    check_path = file_uri_to_path(check_uri)
                    result     = autowaf.run_test(
                        ctx, APPNAME,
                        lambda: check_output(action + '.out', check_path),
                        True, name=str(action) + ' check', quiet=True)

                    # Run round-trip tests
                    test_thru(ctx, uri, action, check_path,
                              ' '.join(next(thru_options_iter)), isyntax, osyntax, options, quiet=True)

                # Write test report entry
                if report is not None:
                    report.write(earl_assertion(test, result[0], asserter))

                # Run lax test
                run_test(command.replace('-f', '-l -f'), None, action + ' lax', True)

    def test_types():
        types = []
        for lang in ['Turtle', 'NTriples', 'Trig', 'NQuads']:
            types += [['http://www.w3.org/ns/rdftest#Test%sPositiveSyntax' % lang, 0],
                      ['http://www.w3.org/ns/rdftest#Test%sNegativeSyntax' % lang, 1],
                      ['http://www.w3.org/ns/rdftest#Test%sNegativeEval' % lang, 1],
                      ['http://www.w3.org/ns/rdftest#Test%sEval' % lang, 0]]
        return types

    for i in test_types():
        run_tests(i[0], i[1])

def test(ctx):
    "runs test suite"

    # Create test output directories
    for i in ['bad', 'good', 'TurtleTests', 'NTriplesTests', 'NQuadsTests', 'TriGTests']:
        try:
            test_dir = os.path.join(autowaf.build_dir(APPNAME, 'tests'), i)
            os.makedirs(test_dir)
            for i in glob.glob(test_dir + '/*.*'):
                os.remove(i)
        except:
            pass

    srcdir = ctx.path.abspath()
    os.environ['PATH'] = '.' + os.pathsep + os.getenv('PATH')

    autowaf.pre_test(ctx, APPNAME)
    autowaf.run_test(ctx, APPNAME, 'serd_test', dirs=['.'])

    autowaf.run_test(ctx, APPNAME,
                     'serdi_static -q -o turtle "%s/tests/good/base.ttl" "base.ttl" > tests/good/base.ttl.out' % srcdir,
                     0, name='base')

    autowaf.run_test(ctx, APPNAME,
                     lambda: file_equals('%s/tests/good/base.ttl' % srcdir, 'tests/good/base.ttl.out'),
                     True, name='base-check')

    nul = os.devnull
    autowaf.run_tests(ctx, APPNAME, [
            'serdi_static "file://%s/tests/good/manifest.ttl" > %s' % (srcdir, nul),
            'serdi_static -v > %s' % nul,
            'serdi_static -h > %s' % nul,
            'serdi_static -s "<foo> a <#Thingie> ." > %s' % nul,
            'serdi_static %s > %s' % (nul, nul)
    ], 0, name='serdi-cmd-good')

    autowaf.run_tests(ctx, APPNAME, [
            'serdi_static -q "file://%s/tests/bad/bad-id-clash.ttl" > %s' % (srcdir, nul),
            'serdi_static > %s' % nul,
            'serdi_static ftp://example.org/unsupported.ttl > %s' % nul,
            'serdi_static -i > %s' % nul,
            'serdi_static -o > %s' % nul,
            'serdi_static -z > %s' % nul,
            'serdi_static -p > %s' % nul,
            'serdi_static -c > %s' % nul,
            'serdi_static -r > %s' % nul,
            'serdi_static -i illegal > %s' % nul,
            'serdi_static -o illegal > %s' % nul,
            'serdi_static -i turtle > %s' % nul,
            'serdi_static /no/such/file > %s' % nul],
                      1, name='serdi-cmd-bad')

    with autowaf.begin_tests(ctx, APPNAME, 'io-error'):
        # Test read error by reading a directory
        autowaf.run_test(ctx, APPNAME, 'serdi_static -e "file://%s/"' % srcdir,
                         1, name='read_error')

        # Test read error with bulk input by reading a directory
        autowaf.run_test(ctx, APPNAME, 'serdi_static "file://%s/"' % srcdir,
                         1, name='read_error_bulk')

        # Test write error by writing to /dev/full
        if os.path.exists('/dev/full'):
            autowaf.run_test(ctx, APPNAME,
                             'serdi_static "file://%s/tests/good/manifest.ttl" > /dev/full' % srcdir,
                             1, name='write_error')

    # Serd-specific test cases
    serd_base = 'http://drobilla.net/sw/serd/tests/'
    test_suite(ctx, serd_base + 'good/', 'good', None, 'Turtle', 'NTriples')
    test_suite(ctx, serd_base + 'bad/', 'bad', None, 'Turtle', 'NTriples')

    # Standard test suites
    with open('earl.ttl', 'w') as report:
        report.write('@prefix earl: <http://www.w3.org/ns/earl#> .\n'
                     '@prefix dc: <http://purl.org/dc/elements/1.1/> .\n')

        with open(os.path.join(srcdir, 'serd.ttl')) as serd_ttl:
            for line in serd_ttl:
                report.write(line)

        w3c_base = 'http://www.w3.org/2013/'
        test_suite(ctx, w3c_base + 'NTriplesTests/',
                   'NTriplesTests', report, 'NTriples', 'NTriples')
        test_suite(ctx, w3c_base + 'NQuadsTests/',
                   'NQuadsTests', report, 'NQuads', 'NQuads')
        test_suite(ctx, w3c_base + 'TriGTests/',
                   'TriGTests', report, 'TriG', 'NQuads', '-a')

    autowaf.post_test(ctx, APPNAME)
    if ctx.autowaf_tests[APPNAME]['failed'] > 0:
        ctx.fatal('Failed %s tests' % APPNAME)

def posts(ctx):
    path = str(ctx.path.abspath())
    autowaf.news_to_posts(
        os.path.join(path, 'NEWS'),
        {'title'        : 'Serd',
         'description'  : autowaf.get_blurb(os.path.join(path, 'README.md')),
         'dist_pattern' : 'http://download.drobilla.net/serd-%s.tar.bz2'},
        { 'Author' : 'drobilla',
          'Tags'   : 'Hacking, RDF, Serd' },
        os.path.join(out, 'posts'))
