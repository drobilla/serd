#!/usr/bin/env python3

# Copyright 2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: ISC

"""Utilities for data-driven tests."""

import datetime
import difflib
import os
import re
import subprocess
import sys


def error(message):
    """Log an error message to stderr"""

    sys.stderr.write("error: ")
    sys.stderr.write(message)


def test_input_syntax(test_class):
    """Return the output syntax use for a given test class."""

    if "NTriples" in test_class:
        return "NTriples"

    if "Turtle" in test_class:
        return "Turtle"

    if "NQuads" in test_class:
        return "NQuads"

    if "Trig" in test_class:
        return "Trig"

    raise Exception("Unknown test class <{}>".format(test_class))


def test_output_syntax(test_class):
    """Return the output syntax use for a given test class."""

    if "NTriples" in test_class or "Turtle" in test_class:
        return "NTriples"

    if "NQuads" in test_class or "Trig" in test_class:
        return "NQuads"

    raise Exception("Unknown test class <{}>".format(test_class))


def earl_assertion(test, passed, asserter):
    """Return a Turtle description of an assertion for the test report."""

    asserter_str = ""
    if asserter is not None:
        asserter_str = "\n\tearl:assertedBy <%s> ;" % asserter

    return """
[]
\ta earl:Assertion ;%s
\tearl:subject <http://drobilla.net/sw/serd> ;
\tearl:test <%s> ;
\tearl:result [
\t\ta earl:TestResult ;
\t\tearl:outcome %s ;
\t\tdc:date "%s"^^xsd:dateTime
\t] .
""" % (
        asserter_str,
        test,
        "earl:passed" if passed else "earl:failed",
        datetime.datetime.now().replace(microsecond=0).isoformat(),
    )


def load_rdf(command_prefix, filename):
    """Load an RDF file as dictionaries via serd-pipe (only supports URIs)."""

    rdf_type = "http://www.w3.org/1999/02/22-rdf-syntax-ns#type"
    model = {}
    instances = {}

    cmd = command_prefix + [filename]
    proc = subprocess.run(
        cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True
    )
    for line in proc.stdout.splitlines():
        matches = re.match(
            r"<([^ ]*)> <([^ ]*)> <([^ ]*)> \.", line.decode("utf-8")
        )
        if matches:
            s, p, o = (matches.group(1), matches.group(2), matches.group(3))
            if s not in model:
                model[s] = {p: [o]}
            elif p not in model[s]:
                model[s][p] = [o]
            else:
                model[s][p].append(o)

            if p == rdf_type:
                if o not in instances:
                    instances[o] = set([s])
                else:
                    instances[o].update([s])

    return model, instances


def lines_equal(from_lines, to_lines, from_filename, to_filename):
    """Return true if from_lines equals to_lines, or print a diff."""
    same = True
    for line in difflib.unified_diff(
        from_lines,
        to_lines,
        fromfile=os.path.abspath(from_filename),
        tofile=os.path.abspath(to_filename),
    ):
        sys.stderr.write(line)
        same = False

    return same


def file_equals(patha, pathb):
    """Return true if the file at patha is the same as the file at pathb."""

    for path in (patha, pathb):
        if not os.access(path, os.F_OK):
            error("missing file {}\n".format(path))
            return False

    with open(patha, "r", encoding="utf-8") as fa:
        with open(pathb, "r", encoding="utf-8") as fb:
            return show_diff(fa.readlines(), fb.readlines(), patha, pathb)


def show_diff(from_lines, to_lines, from_filename, to_filename):
    """Print a diff between files to stderr."""

    same = True
    for line in difflib.unified_diff(
        from_lines,
        to_lines,
        fromfile=os.path.abspath(from_filename),
        tofile=os.path.abspath(to_filename),
    ):
        sys.stderr.write(line)
        same = False

    return same
