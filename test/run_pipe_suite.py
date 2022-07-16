#!/usr/bin/env python3

# Copyright 2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: ISC

"""Run an RDF test suite with serd-pipe."""

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


def test_osyntax_options(osyntax):
    if osyntax.lower() == "ntriples" or osyntax.lower() == "nquads":
        return ["-O", "ascii"]

    return []


def test_thru(
    base_uri,
    path,
    check_lines,
    check_path,
    out_test_dir,
    flags,
    isyntax,
    osyntax,
    command_prefix,
):
    """Test rewriting a file in the input syntax.

    This rewrites a source test file in the original fancy syntax, then
    rewrites that output again in the simple syntax used for test output
    (NTriples or NQuads).  Checking the final output against the expected test
    output tests that piping the file through serd with pretty-printing was
    lossless.
    """

    assert isyntax is not None
    assert osyntax is not None

    test_name = os.path.basename(path)
    out_path = os.path.join(out_test_dir, test_name + ".pass")
    thru_path = os.path.join(out_test_dir, test_name + ".thru")

    out_cmd = (
        command_prefix
        + [f for sublist in flags for f in sublist]
        + [
            "-B",
            base_uri,
            "-I",
            isyntax,
            "-O",
            isyntax,
            "-o",
            out_path,
            path,
        ]
    )

    subprocess.run(out_cmd, check=True)

    thru_cmd = (
        command_prefix
        + test_osyntax_options(osyntax)
        + [
            "-B",
            base_uri,
            "-I",
            isyntax,
            "-O",
            "ascii",
            "-O",
            osyntax,
            out_path,
        ]
    )

    proc = subprocess.run(
        thru_cmd, check=True, capture_output=True, encoding="utf-8"
    )

    if not serd_test_util.lines_equal(
        check_lines,
        proc.stdout.splitlines(True),
        check_path,
        thru_path,
    ):
        log_error("Rewritten {} is different\n".format(check_path))
        return 1

    return 0


def _uri_path(uri):
    path = urllib.parse.urlparse(uri).path
    drive = os.path.splitdrive(path[1:])[0]
    return path if not drive else path[1:]


def _option_combinations(options):
    """Return an iterator that cycles through all combinations of options."""

    combinations = []
    for count in range(len(options) + 1):
        combinations += list(itertools.combinations(options, count))

    return itertools.cycle(combinations)


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
        thru_flags = [
            ["-R", "http://example.org/"],
            ["-b", "1"],
            ["-b", "16384"],
        ]
        thru_options_iter = _option_combinations(thru_flags)
        if output_syntax is not None:
            osyntax = output_syntax
        else:
            osyntax = serd_test_util.test_output_syntax(test_class)

        if input_syntax is not None:
            isyntax = input_syntax
        else:
            isyntax = serd_test_util.test_input_syntax(test_class)

        for test in sorted(tests):
            test_uri = model[test][mf + "action"][0]
            test_uri_path = _uri_path(test_uri)
            test_name = os.path.basename(test_uri_path)
            test_path = os.path.join(test_dir, test_name)

            command = command_prefix + [
                "-O",
                osyntax,
                "-O",
                "ascii",
                "-B",
                test_uri,
                test_path,
            ]

            command_string = " ".join(shlex.quote(c) for c in command)
            out_filename = os.path.join(out_test_dir, test_name + ".out")

            results.n_tests += 1

            if expected_return == 0:  # Positive test
                with tempfile.TemporaryFile("w+", encoding="utf-8") as out:
                    proc = subprocess.run(command, check=False, stdout=out)
                    if proc.returncode == 0:
                        passed = True
                    else:
                        results.n_failures += 1
                        log_error(
                            "Unexpected failure of command: {}\n".format(
                                command_string
                            )
                        )

                    if proc.returncode == 0 and mf + "result" in model[test]:
                        # Check output against expected output from test suite
                        check_uri = model[test][mf + "result"][0]
                        check_filename = os.path.basename(_uri_path(check_uri))
                        check_path = os.path.join(test_dir, check_filename)

                        with open(check_path, "r", encoding="utf-8") as check:
                            check_lines = check.readlines()

                            out.seek(0)
                            if not serd_test_util.lines_equal(
                                check_lines,
                                out.readlines(),
                                check_path,
                                out_filename,
                            ):
                                results.n_failures += 1
                                log_error(
                                    "Output {} does not match {}\n".format(
                                        out_filename, check_path
                                    )
                                )

                            # Run round-trip test
                            results.n_failures += test_thru(
                                test_uri,
                                test_path,
                                check_lines,
                                check_path,
                                out_test_dir,
                                list(next(thru_options_iter)),
                                isyntax,
                                osyntax,
                                command_prefix,
                            )

            else:  # Negative test
                with tempfile.TemporaryFile() as stderr:
                    proc = subprocess.run(
                        command,
                        check=False,
                        stdout=subprocess.DEVNULL,
                        stderr=stderr,
                    )

                    if proc.returncode != 0:
                        passed = True
                    else:
                        results.n_failures += 1
                        log_error(
                            "Unexpected success of command: {}\n".format(
                                command_string
                            )
                        )

                    # Check that an error message was printed
                    stderr.seek(0, 2)  # Seek to end
                    if stderr.tell() == 0:  # Empty
                        results.n_failures += 1
                        log_error(
                            "No error message printed by: {}\n".format(
                                command_string
                            )
                        )

            # Write test report entry
            if report_filename:
                with open(report_filename, "a", encoding="utf-8") as report:
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
    parser.add_argument("--tool", default="tools/serd-pipe", help="executable")
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
            sys.stderr.write(e.stderr)

        sys.stderr.write("error: %s\n" % e)
        sys.exit(e.returncode)
