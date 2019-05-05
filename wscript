#!/usr/bin/env python

import glob
import io
import os
import sys

from waflib import Logs, Options
from waflib.extras import autowaf

# Library and package version (UNIX style major, minor, micro)
# major increment <=> incompatible changes
# minor increment <=> compatible changes (additions)
# micro increment <=> no interface changes
SERD_VERSION       = '1.0.0'
SERD_MAJOR_VERSION = '1'

# Mandatory waf variables
APPNAME = 'serd'        # Package name for waf dist
VERSION = SERD_VERSION  # Package version for waf dist
top     = '.'           # Source directory
out     = 'build'       # Build directory

# Release variables
uri          = 'http://drobilla.net/sw/serd'
dist_pattern = 'http://download.drobilla.net/serd-%d.%d.%d.tar.bz2'
post_tags    = ['Hacking', 'RDF', 'Serd']

def options(ctx):
    ctx.load('compiler_c')
    opt = ctx.configuration_options()
    ctx.add_flags(
        opt,
        {'no-utils':     'do not build command line utilities',
         'stack-check':  'include runtime stack sanity checks',
         'static':       'build static library',
         'no-shared':    'do not build shared library',
         'static-progs': 'build programs as static binaries',
         'largefile':    'build with large file support on 32-bit systems',
         'no-pcre':      'do not use PCRE, even if present',
         'no-posix':     'do not use POSIX functions, even if present'})

def configure(conf):
    conf.load('compiler_c', cache=True)
    conf.load('autowaf', cache=True)
    autowaf.set_c_lang(conf, 'c99')

    conf.env.update({
        'BUILD_UTILS':  not Options.options.no_utils,
        'BUILD_SHARED': not Options.options.no_shared,
        'STATIC_PROGS': Options.options.static_progs,
        'BUILD_STATIC': Options.options.static or Options.options.static_progs})

    if not conf.env.BUILD_SHARED and not conf.env.BUILD_STATIC:
        conf.fatal('Neither a shared nor a static build requested')

    if Options.options.stack_check:
        conf.define('SERD_STACK_CHECK', SERD_VERSION)

    if Options.options.largefile:
        conf.env.append_unique('DEFINES', ['_FILE_OFFSET_BITS=64'])

    if not Options.options.no_posix:
        for name, header in {'posix_memalign': 'stdlib.h',
                             'posix_fadvise':  'fcntl.h',
                             'fileno':         'stdio.h'}.items():
            conf.check_function('c', name,
                                header_name = header,
                                define_name = 'HAVE_' + name.upper(),
                                defines     = ['_POSIX_C_SOURCE=200809L'],
                                mandatory   = False)

    if not Options.options.no_pcre:
        autowaf.check_pkg(conf, 'libpcre', uselib_store='PCRE', mandatory=False)

    if conf.env.HAVE_PCRE:
        if conf.check(cflags=['-pthread'], mandatory=False):
            conf.env.PTHREAD_CFLAGS = ['-pthread']
            if conf.env.CC_NAME != 'clang':
                conf.env.PTHREAD_LINKFLAGS = ['-pthread']
        elif conf.check(linkflags=['-lpthread'], mandatory=False):
            conf.env.PTHREAD_CFLAGS    = []
            conf.env.PTHREAD_LINKFLAGS = ['-lpthread']
        else:
            conf.env.PTHREAD_CFLAGS    = []
            conf.env.PTHREAD_LINKFLAGS = []

    autowaf.set_lib_env(conf, 'serd', SERD_VERSION)
    conf.write_config_header('serd_config.h', remove=False)

    autowaf.display_summary(
        conf,
        {'Build static library': bool(conf.env['BUILD_STATIC']),
         'Build shared library': bool(conf.env['BUILD_SHARED']),
         'Build utilities':      bool(conf.env['BUILD_UTILS']),
         'Build unit tests':     bool(conf.env['BUILD_TESTS'])})

lib_headers = ['src/reader.h']

