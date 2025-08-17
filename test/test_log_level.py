#!/usr/bin/env python3

# Copyright 2022-2026 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: ISC

"""Test log level command-line option."""

import shlex
import subprocess

import serd_test_util as util

args = util.wrapper_args(__doc__, True)

# Test with a level that shouldn't produce any messages

command = shlex.split(args.wrapper) + [args.tool, "-l", "notice", args.input]
proc = subprocess.run(
    command,
    encoding="utf-8",
    check=False,
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
)

assert proc.returncode == 0
assert args.wrapper or len(proc.stderr) == 0

# Test with a level that should produce messages

command = shlex.split(args.wrapper) + [args.tool, "-l", "info", args.input]
proc = subprocess.run(
    command,
    encoding="utf-8",
    check=False,
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
)

assert proc.returncode == 0
assert args.wrapper or len(proc.stderr) > 0
