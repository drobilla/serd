#!/usr/bin/env python3

"""Test serdi quiet option."""

import argparse
import sys
import shlex
import subprocess

parser = argparse.ArgumentParser(description=__doc__)

parser.add_argument("--serdi", default="./serdi", help="path to serdi")
parser.add_argument("--wrapper", default="", help="executable wrapper")
parser.add_argument("input", help="invalid input file")

args = parser.parse_args(sys.argv[1:])
command = shlex.split(args.wrapper) + [args.serdi, "-q", args.input]
proc = subprocess.run(command, check=False, capture_output=True)

assert proc.returncode != 0
assert len(proc.stderr) == 0
