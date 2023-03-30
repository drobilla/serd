#!/usr/bin/env python3

# Copyright 2019-2023 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: ISC

"""Test reading from several input files."""

# pylint: disable=duplicate-code

import os
import shlex
import subprocess
import tempfile

import serd_test_util as util

args = util.wrapper_args(__doc__, True)
testdir = args.input
in1_path = os.path.join(testdir, "input1.ttl")
in2_path = os.path.join(testdir, "input2.trig")
check_path = os.path.join(testdir, "output.nq")
command = shlex.split(args.wrapper) + [args.tool, in1_path, in2_path]


with tempfile.TemporaryFile(mode="w+", encoding="utf-8") as out:
    proc = subprocess.run(command, check=False, stdout=out)

    assert proc.returncode == 0

    out.seek(0)
    with open(check_path, "r", encoding="utf-8") as check:
        assert util.lines_equal(list(check), list(out), check_path, "output")
