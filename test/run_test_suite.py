#!/usr/bin/env python3

# Copyright 2022-2023 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: ISC

"""Run an RDF test suite with serdi."""

import argparse
import difflib
import itertools
import os
import shlex
import subprocess
import sys
import tempfile

import serd_test_util


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
    """Test lossless round-tripping through two different syntaxes."""

    assert isyntax is not None
    assert osyntax is not None

    test_name = os.path.basename(path)
    out_path = os.path.join(out_test_dir, test_name + ".pass")
    thru_path = os.path.join(out_test_dir, test_name + ".thru")

    out_cmd = (
        command_prefix
        + [f for sublist in flags for f in sublist]
        + [
            "-i",
            isyntax,
            "-o",
            isyntax,
            "-p",
            "serd_test",
            path,
            base_uri,
        ]
    )

    thru_cmd = command_prefix + [
        "-i",
        isyntax,
        "-o",
        osyntax,
        "-c",
        "serd_test",
        out_path,
        base_uri,
    ]

    with open(out_path, "wb") as out:
        subprocess.run(out_cmd, check=True, stdout=out)

    proc = subprocess.run(
        thru_cmd, check=True, capture_output=True, encoding="utf-8"
    )

    return serd_test_util.lines_equal(
        check_lines,
        proc.stdout.splitlines(True),
        check_path,
        thru_path,
    )


def _test_input_syntax(test_class):
    """Return the output syntax use for a given test class."""

    if "NTriples" in test_class:
        return "NTriples"

    if "Turtle" in test_class:
        return "Turtle"

    if "NQuads" in test_class:
        return "NQuads"

    if "Trig" in test_class:
        return "Trig"

    raise Exception("Unknown test class <{}>".format(test_class))


def _test_output_syntax(test_class):
    """Return the output syntax use for a given test class."""

    if "NTriples" in test_class or "Turtle" in test_class:
        return "NTriples"

    if "NQuads" in test_class or "Trig" in test_class:
        return "NQuads"

    raise Exception("Unknown test class <{}>".format(test_class))


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
    command_prefix,
    out_test_dir,
):
    """Run all tests in a test suite manifest."""

    mf = "http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#"
    test_dir = os.path.dirname(manifest_path)
    model, instances = serd_test_util.load_rdf(
        manifest_path, base_uri, command_prefix
    )

    asserter = ""
    if os.getenv("USER") == "drobilla":
        asserter = "http://drobilla.net/drobilla#me"

    def run_tests(test_class, tests, expected_return, results):
        thru_flags = [["-e"], ["-f"], ["-b"], ["-r", "http://example.org/"]]
        osyntax = _test_output_syntax(test_class)
        thru_options_iter = _option_combinations(thru_flags)

        if input_syntax is not None:
            isyntax = input_syntax
        else:
            isyntax = _test_input_syntax(test_class)

        for test in sorted(tests):
            test_uri = model[test][mf + "action"][0]
            test_uri_path = serd_test_util.uri_path(test_uri)
            test_name = os.path.basename(test_uri_path)
            test_path = os.path.join(test_dir, test_name)

            command = command_prefix + ["-f", test_path, test_uri]
            command_string = " ".join(shlex.quote(c) for c in command)
            out_filename = os.path.join(out_test_dir, test_name + ".out")

            results.n_tests += 1

            if expected_return == 0:  # Positive test
                with tempfile.TemporaryFile("w+", encoding="utf-8") as out:
                    proc = subprocess.run(command, check=False, stdout=out)
                    if proc.returncode == 0:
                        results.test_passed()
                        passed = True
                    else:
                        results.test_failed()
                        serd_test_util.error(
                            "Unexpected failure of command: {}\n".format(
                                command_string
                            )
                        )

                    if proc.returncode == 0 and mf + "result" in model[test]:
                        # Check output against expected output from test suite
                        check_uri = model[test][mf + "result"][0]
                        check_filename = os.path.basename(
                            serd_test_util.uri_path(check_uri)
                        )
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
                                results.test_failed()
                                serd_test_util.error(
                                    "Output {} does not match {}\n".format(
                                        out_filename, check_path
                                    )
                                )

                            # Run round-trip test
                            check.seek(0)
                            results.check(
                                test_thru(
                                    test_uri,
                                    test_path,
                                    check_lines,
                                    check_path,
                                    out_test_dir,
                                    list(next(thru_options_iter)),
                                    isyntax,
                                    osyntax,
                                    command_prefix,
                                ),
                                "Corrupted round-trip of {}\n".format(
                                    test_uri
                                ),
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
                        serd_test_util.error(
                            "Unexpected success of command: {}\n".format(
                                command_string
                            )
                        )

                    # Check that an error message was printed
                    stderr.seek(0, 2)  # Seek to end
                    if stderr.tell() == 0:  # Empty
                        results.n_failures += 1
                        serd_test_util.error(
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
    results = serd_test_util.Results()
    ns_rdftest = "http://www.w3.org/ns/rdftest#"
    for test_class, instances in instances.items():
        if test_class.startswith(ns_rdftest):
            expected = (
                1
                if "-l" not in command_prefix and "Negative" in test_class
                else 0
            )
            run_tests(test_class, instances, expected, results)

    return serd_test_util.print_result_summary(results)


def main():
    """Run the command line tool."""

    parser = argparse.ArgumentParser(
        usage="%(prog)s [OPTION]... MANIFEST BASE_URI -- [SERDI_OPTION]...",
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )

    parser.add_argument("--report", help="path to write result report to")
    parser.add_argument("--serdi", default="serdi", help="path to serdi")
    parser.add_argument("--syntax", default=None, help="input syntax")
    parser.add_argument("--wrapper", default="", help="executable wrapper")
    parser.add_argument("manifest", help="test suite manifest.ttl file")
    parser.add_argument("base_uri", help="base URI for tests")
    parser.add_argument(
        "serdi_option", nargs=argparse.REMAINDER, help="option for serdi"
    )

    args = parser.parse_args(sys.argv[1:])
    command_prefix = (
        shlex.split(args.wrapper) + [args.serdi] + args.serdi_option
    )

    with tempfile.TemporaryDirectory() as test_out_dir:
        return test_suite(
            args.manifest,
            args.base_uri,
            args.report,
            args.syntax,
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
