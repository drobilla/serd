#!/usr/bin/env python3

# Copyright 2022-2023 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: ISC

"""Test errors that only really happen when the output file is closed."""

import argparse
import os
import shlex
import subprocess
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--tool", default="tools/serd-pipe", help="executable")
parser.add_argument("--wrapper", default="", help="executable wrapper")
parser.add_argument("input", help="valid input file")
parser.add_argument("arg", nargs=argparse.REMAINDER, help="tool argument")
args = parser.parse_args(sys.argv[1:])

command = shlex.split(args.wrapper) + [args.tool] + args.arg + [args.input]

if not os.path.exists("/dev/full"):
    sys.stderr.write("warning: /dev/full not present, skipping test\n")
    sys.exit(0)

with open("/dev/full", "w", encoding="utf-8") as out:
    proc = subprocess.run(
        command, check=False, stdout=out, stderr=subprocess.PIPE
    )

assert proc.returncode != 0
assert "error" in proc.stderr.decode("utf-8")
