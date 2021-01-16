#!/usr/bin/env python

import glob
import os
import sys

from waflib import Build, Logs, Options
from waflib.extras import autowaf

# Library and package version (UNIX style major, minor, micro)
# major increment <=> incompatible changes
# minor increment <=> compatible changes (additions)
# micro increment <=> no interface changes
SERD_VERSION       = '0.30.9'
SERD_MAJOR_VERSION = '0'

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
    ctx.add_flags(
        ctx.configuration_options(),
        {'no-utils':     'do not build command line utilities',
         'stack-check':  'include runtime stack sanity checks',
         'static':       'build static library',
         'no-shared':    'do not build shared library',
         'static-progs': 'build programs as static binaries',
         'largefile':    'build with large file support on 32-bit systems',
         'no-posix':     'do not use POSIX functions, even if present'})


def configure(conf):
    conf.load('compiler_c', cache=True)
    conf.load('autowaf', cache=True)

    if conf.env.DOCS:
        conf.load('sphinx')

    if not autowaf.set_c_lang(conf, 'c11', mandatory=False):
        autowaf.set_c_lang(conf, 'c99')

    if Options.options.strict:
        # Check for programs used by lint target
        conf.find_program("flake8", var="FLAKE8", mandatory=False)
        conf.find_program("clang-tidy", var="CLANG_TIDY", mandatory=False)
        conf.find_program("iwyu_tool", var="IWYU_TOOL", mandatory=False)

    if Options.options.ultra_strict:
        autowaf.add_compiler_flags(conf.env, '*', {
            'clang': [
                '-Wno-cast-align',
                '-Wno-cast-qual',
                '-Wno-conversion',
                '-Wno-covered-switch-default',
                '-Wno-disabled-macro-expansion',
                '-Wno-double-promotion',
                '-Wno-format-nonliteral',
                '-Wno-implicit-fallthrough',
                '-Wno-nullability-extension',
                '-Wno-nullable-to-nonnull-conversion',
                '-Wno-padded',
                '-Wno-reserved-id-macro',
                '-Wno-sign-conversion',
            ],
            'gcc': [
                '-Wno-cast-align',
                '-Wno-cast-qual',
                '-Wno-float-conversion',
                '-Wno-inline',
                '-Wno-padded',
                '-Wno-sign-conversion',
            ],
            'msvc': [
                '/wd4061',  # enumerator in switch is not explicitly handled
                '/wd4365',  # signed/unsigned mismatch
                '/wd4514',  # unreferenced inline function has been removed
                '/wd4710',  # function not inlined
                '/wd4711',  # function selected for automatic inline expansion
                '/wd4820',  # padding added after construct
                '/wd4996',  # POSIX name for this item is deprecated
            ],
        })

        autowaf.add_compiler_flags(conf.env, 'c', {
            'clang': [
                '-Wno-bad-function-cast',
            ],
            'gcc': [
                '-Wno-bad-function-cast',
            ],
            'msvc': [
                '/wd4706',  # assignment within conditional expression
                '/wd5045',  # will insert Spectre mitigation for memory load
            ],
        })

        if 'mingw' in conf.env.CC[0]:
            conf.env.append_value('CFLAGS', '-Wno-unused-macros')

    conf.env.update({
        'BUILD_UTILS': not Options.options.no_utils,
        'BUILD_SHARED': not Options.options.no_shared,
        'STATIC_PROGS': Options.options.static_progs,
        'BUILD_STATIC': (Options.options.static or
                         Options.options.static_progs)})

    if not conf.env.BUILD_SHARED and not conf.env.BUILD_STATIC:
        conf.fatal('Neither a shared nor a static build requested')

    if Options.options.stack_check:
        conf.define('SERD_STACK_CHECK', SERD_VERSION)

    if Options.options.largefile:
        conf.env.append_unique('DEFINES', ['_FILE_OFFSET_BITS=64'])

    conf.check_function('c', 'aligned_alloc',
                        header_name = 'stdlib.h',
                        return_type = 'void*',
                        arg_types   = 'size_t,size_t',
                        define_name = 'HAVE_ALIGNED_ALLOC',
                        mandatory   = False)

    if not Options.options.no_posix:
        funcs = {'posix_memalign': ('stdlib.h', 'int', 'void**,size_t,size_t'),
                 'posix_fadvise':  ('fcntl.h', 'int', 'int,off_t,off_t,int'),
                 'fileno':         ('stdio.h', 'int', 'FILE*')}

        for name, (header, ret, args) in funcs.items():
            conf.check_function('c', name,
                                header_name = header,
                                return_type = ret,
                                arg_types   = args,
                                define_name = 'HAVE_' + name.upper(),
                                defines     = ['_POSIX_C_SOURCE=200809L'],
                                mandatory   = False)

    # Set up environment for building/using as a subproject
    autowaf.set_lib_env(conf, 'serd', SERD_VERSION,
                        include_path=str(conf.path.find_node('include')))

    if conf.env.BUILD_TESTS:
        serdi_node = conf.path.get_bld().make_node('serdi_static')
    else:
        serdi_node = conf.path.get_bld().make_node('serdi')

    conf.env.SERDI = [serdi_node.abspath()]

    conf.define('SERD_NO_DEFAULT_CONFIG', 1)

    autowaf.display_summary(
        conf,
        {'Build static library': bool(conf.env['BUILD_STATIC']),
         'Build shared library': bool(conf.env['BUILD_SHARED']),
         'Build utilities':      bool(conf.env['BUILD_UTILS']),
         'Build unit tests':     bool(conf.env['BUILD_TESTS'])})


