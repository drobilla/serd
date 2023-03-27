#!/usr/bin/env python3

# Copyright 2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: ISC

"""Test reading from stdin with serd-pipe."""

# pylint: disable=consider-using-f-string

import serd_test_util as util

args = util.wrapper_args(__doc__)
command = [args.serdi, "-B", "http://example.org", "-i", "turtle", "-"]

IN_DOCUMENT = "<s> <p> <o> ."
OUT_DOCUMENT = "<{0}s> <{0}p> <{0}o> .".format("http://example.org/")

stdout = util.command_output(args.wrapper, command, IN_DOCUMENT)
lines = stdout.splitlines(True)

assert len(lines) == 1
assert lines[0].strip() == OUT_DOCUMENT