lib_source = ['src/base64.c',
              'src/byte_sink.c',
              'src/byte_source.c',
              'src/cursor.c',
              'src/env.c',
              'src/inserter.c',
              'src/iter.c',
              'src/model.c',
              'src/n3.c',
              'src/node.c',
              'src/nodes.c',
              'src/range.c',
              'src/reader.c',
              'src/sink.c',
              'src/statement.c',
              'src/string.c',
              'src/syntax.c',
              'src/system.c',
              'src/uri.c',
              'src/validate.c',
              'src/world.c',
              'src/writer.c',
              'src/zix/btree.c',
              'src/zix/digest.c',
              'src/zix/hash.c']

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
                'use':             ['PCRE'],
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
            uselib          = 'PCRE',
            defines         = defines + ['SERD_SHARED', 'SERD_INTERNAL'],
            **lib_args)

    # Static library
    if bld.env.BUILD_STATIC:
        bld(features        = 'c cstlib',
            source          = lib_source,
            name            = 'libserd_static',
            target          = 'serd-%s' % SERD_MAJOR_VERSION,
            uselib          = 'PCRE',
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
            uselib       = 'PCRE',
            defines      = defines + ['SERD_INTERNAL'],
            **test_args)

        # Test programs
        for prog in [('serdi_static', 'src/serdi.c'),
                     ('base64_test', 'tests/base64_test.c'),
                     ('cursor_test', 'tests/cursor_test.c'),
                     ('statement_test', 'tests/statement_test.c'),
                     ('sink_test', 'tests/sink_test.c'),
                     ('serd_test', 'tests/serd_test.c'),
                     ('read_chunk_test', 'tests/read_chunk_test.c'),
                     ('terse_write_test', 'tests/terse_write_test.c'),
                     ('nodes_test', 'tests/nodes_test.c'),
                     ('overflow_test', 'tests/overflow_test.c'),
                     ('model_test', 'tests/model_test.c')]:
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
                  uselib       = 'PCRE',
                  lib          = lib_args['lib'],
                  cflags       = bld.env.PTHREAD_CFLAGS,
                  linkflags    = bld.env.PTHREAD_LINKFLAGS,
                  install_path = '${BINDIR}')
        if not bld.env.BUILD_SHARED or bld.env.STATIC_PROGS:
            obj.use = 'libserd_static'
        if bld.env.STATIC_PROGS:
            obj.env.SHLIB_MARKER  = obj.env.STLIB_MARKER
            obj.linkflags        += ['-static']

    # Documentation
    if bld.env.DOCS:
        autowaf.build_dox(bld, 'SERD', SERD_VERSION, top, out)
        bld(features='subst',
            source='doc/index.html.in',
            target='doc/index.html',
            install_path='',
            name='index',
            SERD_VERSION=SERD_VERSION)

    # Man page
    bld.install_files('${MANDIR}/man1', 'doc/serdi.1')

    bld.add_post_fun(autowaf.run_ldconfig)

def lint(ctx):
    "checks code for style issues"
    import subprocess
    subprocess.call('clang-tidy ../src/*.c ../tests/*.c ../tests/*.cpp',
                    cwd='build', shell=True)

def amalgamate(ctx):
    "builds single-file amalgamated source"
    import shutil
    import re
    shutil.copy('serd/serd.h', 'build/serd.h')

    def include_line(line):
        return (not re.match('#include "[^/]*\.h"', line) and
                not re.match('#include "serd/serd.h"', line))

    with open('build/serd.c', 'w') as amalgamation:
        amalgamation.write('/* This is amalgamated code, do not edit! */\n')
        amalgamation.write('#include "serd.h"\n\n')

        for header_path in ['src/serd_internal.h',
                            'src/byte_sink.h',
                            'src/byte_source.h',
                            'src/stack.h',
                            'src/string_utils.h',
                            'src/uri_utils.h',
                            'src/reader.h']:
            with open(header_path) as header:
                for l in header:
                    if include_line(l):
                        amalgamation.write(l)

        for f in lib_headers + lib_source:
            with open(f) as fd:
                amalgamation.write('\n/**\n   @file %s\n*/' % f)
                for l in fd:
                    if include_line(l):
                        amalgamation.write(l)

    for i in ['c', 'h']:
        Logs.info('Wrote build/serd.%s' % i)

def earl_assertion(test, passed, asserter):
    import datetime

    asserter_str = ''
    if asserter is not None:
        asserter_str = '\n\tearl:assertedBy <%s> ;' % asserter

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
       'earl:passed' if passed else 'earl:failed',
       datetime.datetime.now().replace(microsecond=0).isoformat())

