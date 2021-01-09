#!/usr/bin/env python3

"""Test errors writing to a file."""

import argparse
import sys
import shlex
import subprocess
import os

parser = argparse.ArgumentParser(description=__doc__)

parser.add_argument("--serdi", default="./serdi", help="path to serdi")
parser.add_argument("--wrapper", default="", help="executable wrapper")
parser.add_argument("input", help="valid input file")

args = parser.parse_args(sys.argv[1:])
command = shlex.split(args.wrapper) + [args.serdi, args.input]

if os.path.exists("/dev/full"):

    with open("/dev/full", "w") as out:
        proc = subprocess.run(
            command, check=False, stdout=out, stderr=subprocess.PIPE
        )

    assert proc.returncode != 0
    assert "error" in proc.stderr.decode("utf-8")

else:
    sys.stderr.write("warning: /dev/full not present, skipping test")
