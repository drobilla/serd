#!/usr/bin/env python3

# Copyright 2022-2023 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: ISC

"""Test quiet command-line option."""

import shlex
import subprocess

import serd_test_util as util

args = util.wrapper_args(__doc__, True)
command = shlex.split(args.wrapper) + [args.tool, "-q", args.input]
proc = subprocess.run(
    command, check=False, stdout=subprocess.PIPE, stderr=subprocess.PIPE
)

assert proc.returncode != 0
assert args.wrapper or len(proc.stderr) == 0
