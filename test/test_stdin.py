#!/usr/bin/env python3

# Copyright 2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: ISC

"""Test reading from stdin with serdi."""

import argparse
import sys
import shlex
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)

parser.add_argument("--serdi", default="./serdi", help="path to serdi")
parser.add_argument("--wrapper", default="", help="executable wrapper")

args = parser.parse_args(sys.argv[1:])
command = shlex.split(args.wrapper) + [args.serdi, "-"]

DOCUMENT = "<{0}s> <{0}p> <{0}o> .".format("http://example.org/")

with tempfile.TemporaryFile() as out:
    proc = subprocess.run(
        command,
        check=False,
        encoding="utf-8",
        input=DOCUMENT,
        stdout=out,
        stderr=subprocess.PIPE,
    )

    assert proc.returncode == 0
    assert args.wrapper or len(proc.stderr) == 0

    out.seek(0)
    lines = out.readlines()

    assert len(lines) == 1
    assert lines[0].decode("utf-8").strip() == DOCUMENT