serdi = './serdi_static'

def test_osyntax_options(osyntax):
    if osyntax.lower() == 'ntriples' or osyntax.lower() == 'nquads':
        return [['-a']]
    return []

def flatten_options(opts):
    return [o for sublist in opts for o in sublist]

def test_thru(check, base, path, check_path, flags, isyntax, osyntax, options=[]):
    out_path = path + '.pass'
    opts = options + flatten_options(test_osyntax_options(osyntax))
    flags = flatten_options(flags)
    osyntax_opts = [f for sublist in test_osyntax_options(osyntax) for f in sublist]
    out_cmd = [serdi] + opts + flags + [
        '-i', isyntax,
        '-o', isyntax,
        '-p', 'foo',
        '-I', base,
        check.tst.src_path(path)]

    thru_path = path + '.thru'
    thru_cmd = [serdi] + opts + osyntax_opts + [
        '-i', isyntax,
        '-o', osyntax,
        '-c', 'foo',
        '-a',
        '-I', base,
        out_path]

    return (check(out_cmd, stdout=out_path, verbosity=0, name=out_path) and
            check(thru_cmd, stdout=thru_path, verbosity=0, name=thru_path) and
            check.file_equals(check_path, thru_path, verbosity=0))

def file_uri_to_path(uri):
    try:
        from urlparse import urlparse # Python 2
    except:
        from urllib.parse import urlparse # Python 3

    path  = urlparse(uri).path
    drive = os.path.splitdrive(path[1:])[0]
    return path if not drive else path[1:]

def _test_output_syntax(test_class):
    if 'NTriples' in test_class or 'Turtle' in test_class:
        return 'NTriples'
    elif 'NQuads' in test_class or 'Trig' in test_class:
        return 'NQuads'
    raise Exception('Unknown test class <%s>' % test_class)

def _load_rdf(filename):
    "Load an RDF file into python dictionaries via serdi.  Only supports URIs."
    import subprocess
    import re

    rdf_type = 'http://www.w3.org/1999/02/22-rdf-syntax-ns#type'
    model = {}
    instances = {}

    cmd = ['./serdi_static', filename]
    if Options.options.test_wrapper:
        import shlex
        cmd = shlex.split(Options.options.test_wrapper) + cmd

    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    for line in proc.communicate()[0].splitlines():
        matches = re.match('<([^ ]*)> <([^ ]*)> <([^ ]*)> \.', line.decode('utf-8'))
        if matches:
            s, p, o = (matches.group(1), matches.group(2), matches.group(3))
            if s not in model:
                model[s] = {p: [o]}
            elif p not in model[s]:
                model[s][p] = [o]
            else:
                model[s][p].append(o)

            if p == rdf_type:
                if o not in instances:
                    instances[o] = set([s])
                else:
                    instances[o].update([s])

    return model, instances

def _file_lines_equal(patha, pathb, subst_from='', subst_to=''):
    import io

    for path in (patha, pathb):
        if not os.access(path, os.F_OK):
            Logs.pprint('RED', 'error: missing file %s' % path)
            return False

    la = sorted(set(io.open(patha, encoding='utf-8').readlines()))
    lb = sorted(set(io.open(pathb, encoding='utf-8').readlines()))
    if la != lb:
        autowaf.show_diff(la, lb, patha, pathb)
        return False

    return True

def _option_combinations(options):
    "Return an iterator that cycles through all combinations of the given options"
    import itertools

    combinations = []
    for n in range(len(options) + 1):
        combinations += list(itertools.combinations(options, n))

    return itertools.cycle(combinations)

