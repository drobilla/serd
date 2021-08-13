#!/usr/bin/env python3

# Copyright 2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: ISC

"""Run the RDF-based test suite for serd-filter."""

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


def _uri_path(test_dir, uri):
    path = urllib.parse.urlparse(uri).path
    drive = os.path.splitdrive(path[1:])[0]
    path = path if not drive else path[1:]
    return os.path.join(test_dir, os.path.basename(path))


def test_suite(
    manifest_path,
    base_uri,
    command_prefix,
    out_dir,
):
    """Run all tests in the manifest."""

    mf = "http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#"
    suite_dir = os.path.dirname(manifest_path)

    model, instances = serd_test_util.load_rdf(
        command_prefix + ["-B", base_uri], manifest_path
    )

    os.makedirs(out_dir, exist_ok=True)

    class Results:
        def __init__(self):
            self.n_tests = 0
            self.n_failures = 0

    def run_test(entry, results):
        """Run a single test entry from the manifest."""

        input_uri = model[entry][mf + "action"][0]
        input_path = _uri_path(suite_dir, input_uri)

        output_path = os.path.join(out_dir, os.path.basename(input_path))

        command = command_prefix + [
            "-B",
            base_uri,
            "-O",
            "turtle",
            "-o",
            output_path,
            input_path,
        ]

        # Read the flat input and pretty-print output
        results.n_tests += 1
        try:
            subprocess.run(command, check=True)

            # Check that the output is exactly the same as the expected result
            if not serd_test_util.file_equals(input_path, output_path):
                results.n_failures += 1
                log_error(
                    "Output {} differs from {}\n".format(
                        output_path, input_path
                    )
                )

        except Exception as e:
            log_error(str(e))
            results.n_failures += 1

    # Run all test types in the test suite
    results = Results()
    for klass, instances in instances.items():
        if klass == "http://drobilla.net/ns/serd#TestTurtlePrint":
            for entry in instances:
                run_test(entry, results)

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
        usage="%(prog)s [OPTION]... MANIFEST -- [TOOL_OPTION]...",
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )

    parser.add_argument("--tool", default="tools/serd-sort", help="executable")
    parser.add_argument("--wrapper", default="", help="executable wrapper")

    parser.add_argument(
        "-o", "--out-dir", default=None, help="output directory"
    )

    parser.add_argument(
        "--base-uri",
        default="http://drobilla.net/sw/serd/test/",
        help="base URI",
    )
    parser.add_argument("manifest", help="test suite manifest.ttl file")

    args = parser.parse_args(sys.argv[1:])
    wrapper_prefix = shlex.split(args.wrapper)
    command_prefix = wrapper_prefix + [args.tool]

    if args.out_dir is not None:
        return test_suite(
            args.manifest, args.base_uri, command_prefix, args.out_dir
        )

    with tempfile.TemporaryDirectory() as out_dir:
        return test_suite(args.manifest, args.base_uri, command_prefix, out_dir)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except subprocess.CalledProcessError as e:
        if e.stderr is not None:
            sys.stderr.write(e.stderr.decode("utf-8"))

        sys.stderr.write("error: %s\n" % e)
        sys.exit(e.returncode)
