#!/usr/bin/env python3

# Copyright 2022-2023 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: ISC

"""Run the collation tests for serd-sort."""

import os
import shlex
import subprocess
import sys

import serd_test_util as util

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


def run_sort_test(command, in_path, good_path):
    """Sort a single input in the named order and check the output.

    The expected output is assumed to exist at test_dir/NAME.untyped.nq.
    """

    result_name = os.path.basename(good_path)
    options = []
    if result_name not in ["pretty.nq", "untyped.nq"]:
        options += ["-c", os.path.splitext(result_name)[0]]

    command = command + options + [in_path]

    proc = subprocess.run(
        command, check=True, encoding="utf-8", capture_output=True
    )

    lines = proc.stdout.splitlines(True)
    with open(good_path, "r", encoding="utf-8") as good:
        return util.lines_equal(list(good), lines, good_path, result_name)


def run_tests(test_dir, command):
    """Run all the tests in the suite."""

    n_failures = 0
    in_path = os.path.join(test_dir, "input.trig")

    # Test all the basic collations, and "pretty" with type first
    for name in collations:
        good_path = os.path.join(test_dir, name + ".nq")
        prefixes = [command, command + ["-I", "trig"]]
        for prefix in prefixes:
            if not run_sort_test(prefix, in_path, good_path):
                n_failures += 1

    # Test "pretty" without type first
    if not run_sort_test(
        command + ["-O", "longhand"],
        in_path,
        os.path.join(test_dir, "untyped.nq"),
    ):
        n_failures += 1

    return n_failures


def main():
    """Run the command line tool."""

    args = util.wrapper_args(__doc__, True)
    wrapper_prefix = shlex.split(args.wrapper)
    command_prefix = wrapper_prefix + [args.tool]

    return run_tests(os.path.dirname(args.input), command_prefix)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except subprocess.CalledProcessError as error:
        if error.stderr is not None:
            sys.stderr.write(error.stderr)

        sys.stderr.write(sys.argv[0])
        sys.stderr.write(": error: ")
        sys.stderr.write(str(error))
        sys.stderr.write("\n")
        sys.exit(error.returncode)
