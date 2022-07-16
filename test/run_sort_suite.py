#!/usr/bin/env python3

# Copyright 2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: ISC

"""Run an RDF test suite with serd-sort."""

import serd_test_util

import argparse
import datetime
import difflib
import itertools
import os
import re
import shlex
import subprocess
import sys
import tempfile
import urllib.parse


def log_error(message):
    """Log an error message to stderr"""

    sys.stderr.write("error: ")
    sys.stderr.write(message)


def _uri_path(uri):
    path = urllib.parse.urlparse(uri).path
    drive = os.path.splitdrive(path[1:])[0]
    return path if not drive else path[1:]


def _file_lines_equal(patha, pathb, subst_from="", subst_to=""):
    import io

    for path in (patha, pathb):
        if not os.access(path, os.F_OK):
            sys.stderr.write("error: missing file %s" % path)
            return False

    la = sorted(set(io.open(patha, encoding="utf-8").readlines()))
    lb = sorted(set(io.open(pathb, encoding="utf-8").readlines()))
    if la != lb:
        serd_test_util.show_diff(la, lb, patha, pathb)
        return False

    return True


def _add_extension(filename, extension):
    first_dot = filename.find(".")

    return filename[0:first_dot] + extension + filename[first_dot:]


def test_suite(
    manifest_path,
    base_uri,
    report_filename,
    input_syntax,
    output_syntax,
    command_prefix,
    out_test_dir,
):
    """Run all tests in a test suite manifest."""

    mf = "http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#"
    test_dir = os.path.dirname(manifest_path)
    model, instances = serd_test_util.load_rdf(
        command_prefix + ["-B", base_uri], manifest_path
    )

    asserter = ""
    if os.getenv("USER") == "drobilla":
        asserter = "http://drobilla.net/drobilla#me"

    class Results:
        def __init__(self):
            self.n_tests = 0
            self.n_failures = 0

    def run_tests(test_class, tests, expected_return, results):
        osyntax = output_syntax
        if osyntax is None:
            osyntax = serd_test_util.test_output_syntax(test_class)

        isyntax = input_syntax
        if isyntax is None:
            isyntax = serd_test_util.test_input_syntax(test_class)

        for test in sorted(tests):
            test_uri = model[test][mf + "action"][0]
            test_uri_path = _uri_path(test_uri)
            test_name = os.path.basename(test_uri_path)
            test_path = os.path.join(test_dir, test_name)

            command = command_prefix + [
                "-B",
                test_uri,
                "-O",
                osyntax,
                "-O",
                "ascii",
                test_path,
            ]

            command_string = " ".join(shlex.quote(c) for c in command)
            out_filename = os.path.join(
                out_test_dir, _add_extension(test_name, ".sort")
            )

            results.n_tests += 1

            if expected_return == 0:  # Positive test

                with open(out_filename, "w") as stdout:
                    proc = subprocess.run(command, check=False, stdout=stdout)
                    passed = proc.returncode == expected_return
                    if not passed:
                        results.n_failures += 1
                        log_error(
                            "Unexpected failure of command: {}\n".format(
                                command_string
                            )
                        )

                if passed and (mf + "result") in model[test]:
                    # Check output against expected output from test suite
                    check_uri = model[test][mf + "result"][0]
                    check_filename = os.path.basename(_uri_path(check_uri))
                    check_path = os.path.join(test_dir, check_filename)

                    if not _file_lines_equal(check_path, out_filename):
                        results.n_failures += 1
                        log_error(
                            "Output {} differs from {}\n".format(
                                out_filename, check_path
                            )
                        )

            else:  # Negative test

                with tempfile.TemporaryFile() as stderr:
                    with open(out_filename, "w") as stdout:
                        proc = subprocess.run(
                            command, check=False, stdout=stdout, stderr=stderr
                        )

                    passed = proc.returncode != 0
                    if passed:
                        # Check that an error message was printed
                        stderr.seek(0, 2)  # Seek to end
                        if stderr.tell() == 0:  # Empty
                            results.n_failures += 1
                            log_error("No error: {}\n".format(command_string))

                    else:
                        results.n_failures += 1
                        log_error("Should fail: {}\n".format(command_string))

            # Write test report entry
            if report_filename:
                with open(report_filename, "a") as report:
                    report.write(
                        serd_test_util.earl_assertion(test, passed, asserter)
                    )

    # Run all test types in the test suite
    results = Results()
    ns_rdftest = "http://www.w3.org/ns/rdftest#"
    for test_class, instances in instances.items():
        if test_class.startswith(ns_rdftest):
            expected = (
                1
                if "lax" not in command_prefix and "Negative" in test_class
                else 0
            )
            run_tests(test_class, instances, expected, results)

    # Print result summary
    if results.n_failures > 0:
        log_error(
            "{}/{} tests failed\n".format(results.n_failures, results.n_tests)
        )
    else:
        sys.stdout.write("All {} tests passed\n".format(results.n_tests))

    return results.n_failures


def main():
    """Run the command line tool."""

    parser = argparse.ArgumentParser(
        usage="%(prog)s [OPTION]... MANIFEST BASE_URI -- [TOOL_OPTION]...",
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )

    parser.add_argument("--report", help="path to write result report to")
    parser.add_argument("--tool", default="tools/serd-sort", help="executable")
    parser.add_argument("--syntax", default=None, help="input syntax")
    parser.add_argument("--osyntax", default=None, help="output syntax")
    parser.add_argument("--wrapper", default="", help="executable wrapper")
    parser.add_argument("manifest", help="test suite manifest.ttl file")
    parser.add_argument("base_uri", help="base URI for tests")
    parser.add_argument(
        "tool_option", nargs=argparse.REMAINDER, help="option to pass to tool"
    )

    args = parser.parse_args(sys.argv[1:])
    command_prefix = shlex.split(args.wrapper) + [args.tool] + args.tool_option

    with tempfile.TemporaryDirectory() as test_out_dir:
        return test_suite(
            args.manifest,
            args.base_uri,
            args.report,
            args.syntax,
            args.osyntax,
            command_prefix,
            test_out_dir,
        )


if __name__ == "__main__":
    try:
        sys.exit(main())
    except subprocess.CalledProcessError as e:
        if e.stderr is not None:
            sys.stderr.write(e.stderr.decode("utf-8"))

        sys.stderr.write("error: %s\n" % e)
        sys.exit(e.returncode)