def test_suite(ctx,
               base_uri,
               testdir,
               report,
               isyntax,
               options=[],
               output_syntax=None):
    import itertools

    srcdir = ctx.path.abspath()

    mf = 'http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#'
    manifest_path = os.path.join(srcdir, 'tests', testdir, 'manifest.ttl')
    model, instances = _load_rdf(manifest_path)

    asserter = ''
    if os.getenv('USER') == 'drobilla':
        asserter = 'http://drobilla.net/drobilla#me'

    def run_tests(test_class, tests, expected_return):
        thru_flags   = [['-e'], ['-b'], ['-r', 'http://example.org/']]
        osyntax = output_syntax or _test_output_syntax(test_class)
        extra_options_iter = _option_combinations([] if output_syntax else [['-f']])
        thru_options_iter = _option_combinations(thru_flags)
        tests_name = '%s.%s' % (testdir, test_class[test_class.find('#') + 1:])
        with ctx.group(tests_name) as check:
            for test in sorted(tests):
                action_node = model[test][mf + 'action'][0]
                action      = os.path.join('tests', testdir, os.path.basename(action_node))
                rel_action  = os.path.join(os.path.relpath(srcdir), action)
                uri         = base_uri + os.path.basename(action)
                command     = ([serdi, '-a', '-o', osyntax] +
                               ['-I', uri] +
                               options +
                               flatten_options(next(extra_options_iter)) +
                               [rel_action])

                # Run strict test
                if expected_return == 0:
                    result = check(command, stdout=action + '.out', name=action)
                else:
                    result = check(command,
                                   stdout=action + '.out',
                                   stderr=autowaf.NONEMPTY,
                                   expected=expected_return,
                                   name=action)

                if (result and expected_return == 0 and
                    ((mf + 'result') in model[test])):
                    # Check output against test suite
                    check_uri  = model[test][mf + 'result'][0]
                    check_path = ctx.src_path(file_uri_to_path(check_uri))
                    result     = check.file_equals(action + '.out', check_path)

                    # Run round-trip tests
                    if result:
                        test_thru(check, uri, action, check_path,
                                  list(next(thru_options_iter)),
                                  isyntax, osyntax, options)

                # Write test report entry
                if report is not None:
                    report.write(earl_assertion(test, result, asserter))

                if expected_return == 0:
                    # Run model test for positive test (must succeed)
                    model_out_path = action + '.model.out'
                    check([command[0]] + ['-m'] + command[1:], stdout=model_out_path,
                          name=action + ' model')

                    if result and ((mf + 'result') in model[test]):
                        check(lambda: _file_lines_equal(check_path, model_out_path),
                              name=action + ' model check')

    ns_rdftest = 'http://www.w3.org/ns/rdftest#'
    for test_class, instances in instances.items():
        if test_class.startswith(ns_rdftest):
            expected = 1 if '-l' not in options and 'Negative' in test_class else 0
            run_tests(test_class, instances, expected)

