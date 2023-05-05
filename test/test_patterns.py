#!/usr/bin/env python3

# Copyright 2021-2023 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: ISC

"""Test filtering statements inclusively and exclusively."""

import serd_test_util as util

DOCS = {
    "ntriples": """
<http://example.org/s> <http://example.org/p> <http://example.org/o> .
<http://example.org/N> <http://example.org/I> <http://example.org/L> .
""",
    "nquads": """
<urn:example:s> <urn:example:p> <urn:example:o> <urn:example:g> .
<urn:example:N> <urn:example:U> <urn:example:L> <urn:example:L> .
""",
}

args = util.wrapper_args(__doc__)


def check_pattern(syntax, pattern, expected_inclusive, expected_exclusive):
    """Run a check with an exclusive pattern."""

    command = [args.tool, "-I", syntax, pattern]
    inclusive = util.command_output(args.wrapper, command, DOCS[syntax])
    assert inclusive == expected_inclusive

    command = [args.tool, "-I", syntax, "-v", pattern]
    exclusive = util.command_output(args.wrapper, command, DOCS[syntax])
    assert exclusive == expected_exclusive


check_pattern(
    "ntriples",
    "?s <http://example.org/p> <http://example.org/o> .",
    "<http://example.org/s> <http://example.org/p> <http://example.org/o> .\n",
    "<http://example.org/N> <http://example.org/I> <http://example.org/L> .\n",
)

check_pattern(
    "ntriples",
    "<http://example.org/s> ?p <http://example.org/o> .",
    "<http://example.org/s> <http://example.org/p> <http://example.org/o> .\n",
    "<http://example.org/N> <http://example.org/I> <http://example.org/L> .\n",
)

check_pattern(
    "ntriples",
    "<http://example.org/s> <http://example.org/p> ?o .",
    "<http://example.org/s> <http://example.org/p> <http://example.org/o> .\n",
    "<http://example.org/N> <http://example.org/I> <http://example.org/L> .\n",
)

check_pattern(
    "nquads",
    "?s <urn:example:p> <urn:example:o> <urn:example:g> .",
    "<urn:example:s> <urn:example:p> <urn:example:o> <urn:example:g> .\n",
    "<urn:example:N> <urn:example:U> <urn:example:L> <urn:example:L> .\n",
)

check_pattern(
    "nquads",
    "<urn:example:s> ?p <urn:example:o> <urn:example:g> .",
    "<urn:example:s> <urn:example:p> <urn:example:o> <urn:example:g> .\n",
    "<urn:example:N> <urn:example:U> <urn:example:L> <urn:example:L> .\n",
)

check_pattern(
    "nquads",
    "<urn:example:s> <urn:example:p> ?o <urn:example:g> .",
    "<urn:example:s> <urn:example:p> <urn:example:o> <urn:example:g> .\n",
    "<urn:example:N> <urn:example:U> <urn:example:L> <urn:example:L> .\n",
)

check_pattern(
    "nquads",
    "<urn:example:s> <urn:example:p> <urn:example:o> ?g .",
    "<urn:example:s> <urn:example:p> <urn:example:o> <urn:example:g> .\n",
    "<urn:example:N> <urn:example:U> <urn:example:L> <urn:example:L> .\n",
)
