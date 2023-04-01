#!/usr/bin/env python3

# Copyright 2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: ISC

"""Test writing with -O contextual (SERD_WRITE_CONTEXTUAL)."""

# pylint: disable=consider-using-f-string

import argparse
import sys
import shlex
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)

parser.add_argument("--tool", default="tools/serd-pipe", help="executable")
parser.add_argument("--wrapper", default="", help="executable wrapper")
parser.add_argument("input", default="", help="input file")

args = parser.parse_args(sys.argv[1:])
command = shlex.split(args.wrapper) + [
    args.tool,
    "-O",
    "turtle",
    "-O",
    "contextual",
    args.input,
]

DOC = "<{0}s> <{0}p> <{0}o> .".format("http://example.org/")

with tempfile.TemporaryFile() as out:
    proc = subprocess.run(
        command,
        check=False,
        encoding="utf-8",
        input=DOC,
        stdout=out,
        stderr=subprocess.PIPE,
    )

    assert proc.returncode == 0
    assert args.wrapper or len(proc.stderr) == 0

    out.seek(0)
    lines = out.readlines()

    for line in lines:
        assert "@prefix" not in line.decode("utf-8")
