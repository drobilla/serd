#!/usr/bin/env python3

"""Test reading from several input files."""

import argparse
import difflib
import os
import shlex
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser(description=__doc__)

parser.add_argument("--serdi", default="./serdi", help="path to serdi")
parser.add_argument("--wrapper", default="", help="executable wrapper")
parser.add_argument("testdir", help="multifile test directory")

args = parser.parse_args(sys.argv[1:])
in1_path = os.path.join(args.testdir, "input1.ttl")
in2_path = os.path.join(args.testdir, "input2.trig")
check_path = os.path.join(args.testdir, "output.nq")
command = shlex.split(args.wrapper) + [args.serdi, in1_path, in2_path]


def _show_diff(from_lines, to_lines, from_filename, to_filename):
    same = True
    for line in difflib.unified_diff(
        from_lines,
        to_lines,
        fromfile=os.path.abspath(from_filename),
        tofile=os.path.abspath(to_filename),
    ):
        sys.stderr.write(line)
        same = False

    return same


with tempfile.TemporaryFile(mode="w+", encoding="utf-8") as out:
    proc = subprocess.run(command, check=False, stdout=out)

    assert proc.returncode == 0

    out.seek(0)
    with open(check_path, "r", encoding="utf-8") as check:

        output_matches = _show_diff(
            check.readlines(), out.readlines(), check_path, "output"
        )

        assert output_matches
