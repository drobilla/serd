#!/usr/bin/env python3

# Copyright 2021 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: ISC

"""Test filtering statements exclusively."""

import argparse
import sys
import shlex
import subprocess
import tempfile

DOCUMENTS = {
    "ntriples": """
    <urn:example:s> <urn:example:p> <urn:example:o> .
    <urn:example:s> <urn:example:q> <urn:example:r> .
""",
    "nquads": """
    <urn:example:s> <urn:example:p> <urn:example:o> <urn:example:g> .
    <urn:example:s> <urn:example:q> <urn:example:r> <urn:example:g> .
""",
}

parser = argparse.ArgumentParser(description=__doc__)

parser.add_argument("--tool", default="tools/serd-filter", help="executable")
parser.add_argument("--wrapper", default="", help="executable wrapper")

args = parser.parse_args(sys.argv[1:])


def check_pattern(syntax, pattern, result):
    command = shlex.split(args.wrapper) + [
        args.tool,
        "-I",
        syntax,
        pattern,
        "-",
    ]

    with tempfile.TemporaryFile() as out:
        proc = subprocess.run(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
            encoding="utf-8",
            input=DOCUMENTS[syntax],
        )

        assert proc.returncode == 0
        assert args.wrapper or len(proc.stderr) == 0
        assert proc.stdout == result


check_pattern(
    "ntriples",
    "?s <urn:example:p> <urn:example:o> .",
    "<urn:example:s> <urn:example:p> <urn:example:o> .\n",
)

check_pattern(
    "ntriples",
    "<urn:example:s> ?p <urn:example:o> .",
    "<urn:example:s> <urn:example:p> <urn:example:o> .\n",
)

check_pattern(
    "ntriples",
    "<urn:example:s> <urn:example:p> ?o .",
    "<urn:example:s> <urn:example:p> <urn:example:o> .\n",
)

check_pattern(
    "nquads",
    "?s <urn:example:p> <urn:example:o> <urn:example:g> .",
    "<urn:example:s> <urn:example:p> <urn:example:o> <urn:example:g> .\n",
)

check_pattern(
    "nquads",
    "<urn:example:s> ?p <urn:example:o> <urn:example:g> .",
    "<urn:example:s> <urn:example:p> <urn:example:o> <urn:example:g> .\n",
)

check_pattern(
    "nquads",
    "<urn:example:s> <urn:example:p> ?o <urn:example:g> .",
    "<urn:example:s> <urn:example:p> <urn:example:o> <urn:example:g> .\n",
)

check_pattern(
    "nquads",
    "<urn:example:s> <urn:example:p> <urn:example:o> ?g .",
    "<urn:example:s> <urn:example:p> <urn:example:o> <urn:example:g> .\n",
)
