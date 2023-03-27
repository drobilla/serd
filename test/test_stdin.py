#!/usr/bin/env python3

# Copyright 2022-2023 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: ISC

"""Test reading from stdin with serdi."""

# pylint: disable=consider-using-f-string

import serd_test_util as util

args = util.wrapper_args(__doc__)
command = [args.serdi, "-i", "ntriples", "-B", "http://example.org", "-"]

DOCUMENT = "<{0}s> <{0}p> <{0}o> .".format("http://example.org/")

stdout = util.command_output(args.wrapper, command, DOCUMENT)
lines = stdout.splitlines(True)

assert len(lines) == 1
assert lines[0].strip() == DOCUMENT
