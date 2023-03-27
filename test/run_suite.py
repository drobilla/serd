#!/usr/bin/env python3

# Copyright 2022-2023 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: ISC

"""Run a "simple" one-pass RDF-based test suite for serd."""

import argparse
import os
import shlex
import subprocess
import sys
import tempfile

import serd_test_util as util

NS_MF = "http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#"
NS_RDFT = "http://www.w3.org/ns/rdftest#"

DEVNULL = subprocess.DEVNULL
PIPE = subprocess.PIPE

TEST_TYPES = [
    NS_RDFT + "TestNQuadsPositiveSyntax",
    NS_RDFT + "TestNTriplesPositiveSyntax",
    NS_RDFT + "TestTrigEval",
    NS_RDFT + "TestTrigNegativeEval",
    NS_RDFT + "TestTrigNegativeSyntax",
    NS_RDFT + "TestTrigPositiveSyntax",
    NS_RDFT + "TestTurtleEval",
    NS_RDFT + "TestTurtleNegativeEval",
    NS_RDFT + "TestTurtleNegativeSyntax",
    NS_RDFT + "TestTurtlePositiveSyntax",
]


def run_eval_test(base_uri, command, in_path, good_path, out_path):
    """Run a positive eval test and return whether the output matches."""

    syntax = util.syntax_from_path(out_path)
    command = command + ["-o", syntax, in_path, base_uri]

    proc = subprocess.run(
        command, check=True, encoding="utf-8", stderr=DEVNULL, stdout=PIPE
    )

    out = [l + "\n" for l in proc.stdout.split("\n")][:-1]
    with open(good_path, "r", encoding="utf-8") as good:
        return util.lines_equal(list(good), out, good_path, out_path)


def run_entry(entry, base_uri, command_prefix, out_dir, suite_dir):
    """Run a single test entry from the manifest."""

    in_path = util.file_path(suite_dir, entry[NS_MF + "action"][0])
    base = base_uri + os.path.basename(in_path)

    good_path = in_path
    if NS_MF + "result" in entry:
        good_path = util.file_path(suite_dir, entry[NS_MF + "result"][0])

    out_path = os.path.join(out_dir, os.path.basename(good_path))
    return run_eval_test(base, command_prefix, in_path, good_path, out_path)


def run_suite(args, command, out_dir):
    """Run all tests in the manifest."""

    # Load manifest model
    top = os.path.dirname(args.manifest)
    model, instances = util.load_rdf(args.manifest, args.base_uri, command)

    # Run all the listed tests that have known types
    command = command + args.arg
    results = util.Results()
    for klass, instances in instances.items():
        check = klass in [NS_RDFT + "TestTrigEval", NS_RDFT + "TestTurtleEval"]
        if klass == NS_MF + "Manifest":
            continue

        if klass not in TEST_TYPES:
            raise RuntimeError("Unknown manifest entry type: " + klass)

        for instance in instances:
            try:
                entry = model[instance]
                if check and NS_MF + "result" not in entry:
                    raise RuntimeError("Eval test missing result: " + instance)

                results.check(
                    run_entry(entry, args.base_uri, command, out_dir, top)
                )

            except subprocess.CalledProcessError as exception:
                if exception.stderr is not None:
                    sys.stderr.write(exception.stderr)

                results.check(False, str(exception) + "\n")

    return util.print_result_summary(results)


def main():
    """Run the command line tool."""

    parser = argparse.ArgumentParser(
        usage="%(prog)s [OPTION]... MANIFEST BASE_URI -- [ARG]...",
        description=__doc__,
    )

    parser.add_argument("--serdi", default="serdi", help="path to serdi")
    parser.add_argument("--wrapper", default="", help="executable wrapper")
    parser.add_argument("manifest", help="test suite manifest.ttl file")
    parser.add_argument("base_uri", help="base URI for tests")
    parser.add_argument("arg", nargs=argparse.REMAINDER, help="serdi argument")

    args = parser.parse_args(sys.argv[1:])
    command = shlex.split(args.wrapper) + [args.serdi]

    with tempfile.TemporaryDirectory() as temp:
        return run_suite(args, command, temp)


if __name__ == "__main__":
    sys.exit(main())
