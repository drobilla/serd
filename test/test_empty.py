#!/usr/bin/env python3

# Copyright 2022-2023 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: ISC

"""Test writing empty output."""

# pylint: disable=duplicate-code

import shlex
import subprocess
import tempfile

import serd_test_util as util

args = util.wrapper_args(__doc__, True)
command = shlex.split(args.wrapper) + [args.tool, "-o", "empty", args.input]

with tempfile.TemporaryFile() as out:
    proc = subprocess.run(command, check=False, stdout=out)

    out.seek(0, 2)  # Seek to end

    assert proc.returncode == 0
    assert out.tell() == 0