lib_headers = ['src/reader.h']

lib_source = ['src/base64.c',
              'src/byte_source.c',
              'src/env.c',
              'src/n3.c',
              'src/node.c',
              'src/reader.c',
              'src/string.c',
              'src/system.c',
              'src/uri.c',
              'src/writer.c']


def build(bld):
    # C Headers
    includedir = '${INCLUDEDIR}/serd-%s/serd' % SERD_MAJOR_VERSION
    bld.install_files(includedir, bld.path.ant_glob('include/serd/*.h'))

    # Pkgconfig file
    autowaf.build_pc(bld, 'SERD', SERD_VERSION, SERD_MAJOR_VERSION, [],
                     {'SERD_MAJOR_VERSION': SERD_MAJOR_VERSION})

    defines = []
    lib_args = {'export_includes': ['include'],
                'includes':        ['include'],
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
            defines         = defines + ['SERD_INTERNAL'],
            **lib_args)

    # Static library
    if bld.env.BUILD_STATIC:
        bld(features        = 'c cstlib',
            source          = lib_source,
            name            = 'libserd_static',
            target          = 'serd-%s' % SERD_MAJOR_VERSION,
            defines         = defines + ['SERD_STATIC', 'SERD_INTERNAL'],
            **lib_args)

    if bld.env.BUILD_TESTS:
        coverage_flags = [''] if bld.env.NO_COVERAGE else ['--coverage']
        test_args = {'includes':     ['include'],
                     'cflags':       coverage_flags,
                     'linkflags':    coverage_flags,
                     'lib':          lib_args['lib'],
                     'install_path': ''}

        # Profiled static library for test coverage
        bld(features     = 'c cstlib',
            source       = lib_source,
            name         = 'libserd_profiled',
            target       = 'serd_profiled',
            defines      = defines + ['SERD_STATIC', 'SERD_INTERNAL'],
            **test_args)

        # Test programs
        for prog in [('serdi_static', 'src/serdi.c'),
                     ('test_env', 'test/test_env.c'),
                     ('test_free_null', 'test/test_free_null.c'),
                     ('test_node', 'test/test_node.c'),
                     ('test_read_chunk', 'test/test_read_chunk.c'),
                     ('test_reader_writer', 'test/test_reader_writer.c'),
                     ('test_string', 'test/test_string.c'),
                     ('test_uri', 'test/test_uri.c')]:
            bld(features     = 'c cprogram',
                source       = prog[1],
                use          = 'libserd_profiled',
                target       = prog[0],
                defines      = defines + ['SERD_STATIC'],
                **test_args)

    # Utilities
    if bld.env.BUILD_UTILS:
        obj = bld(features     = 'c cprogram',
                  source       = 'src/serdi.c',
                  target       = 'serdi',
                  includes     = ['include'],
                  use          = 'libserd',
                  lib          = lib_args['lib'],
                  install_path = '${BINDIR}')
        if not bld.env.BUILD_SHARED or bld.env.STATIC_PROGS:
            obj.use = 'libserd_static'
        if bld.env.STATIC_PROGS:
            obj.env.SHLIB_MARKER = obj.env.STLIB_MARKER
            obj.linkflags        = ['-static']

    # Documentation
    if bld.env.DOCS:
        bld.env.SERD_MAJOR_VERSION = SERD_MAJOR_VERSION
        bld.recurse('doc/c')

    # Man page
    bld.install_files('${MANDIR}/man1', 'doc/serdi.1')

    bld.add_post_fun(autowaf.run_ldconfig)


class LintContext(Build.BuildContext):
    fun = cmd = 'lint'


