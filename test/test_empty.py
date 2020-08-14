#!/usr/bin/env python3

"""Test writing empty output."""

import argparse
import sys
import shlex
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)

parser.add_argument("--serdi", default="./serdi", help="path to serdi")
parser.add_argument("--wrapper", default="", help="executable wrapper")
parser.add_argument("input", help="valid input file")

args = parser.parse_args(sys.argv[1:])
command = shlex.split(args.wrapper) + [args.serdi, "-o", "empty", args.input]

with tempfile.TemporaryFile() as out:

    proc = subprocess.run(command, check=False, stdout=out)

    out.seek(0, 2)  # Seek to end

    assert proc.returncode == 0
    assert out.tell() == 0
