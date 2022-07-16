#!/usr/bin/env python3

# Copyright 2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: ISC

"""Run the collation tests for serd-sort."""

import argparse
import os
import random
import shlex
import subprocess
import sys
import tempfile

import serd_test_util

collations = [
    "GOPS",
    "GOSP",
    "GPSO",
    "GSOP",
    "GSPO",
    "OPS",
    "OSP",
    "POS",
    "PSO",
    "SOP",
    "SPO",
    "pretty",
]


def check(test_dir, command_prefix, out_dir, input_path, name):
    """Sort a single input in the named order and check the output.

    The expected output is assumed to exist at test_dir/NAME.nq.
    """

    output_path = os.path.join(out_dir, name + ".nq")
    result_path = os.path.join(test_dir, name + ".nq")
    options = [] if name == "pretty" else ["-c", name]

    # Randomly add irrelevant options just to cover them
    if random.choice([True, False]):
        options += ["-R", "http://example.org/"]
    if random.choice([True, False]):
        options += ["-I", "TriG"]

    command = command_prefix + options + ["-o", output_path, input_path]

    proc = subprocess.run(
        command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False
    )

    if proc.returncode != 0:
        cmd_string = " ".join(shlex.quote(c) for c in command)
        serd_test_util.error("Unexpected failure: {}".format(cmd_string))
        sys.stderr.write(proc.stderr.decode("utf-8"))
        return False

    if not serd_test_util.file_equals(result_path, output_path):
        serd_test_util.error(
            "Output {} differs from {}\n".format(output_path, result_path)
        )
        return False

    return True


def run_tests(test_dir, command_prefix, out_dir):
    """Run all the tests in the suite."""

    input_trig = os.path.join(test_dir, "input.trig")

    n_failures = 0
    for name in collations:
        if not check(test_dir, command_prefix, out_dir, input_trig, name):
            n_failures += 1

    return n_failures


def main():
    """Run the command line tool."""

    parser = argparse.ArgumentParser(
        usage="%(prog)s [OPTION]... INPUT",
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )

    parser.add_argument(
        "--tool", default="tools/serd-sort", help="serd-sort executable"
    )

    parser.add_argument("--wrapper", default="", help="executable wrapper")
    parser.add_argument(
        "input", help="path to input.trig in the test directory"
    )

    args = parser.parse_args(sys.argv[1:])
    wrapper_prefix = shlex.split(args.wrapper)
    command_prefix = wrapper_prefix + [args.tool]

    with tempfile.TemporaryDirectory() as out_dir:
        return run_tests(os.path.dirname(args.input), command_prefix, out_dir)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except subprocess.CalledProcessError as error:
        if error.stderr is not None:
            sys.stderr.write(error.stderr.decode("utf-8"))

        sys.stderr.write("error: %s\n" % error)
        sys.exit(error.returncode)
