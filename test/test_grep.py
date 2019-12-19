#!/usr/bin/env python3

"""Test filtering statements exclusively."""

import argparse
import sys
import shlex
import subprocess
import tempfile

DOCUMENTS = {
    'ntriples': """
    <urn:example:s> <urn:example:p> <urn:example:o> .
    <urn:example:s> <urn:example:q> <urn:example:r> .
""",

    'nquads': """
    <urn:example:s> <urn:example:p> <urn:example:o> <urn:example:g> .
    <urn:example:s> <urn:example:q> <urn:example:r> <urn:example:g> .
"""
}

parser = argparse.ArgumentParser(description=__doc__)

parser.add_argument("--serdi", default="./serdi", help="path to serdi")
parser.add_argument("--wrapper", default="", help="executable wrapper")

args = parser.parse_args(sys.argv[1:])

def check_pattern(syntax, pattern, result):
    command = shlex.split(args.wrapper) + [
        args.serdi,
        "-i",
        syntax,
        "-G",
        pattern,
        "-s",
        DOCUMENTS[syntax],
    ]

    with tempfile.TemporaryFile() as out:
        proc = subprocess.run(
            command,
            check=False,
            encoding="utf-8",
            capture_output=True,
        )

        assert proc.returncode == 0
        assert len(proc.stderr) == 0
        assert proc.stdout == result

check_pattern("ntriples",
              "?s <urn:example:p> <urn:example:o> .",
              "<urn:example:s> <urn:example:p> <urn:example:o> .\n")

check_pattern("ntriples",
              "<urn:example:s> ?p <urn:example:o> .",
              "<urn:example:s> <urn:example:p> <urn:example:o> .\n")

check_pattern("ntriples",
              "<urn:example:s> <urn:example:p> ?o .",
              "<urn:example:s> <urn:example:p> <urn:example:o> .\n")

check_pattern(
    "nquads",
    "?s <urn:example:p> <urn:example:o> <urn:example:g> .",
    "<urn:example:s> <urn:example:p> <urn:example:o> <urn:example:g> .\n")

check_pattern(
    "nquads",
    "<urn:example:s> ?p <urn:example:o> <urn:example:g> .",
    "<urn:example:s> <urn:example:p> <urn:example:o> <urn:example:g> .\n")

check_pattern(
    "nquads",
    "<urn:example:s> <urn:example:p> ?o <urn:example:g> .",
    "<urn:example:s> <urn:example:p> <urn:example:o> <urn:example:g> .\n")

check_pattern(
    "nquads",
    "<urn:example:s> <urn:example:p> <urn:example:o> ?g .",
    "<urn:example:s> <urn:example:p> <urn:example:o> <urn:example:g> .\n")