def lint(ctx):
    "checks code for style issues"
    import subprocess

    st = 0

    if "FLAKE8" in ctx.env:
        Logs.info("Running flake8")
        st = subprocess.call([ctx.env.FLAKE8[0],
                              "wscript",
                              "--ignore",
                              "E101,E129,W191,E221,W504,E251,E241,E741"])
        st += subprocess.call([ctx.env.FLAKE8[0],
                               "scripts/serd_bench.py",
                               "--ignore",
                               "E203"])
    else:
        Logs.warn("Not running flake8")

    if "IWYU_TOOL" in ctx.env:
        Logs.info("Running include-what-you-use")
        cmd = [ctx.env.IWYU_TOOL[0], "-o", "clang", "-p", "build"]
        output = subprocess.check_output(cmd).decode('utf-8')
        if 'error: ' in output:
            sys.stdout.write(output)
            st += 1
    else:
        Logs.warn("Not running include-what-you-use")

    if "CLANG_TIDY" in ctx.env and "clang" in ctx.env.CC[0]:
        Logs.info("Running clang-tidy")
        sources = glob.glob('include/serd/*.h*')
        sources += glob.glob('src/*.c')
        sources += glob.glob('test/*.c')
        sources = list(map(os.path.abspath, sources))
        procs = []
        for source in sources:
            cmd = [ctx.env.CLANG_TIDY[0], "--quiet", "-p=.", source]
            procs += [subprocess.Popen(cmd, cwd="build")]

        for proc in procs:
            stdout, stderr = proc.communicate()
            st += proc.returncode
    else:
        Logs.warn("Not running clang-tidy")

    if st != 0:
        sys.exit(st)