def test(tst):
    import tempfile

    # Create test output directories
    for i in ['bad', 'good', 'lax', 'terse',
              'TurtleTests', 'NTriplesTests', 'NQuadsTests', 'TriGTests']:
        try:
            test_dir = os.path.join('tests', i)
            os.makedirs(test_dir)
            for i in glob.glob(test_dir + '/*.*'):
                os.remove(i)
        except:
            pass

    srcdir = tst.path.abspath()

    with tst.group('Unit') as check:
        check(['./base64_test'])
        check(['./cursor_test'])
        check(['./statement_test'])
        check(['./sink_test'])
        check(['./model_test'])
        check(['./nodes_test'])
        check(['./overflow_test'])
        check(['./serd_test'])
        check(['./terse_write_test'])
        check(['./read_chunk_test'])

    def test_syntax_io(check, in_name, check_name, lang):
        in_path = 'tests/good/%s' % in_name
        out_path = in_path + '.io'
        check_path = '%s/tests/good/%s' % (srcdir, check_name)

        check([serdi, '-o', lang, '-I', in_path, '%s/%s' % (srcdir, in_path)],
              stdout=out_path, name=in_name)

        check.file_equals(check_path, out_path)

    with tst.group('ThroughSyntax') as check:
        test_syntax_io(check, 'base.ttl',       'base.ttl',        'turtle')
        test_syntax_io(check, 'qualify-in.ttl', 'qualify-out.ttl', 'turtle')
        test_syntax_io(check, 'pretty.trig',    'pretty.trig',     'trig')

    with tst.group('GoodCommands') as check:
        check([serdi, '%s/serd.ttl' % srcdir], stdout=os.devnull)
        check([serdi, '-v'])
        check([serdi, '-h'])
        check([serdi, '-k', '512', '-s', '<foo> a <#Thingie> .'])
        check([serdi, os.devnull])

        with tempfile.TemporaryFile(mode='r') as stdin:
            check([serdi, '-'], stdin=stdin)

        with tempfile.TemporaryFile(mode='w') as stdout:
            check([serdi, '-o', 'empty', '%s/serd.ttl' % srcdir], stdout=stdout)
            stdout.seek(0, 2) # Seek to end
            check(lambda: stdout.tell() == 0, name='empty output')

    with tst.group('BadCommands', expected=1, stderr=autowaf.NONEMPTY) as check:
        check([serdi])
        check([serdi, '/no/such/file'])
        check([serdi, 'ftp://example.org/unsupported.ttl'])
        check([serdi, '-I'])
        check([serdi, '-c'])
        check([serdi, '-i', 'illegal'])
        check([serdi, '-i', 'turtle'])
        check([serdi, '-i'])
        check([serdi, '-k'])
        check([serdi, '-k', '-1'])
        check([serdi, '-k', str(2**63 - 1)])
        check([serdi, '-k', '1024junk'])
        check([serdi, '-o', 'illegal'])
        check([serdi, '-o'])
        check([serdi, '-p'])
        check([serdi, '-q', '%s/tests/bad/bad-base.ttl' % srcdir], stderr=None)
        check([serdi, '-r'])
        check([serdi, '-s'])
        check([serdi, '-z'])
        check([serdi] + ['%s/tests/bad/bad-base.ttl' % srcdir] * 2)

    with tst.group('IoErrors', expected=1) as check:
        check([serdi, '-e', 'file://%s/' % srcdir], name='Read directory')
        check([serdi, 'file://%s/' % srcdir], name='Bulk read directory')
        if os.path.exists('/dev/full'):
            check([serdi, 'file://%s/tests/good/base.ttl' % srcdir],
                  stdout='/dev/full', name='Short write error')
            check([serdi, 'file://%s/tests/good/manifest.ttl' % srcdir],
                  stdout='/dev/full', name='Long write error')

    if sys.version_info.major >= 3:
        from waflib.extras import autoship
        try:
            import rdflib
            with tst.group('NEWS') as check:
                news_path = os.path.join(srcdir, 'NEWS')
                entries = autoship.read_news(top=srcdir)
                autoship.write_news(entries, 'NEWS.norm')
                check.file_equals(news_path, 'NEWS.norm')

                meta_path = os.path.join(srcdir, 'serd.ttl')
                autoship.write_news(entries, 'NEWS.ttl',
                                    format='turtle', template=meta_path)

                ttl_entries = autoship.read_news('NEWS.ttl',
                                                 top=srcdir, format='turtle')

                autoship.write_news(ttl_entries, 'NEWS.round')
                check.file_equals(news_path, 'NEWS.round')
        except ImportError:
            Logs.warn('Failed to import rdflib, not running NEWS tests')

    # Serd-specific test suites
    serd_base = 'http://drobilla.net/sw/serd/tests/'
    test_suite(tst, serd_base + 'good/', 'good', None, 'Turtle')
    test_suite(tst, serd_base + 'bad/', 'bad', None, 'Turtle')
    test_suite(tst, serd_base + 'lax/', 'lax', None, 'Turtle', ['-l'])
    test_suite(tst, serd_base + 'lax/', 'lax', None, 'Turtle')
    test_suite(tst, serd_base + 'terse/', 'terse', None, 'Turtle', ['-t'],
               output_syntax='Turtle')

    # Standard test suites
    with open('earl.ttl', 'w') as report:
        report.write('@prefix earl: <http://www.w3.org/ns/earl#> .\n'
                     '@prefix dc: <http://purl.org/dc/elements/1.1/> .\n')

        with open(os.path.join(srcdir, 'serd.ttl')) as serd_ttl:
            report.writelines(serd_ttl)

        w3c_base = 'http://www.w3.org/2013/'
        test_suite(tst, w3c_base + 'TurtleTests/',
                   'TurtleTests', report, 'Turtle')
        test_suite(tst, w3c_base + 'NTriplesTests/',
                   'NTriplesTests', report, 'NTriples')
        test_suite(tst, w3c_base + 'NQuadsTests/',
                   'NQuadsTests', report, 'NQuads')
        test_suite(tst, w3c_base + 'TriGTests/',
                   'TriGTests', report, 'Trig')
