#!/usr/bin/env python3

# Copyright 2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: ISC

"""Test writing empty output."""

import argparse
import sys
import shlex
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)

parser.add_argument("--tool", default="tools/serd-read", help="executable")
parser.add_argument("--wrapper", default="", help="executable wrapper")
parser.add_argument("input", help="valid input file")

args = parser.parse_args(sys.argv[1:])
command = shlex.split(args.wrapper) + [args.tool, "-O", "empty", args.input]

with tempfile.TemporaryFile() as out:

    proc = subprocess.run(command, check=False, stdout=out)

    out.seek(0, 2)  # Seek to end

    assert proc.returncode == 0
    assert out.tell() == 0