def amalgamate(ctx):
    "builds single-file amalgamated source"
    import shutil
    import re
    shutil.copy('serd/serd.h', 'build/serd.h')

    def include_line(line):
        return (not re.match(r'#include "[^/]*\.h"', line) and
                not re.match('#include "serd/serd.h"', line))

    with open('build/serd.c', 'w') as amalgamation:
        amalgamation.write('/* This is amalgamated code, do not edit! */\n')
        amalgamation.write('#include "serd.h"\n\n')

        for header_path in ['src/serd_internal.h',
                            'src/system.h',
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


def test_thru(check, base, path, check_path, flags, isyntax, osyntax, opts=[]):
    out_path = path + '.pass'
    out_cmd = [serdi] + opts + [f for sublist in flags for f in sublist] + [
        '-i', isyntax,
        '-o', isyntax,
        '-p', 'foo',
        check.tst.src_path(path), base]

    thru_path = path + '.thru'
    thru_cmd = [serdi] + opts + [
        '-i', isyntax,
        '-o', osyntax,
        '-c', 'foo',
        out_path,
        base]

    return (check(out_cmd, stdout=out_path, verbosity=0, name=out_path) and
            check(thru_cmd, stdout=thru_path, verbosity=0, name=thru_path) and
            check.file_equals(check_path, thru_path, verbosity=0))


def file_uri_to_path(uri):
    try:
        from urlparse import urlparse  # Python 2
    except ImportError:
        from urllib.parse import urlparse  # Python 3

    path  = urlparse(uri).path
    drive = os.path.splitdrive(path[1:])[0]
    return path if not drive else path[1:]


def _test_output_syntax(test_class):
    if 'NTriples' in test_class or 'Turtle' in test_class:
        return 'NTriples'
    elif 'NQuads' in test_class or 'Trig' in test_class:
        return 'NQuads'
    raise Exception('Unknown test class <%s>' % test_class)


def _wrapped_command(cmd):
    if Options.options.wrapper:
        import shlex
        return shlex.split(Options.options.wrapper) + cmd

    return cmd


def _load_rdf(filename):
    "Load an RDF file into python dictionaries via serdi.  Only supports URIs."
    import subprocess
    import re

    rdf_type = 'http://www.w3.org/1999/02/22-rdf-syntax-ns#type'
    model = {}
    instances = {}

    cmd = _wrapped_command(['./serdi_static', filename])
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    for line in proc.communicate()[0].splitlines():
        matches = re.match(r'<([^ ]*)> <([^ ]*)> <([^ ]*)> \.',
                           line.decode('utf-8'))
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


def _option_combinations(options):
    "Return an iterator that cycles through all combinations of options"
    import itertools

    combinations = []
    for n in range(len(options) + 1):
        combinations += list(itertools.combinations(options, n))

    return itertools.cycle(combinations)


def test_suite(ctx, base_uri, testdir, report, isyntax, options=[]):
    srcdir = ctx.path.abspath()

    mf = 'http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#'
    manifest_path = os.path.join(srcdir, 'test', testdir, 'manifest.ttl')
    model, instances = _load_rdf(manifest_path)

    asserter = ''
    if os.getenv('USER') == 'drobilla':
        asserter = 'http://drobilla.net/drobilla#me'

    def run_tests(test_class, tests, expected_return):
        thru_flags = [['-e'], ['-f'], ['-b'], ['-r', 'http://example.org/']]
        osyntax = _test_output_syntax(test_class)
        thru_options_iter = _option_combinations(thru_flags)
        tests_name = '%s.%s' % (testdir, test_class[test_class.find('#') + 1:])
        with ctx.group(tests_name) as check:
            for test in sorted(tests):
                action_node = model[test][mf + 'action'][0]
                basename    = os.path.basename(action_node)
                action      = os.path.join('test', testdir, basename)
                rel_action  = os.path.join(os.path.relpath(srcdir), action)
                uri         = base_uri + os.path.basename(action)
                command     = [serdi] + options + ['-f', rel_action, uri]

                # Run strict test
                if expected_return == 0:
                    result = check(command,
                                   stdout=action + '.out',
                                   name=action)
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

    ns_rdftest = 'http://www.w3.org/ns/rdftest#'
    for test_class, instances in instances.items():
        if test_class.startswith(ns_rdftest):
            expected = (1 if '-l' not in options and 'Negative' in test_class
                        else 0)
            run_tests(test_class, instances, expected)


def test(tst):
    import tempfile

    # Create test output directories
    for i in ['bad', 'good', 'lax',
              'TurtleTests', 'NTriplesTests', 'NQuadsTests', 'TriGTests']:
        try:
            test_dir = os.path.join('test', i)
            os.makedirs(test_dir)
            for i in glob.glob(test_dir + '/*.*'):
                os.remove(i)
        except Exception:
            pass

    srcdir = tst.path.abspath()

    with tst.group('Unit') as check:
        check(['./test_env'])
        check(['./test_free_null'])
        check(['./test_node'])
        check(['./test_read_chunk'])
        check(['./test_reader_writer'])
        check(['./test_string'])
        check(['./test_uri'])

    def test_syntax_io(check, in_name, check_name, lang):
        in_path = 'test/good/%s' % in_name
        out_path = in_path + '.io'
        check_path = '%s/test/good/%s' % (srcdir, check_name)

        check([serdi, '-o', lang, '%s/%s' % (srcdir, in_path), in_path],
              stdout=out_path, name=in_name)

        check.file_equals(check_path, out_path)

    with tst.group('ThroughSyntax') as check:
        test_syntax_io(check, 'base.ttl',       'base.ttl',        'turtle')
        test_syntax_io(check, 'qualify-in.ttl', 'qualify-out.ttl', 'turtle')

    with tst.group('GoodCommands') as check:
        check([serdi, '%s/serd.ttl' % srcdir], stdout=os.devnull)
        check([serdi, '-v'])
        check([serdi, '-h'])
        check([serdi, '-s', '<urn:eg:s> a <urn:eg:T> .'])
        check([serdi, os.devnull])
        with tempfile.TemporaryFile(mode='r') as stdin:
            check([serdi, '-'], stdin=stdin)

    with tst.group('BadCommands',
                   expected=1,
                   stderr=autowaf.NONEMPTY) as check:
        check([serdi])
        check([serdi, '/no/such/file'])
        check([serdi, 'ftp://example.org/unsupported.ttl'])
        check([serdi, '-c'])
        check([serdi, '-i', 'illegal'])
        check([serdi, '-i', 'turtle'])
        check([serdi, '-i'])
        check([serdi, '-o', 'illegal'])
        check([serdi, '-o'])
        check([serdi, '-p'])
        check([serdi, '-q', '%s/test/bad/bad-base.ttl' % srcdir], stderr=None)
        check([serdi, '-r'])
        check([serdi, '-z'])
        check([serdi, '-s', '<foo> a <Bar> .'])

    with tst.group('IoErrors', expected=1) as check:
        check([serdi, '-e', 'file://%s/' % srcdir], name='Read directory')
        check([serdi, 'file://%s/' % srcdir], name='Bulk read directory')
        if os.path.exists('/dev/full'):
            check([serdi, 'file://%s/test/good/manifest.ttl' % srcdir],
                  stdout='/dev/full', name='Write error')

    if sys.version_info.major >= 3:
        from waflib.extras import autoship
        try:
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
    serd_base = 'http://drobilla.net/sw/serd/test/'
    test_suite(tst, serd_base + 'good/', 'good', None, 'Turtle')
    test_suite(tst, serd_base + 'bad/', 'bad', None, 'Turtle')
    test_suite(tst, serd_base + 'lax/', 'lax', None, 'Turtle', ['-l'])
    test_suite(tst, serd_base + 'lax/', 'lax', None, 'Turtle')

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
                   'TriGTests', report, 'Trig', ['-a'])
