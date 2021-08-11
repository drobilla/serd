#!/usr/bin/env python3

# Copyright 2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: ISC

"""Utilities for data-driven tests."""

import datetime
import re
import subprocess


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
    """Load an RDF file as dictionaries via serdi (only supports URIs)."""

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
