#!/usr/bin/env python3

# Copyright 2022-2023 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: ISC

"""Run the RDF-based test suite for serd-filter."""

# pylint: disable=duplicate-code

import argparse
import os
import shlex
import subprocess
import sys
import tempfile

import serd_test_util as util

NS_MF = "http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#"
NS_RDFT = "http://www.w3.org/ns/rdftest#"
NS_SERD = "http://drobilla.net/ns/serd#"


def run_entry(entry, filter_command, out_dir, suite_dir):
    """Run a single test entry from the manifest."""

    pattern_path = util.file_path(suite_dir, entry[NS_SERD + "patternFile"][0])
    in_path = util.file_path(suite_dir, entry[NS_MF + "action"][0])
    good_path = util.file_path(suite_dir, entry[NS_MF + "result"][0])
    out_path = os.path.join(out_dir, os.path.basename(good_path))

    # Run the command to write the output file
    options = ["-f", pattern_path, "-o", out_path]
    command = filter_command + options + [in_path]
    subprocess.run(command, check=True)

    # Check that the filtered output matches the expected result
    return util.file_equals(good_path, out_path)


def run_suite(manifest_path, base_uri, filter_command, pipe_command, out_dir):
    """Run all tests in the manifest."""

    # Load manifest model
    suite_dir = os.path.dirname(manifest_path)
    load_command = pipe_command + ["-B", base_uri]
    filter_command = filter_command + ["-B", base_uri]
    model, instances = util.load_rdf(load_command, manifest_path)

    # Run all filter tests in the test suite
    results = util.Results()
    for klass, instances in instances.items():
        if klass == NS_SERD + "TestFilterPositive":
            test_filter_command = filter_command
        elif klass == NS_SERD + "TestFilterNegative":
            test_filter_command = filter_command + ["-v"]
        else:
            continue

        for instance in instances:
            try:
                entry = model[instance]
                results.check(
                    run_entry(entry, test_filter_command, out_dir, suite_dir)
                )

            except subprocess.CalledProcessError as exception:
                if exception.stderr is not None:
                    sys.stderr.write(exception.stderr.decode("utf-8"))

                results.check(False, str(exception))

    return util.print_result_summary(results)


def main():
    """Run the filter test suite via the command line tools."""

    parser = argparse.ArgumentParser(
        usage="%(prog)s [OPTION]... MANIFEST BASE_URI -- [ARG]...",
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )

    parser.add_argument(
        "--pipe", default="tools/serd-pipe", help="pipe executable"
    )

    parser.add_argument(
        "--filter", default="tools/serd-filter", help="filter executable"
    )

    parser.add_argument("--wrapper", default="", help="executable wrapper")
    parser.add_argument("manifest", help="test suite manifest.ttl file")
    parser.add_argument("base_uri", help="base URI for tests")

    args = parser.parse_args(sys.argv[1:])
    wrapper_prefix = shlex.split(args.wrapper)
    filter_command = wrapper_prefix + [args.filter]
    pipe_command = wrapper_prefix + [args.pipe]

    with tempfile.TemporaryDirectory() as test_out_dir:
        return run_suite(
            args.manifest,
            args.base_uri,
            filter_command,
            pipe_command,
            test_out_dir,
        )


if __name__ == "__main__":
    sys.exit(main())
