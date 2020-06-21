# Copyright 2020 David Robillard <d@drobilla.net>
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

import base64
import math
import os
import serd
import shutil
import tempfile
import textwrap
import unittest
import itertools


class NamespaceTests(unittest.TestCase):
    def testConstruction(self):
        with self.assertRaises(TypeError):
            serd.Namespace()

        with self.assertRaises(TypeError):
            serd.Namespace(4)

        with self.assertRaises(TypeError):
            serd.Namespace(None)

        ns1 = serd.Namespace(serd.uri("http://example.org"))
        ns2 = serd.Namespace("http://example.org")

        self.assertEqual(ns1, ns2)

    def testComparison(self):
        ns1 = serd.Namespace("http://example.org/a#")
        ns2 = serd.Namespace("http://example.org/b#")

        self.assertNotEqual(ns1, ns2)

        self.assertEqual(ns1, serd.Namespace("http://example.org/a#"))
        self.assertEqual(ns1, serd.uri("http://example.org/a#"))
        self.assertEqual(ns1, "http://example.org/a#")

        self.assertNotEqual(ns1, serd.uri("http://drobilla.net/a#"))
        self.assertNotEqual(ns1, "http://drobilla.net/a#")

    def testAdd(self):
        ns = serd.Namespace("http://example.org/")

        self.assertEqual(ns + "foo", "http://example.org/foo")
        self.assertEqual(ns.foo, "http://example.org/foo")
        self.assertEqual(ns["foo"], "http://example.org/foo")

    def testName(self):
        ns = serd.Namespace("http://example.org/")

        self.assertEqual(ns.name("http://example.org/foo"), "foo")
        self.assertEqual(ns.name(serd.uri("http://example.org/foo")), "foo")

        self.assertIsNone(ns.name("http://drobilla.net/foo"))


class StringTests(unittest.TestCase):
    def testStrerror(self):
        self.assertEqual(serd.strerror(serd.Status.SUCCESS), "Success")
        self.assertEqual(serd.strerror(99999), "Unknown error")

        with self.assertRaises(OverflowError):
            serd.strerror(-1)

        self.assertEqual(
            serd.strerror(serd.Status.ERR_BAD_WRITE), "Error writing to file"
        )


# class Base64Tests(unittest.TestCase):
#     def testShortBase64(self):
#         data = "foobar".encode("utf-8")
#         encoded = "Zm9vYmFy"

#         self.assertEqual(serd.base64_encode(data), encoded)
#         self.assertEqual(serd.base64_encode(data, True), encoded)
#         self.assertEqual(serd.base64_decode(encoded), data)

#     def testLongBase64(self):
#         data = ("foobar" * 20).encode("utf-8")
#         oneline_encoded = "Zm9vYmFy" * 20
#         multiline_encoded = "\n".join(textwrap.wrap("Zm9vYmFy" * 20, width=76))

#         self.assertEqual(serd.base64_encode(data), oneline_encoded)
#         self.assertEqual(serd.base64_encode(data, True), multiline_encoded)
#         self.assertEqual(serd.base64_decode(oneline_encoded), data)
#         self.assertEqual(serd.base64_decode(multiline_encoded), data)


class SyntaxTests(unittest.TestCase):
    def testSyntaxByName(self):
        self.assertEqual(serd.syntax_by_name("TuRtLe"), serd.Syntax.TURTLE)
        self.assertEqual(serd.syntax_by_name("wat"), serd.Syntax.EMPTY)

    def testGuessSyntax(self):
        self.assertEqual(serd.guess_syntax("foo.nq"), serd.Syntax.NQUADS)
        self.assertEqual(serd.guess_syntax("foo.txt"), serd.Syntax.EMPTY)

    def testSyntaxHasGraphs(self):
        self.assertFalse(serd.syntax_has_graphs(serd.Syntax.EMPTY))
        self.assertFalse(serd.syntax_has_graphs(serd.Syntax.TURTLE))
        self.assertFalse(serd.syntax_has_graphs(serd.Syntax.NTRIPLES))
        self.assertTrue(serd.syntax_has_graphs(serd.Syntax.NQUADS))
        self.assertTrue(serd.syntax_has_graphs(serd.Syntax.TRIG))


class WorldTests(unittest.TestCase):
    def setUp(self):
        self.world = serd.World()

    def testGetBlank(self):
        self.assertEqual(self.world.get_blank(), serd.blank("b1"))
        self.assertEqual(self.world.get_blank(), serd.blank("b2"))


class NodeTests(unittest.TestCase):
    @staticmethod
    def _throughSyntax(n):
        return serd.Node.from_syntax(n.to_syntax())

    def testConstruction(self):
        self.assertEqual(serd.Node("hello"), serd.plain_literal("hello"))
        self.assertEqual(serd.Node(False), serd.boolean(False))
        self.assertEqual(serd.Node(True), serd.boolean(True))
        self.assertEqual(serd.Node(42), serd.integer(42))
        self.assertEqual(serd.Node(42.34), serd.double(42.34))

        with self.assertRaises(ValueError):
            serd.Node(-9223372036854775809)

        with self.assertRaises(ValueError):
            serd.Node(9223372036854775808)

    def testString(self):
        n = serd.string("hello")
        self.assertEqual(n.type(), serd.NodeType.LITERAL)
        self.assertEqual(n, "hello")
        self.assertEqual(len(n), 5)
        self.assertEqual(repr(n), 'serd.string("hello")')
        self.assertEqual(n, eval(repr(n)))
        self.assertEqual(n, self._throughSyntax(n))
        self.assertIsNone(n.datatype())
        self.assertIsNone(n.language())

    def testPlainLiteral(self):
        n = serd.plain_literal("hallo", "de")
        self.assertEqual(n.type(), serd.NodeType.LITERAL)
        self.assertEqual(n, "hallo")
        self.assertEqual(len(n), 5)
        self.assertEqual(repr(n), 'serd.plain_literal("hallo", "de")')
        self.assertEqual(n, eval(repr(n)))
        self.assertEqual(n, self._throughSyntax(n))
        self.assertIsNone(n.datatype())
        self.assertEqual(n.language(), serd.string("de"))

    def testTypedLiteral(self):
        datatype = serd.uri("http://example.org/ns#Hex")
        n = serd.typed_literal("ABCD", datatype)
        self.assertEqual(n.type(), serd.NodeType.LITERAL)
        self.assertEqual(n, "ABCD")
        self.assertEqual(len(n), 4)
        self.assertEqual(
            repr(n), 'serd.typed_literal("ABCD", "http://example.org/ns#Hex")'
        )
        self.assertEqual(n, eval(repr(n)))
        self.assertEqual(n, self._throughSyntax(n))
        self.assertEqual(n.datatype(), datatype)
        self.assertIsNone(n.language())

    def testBlank(self):
        n = serd.blank("b0")
        self.assertEqual(n.type(), serd.NodeType.BLANK)
        self.assertEqual(n, "b0")
        self.assertEqual(len(n), 2)
        self.assertEqual(repr(n), 'serd.blank("b0")')
        self.assertEqual(n, eval(repr(n)))
        self.assertEqual(n, self._throughSyntax(n))
        self.assertIsNone(n.datatype())
        self.assertIsNone(n.language())

    def testCurie(self):
        n = serd.curie("ns:name")
        self.assertEqual(n.type(), serd.NodeType.CURIE)
        self.assertEqual(n, "ns:name")
        self.assertEqual(len(n), 7)
        self.assertEqual(repr(n), 'serd.curie("ns:name")')
        self.assertEqual(n, eval(repr(n)))
        self.assertEqual(n, self._throughSyntax(n))
        self.assertIsNone(n.datatype())
        self.assertIsNone(n.language())

    def testUri(self):
        n = serd.uri("http://example.org/")
        self.assertEqual(n.type(), serd.NodeType.URI)
        self.assertEqual(n, "http://example.org/")
        self.assertEqual(len(n), 19)
        self.assertEqual(repr(n), 'serd.uri("http://example.org/")')
        self.assertEqual(n, eval(repr(n)))
        self.assertEqual(n, self._throughSyntax(n))
        self.assertIsNone(n.datatype())
        self.assertIsNone(n.language())

    def testRelativeUri(self):
        n = serd.uri("rel/uri")
        self.assertEqual(n.type(), serd.NodeType.URI)
        self.assertEqual(n, "rel/uri")
        self.assertEqual(len(n), 7)
        self.assertEqual(repr(n), 'serd.uri("rel/uri")')
        self.assertEqual(n, eval(repr(n)))
        self.assertEqual(n, self._throughSyntax(n))
        self.assertIsNone(n.datatype())
        self.assertIsNone(n.language())

    # def testResolvedUri(self):
    #     base = serd.uri("http://example.org/")
    #     n = serd.resolved_uri("name", base)
    #     self.assertEqual(n.type(), serd.NodeType.URI)
    #     self.assertEqual(n, "http://example.org/name")
    #     self.assertEqual(len(n), 23)
    #     self.assertEqual(repr(n), 'serd.uri("http://example.org/name")')
    #     self.assertEqual(n, eval(repr(n)))
    #     self.assertEqual(n, self._throughSyntax(n))
    #     self.assertIsNone(n.datatype())
    #     self.assertIsNone(n.language())

    def testLocalFileUri(self):
        n = serd.file_uri("/foo/bar")
        self.assertEqual(n.type(), serd.NodeType.URI)
        self.assertEqual(n, "file:///foo/bar")
        self.assertEqual(len(n), 15)
        self.assertEqual(repr(n), 'serd.uri("file:///foo/bar")')
        self.assertEqual(n, eval(repr(n)))
        self.assertEqual(n, self._throughSyntax(n))
        self.assertIsNone(n.datatype())
        self.assertIsNone(n.language())

    def testFileUriWithHostname(self):
        n = serd.file_uri("/foo/bar", "host")
        self.assertEqual(n.type(), serd.NodeType.URI)
        self.assertEqual(n, "file://host/foo/bar")
        print(n)
        self.assertEqual(len(n), 19)
        self.assertEqual(repr(n), 'serd.uri("file://host/foo/bar")')
        self.assertEqual(n, eval(repr(n)))
        self.assertEqual(n, self._throughSyntax(n))
        self.assertIsNone(n.datatype())
        self.assertIsNone(n.language())

    def testDecimal(self):
        xsd_decimal = "http://www.w3.org/2001/XMLSchema#decimal"

        n = serd.decimal(12.34, None)
        self.assertEqual(n.type(), serd.NodeType.LITERAL)
        self.assertEqual(n, "12.34")
        self.assertEqual(len(n), 5)
        self.assertEqual(
            repr(n),
            'serd.typed_literal("12.34", "http://www.w3.org/2001/XMLSchema#decimal")',
        )
        self.assertEqual(n, eval(repr(n)))
        self.assertEqual(n, self._throughSyntax(n))
        self.assertEqual(n.datatype(), serd.uri(xsd_decimal))
        self.assertIsNone(n.language())

        n = serd.decimal(12.34)
        self.assertEqual(n.type(), serd.NodeType.LITERAL)
        self.assertEqual(n, "12.34")
        self.assertEqual(len(n), 5)
        self.assertEqual(
            repr(n), 'serd.typed_literal("12.34", "{}")'.format(xsd_decimal)
        )
        self.assertEqual(n, eval(repr(n)))
        self.assertEqual(n, self._throughSyntax(n))
        self.assertEqual(n.datatype(), serd.uri(xsd_decimal))
        self.assertIsNone(n.language())

        datatype = "http://example.org/ns#Decimal"
        d = serd.decimal(1234, datatype=serd.uri(datatype))
        self.assertEqual(d.datatype(), serd.uri(datatype))
        self.assertEqual(d, "1234.0")
        self.assertEqual(len(d), 6)
        self.assertEqual(
            repr(d), 'serd.typed_literal("1234.0", "%s")' % datatype
        )
        self.assertEqual(d, eval(repr(d)))
        self.assertEqual(n, self._throughSyntax(n))

    def testDouble(self):
        xsd_double = "http://www.w3.org/2001/XMLSchema#double"
        n = serd.double(12.34)
        self.assertEqual(n.type(), serd.NodeType.LITERAL)
        self.assertEqual(n, "1.234E1")
        self.assertEqual(len(n), 7)
        self.assertEqual(
            repr(n), 'serd.typed_literal("1.234E1", "{}")'.format(xsd_double)
        )
        self.assertEqual(n, eval(repr(n)))
        self.assertEqual(n, self._throughSyntax(n))
        self.assertEqual(n.datatype(), serd.uri(xsd_double))
        self.assertIsNone(n.language())

    def testFloat(self):
        xsd_float = "http://www.w3.org/2001/XMLSchema#float"
        n = serd.float(234.5)
        self.assertEqual(n.type(), serd.NodeType.LITERAL)
        self.assertEqual(n, "2.345E2")
        self.assertEqual(len(n), 7)
        self.assertEqual(
            repr(n), 'serd.typed_literal("2.345E2", "{}")'.format(xsd_float)
        )
        self.assertEqual(n, eval(repr(n)))
        self.assertEqual(n, self._throughSyntax(n))
        self.assertEqual(n.datatype(), serd.uri(xsd_float))
        self.assertIsNone(n.language())

    def testInteger(self):
        xsd_integer = "http://www.w3.org/2001/XMLSchema#integer"
        n = serd.integer(42)
        self.assertEqual(n.type(), serd.NodeType.LITERAL)
        self.assertEqual(n, "42")
        self.assertEqual(len(n), 2)
        self.assertEqual(
            repr(n), 'serd.typed_literal("42", "{}")'.format(xsd_integer)
        )
        self.assertEqual(n, eval(repr(n)))
        self.assertEqual(n, self._throughSyntax(n))
        self.assertEqual(n.datatype(), serd.uri(xsd_integer))
        self.assertIsNone(n.language())

        datatype = "http://example.org/ns#Integer"
        d = serd.decimal(1234, datatype=serd.uri(datatype))
        self.assertEqual(d.datatype(), serd.uri(datatype))
        self.assertEqual(d, "1234.0")
        self.assertEqual(len(d), 6)
        self.assertEqual(
            repr(d), 'serd.typed_literal("1234.0", "{}")'.format(datatype)
        )
        self.assertEqual(d, eval(repr(d)))
        self.assertEqual(n, self._throughSyntax(n))

    def testBoolean(self):
        xsd_boolean = "http://www.w3.org/2001/XMLSchema#boolean"
        t = serd.boolean(True)
        self.assertEqual(t.type(), serd.NodeType.LITERAL)
        self.assertEqual(t, "true")
        self.assertEqual(len(t), 4)
        self.assertEqual(repr(t), "serd.boolean(True)")
        self.assertEqual(t, eval(repr(t)))
        self.assertEqual(t, self._throughSyntax(t))
        self.assertEqual(t.datatype(), serd.uri(xsd_boolean))
        self.assertIsNone(t.language())

        f = serd.boolean(False)
        self.assertEqual(f.type(), serd.NodeType.LITERAL)
        self.assertEqual(f, "false")
        self.assertEqual(len(f), 5)
        self.assertEqual(repr(f), "serd.boolean(False)")
        self.assertEqual(f, eval(repr(f)))
        self.assertEqual(f, self._throughSyntax(f))
        self.assertEqual(f.datatype(), serd.uri(xsd_boolean))
        self.assertIsNone(f.language())

    def testBlob(self):
        xsd_base64Binary = "http://www.w3.org/2001/XMLSchema#base64Binary"
        n = serd.base64(b"DEAD")
        n_bytes = base64.b64encode(b"DEAD")
        self.assertEqual(n.type(), serd.NodeType.LITERAL)
        self.assertEqual(bytes(str(n), "utf-8"), n_bytes)
        self.assertEqual(len(n), 8)
        self.assertEqual(
            repr(n),
            'serd.typed_literal("{}", "{}")'.format(
                n_bytes.decode("utf-8"), xsd_base64Binary
            ),
        )
        self.assertEqual(n, eval(repr(n)))
        self.assertEqual(n, self._throughSyntax(n))
        self.assertEqual(n.datatype(), serd.uri(xsd_base64Binary))
        self.assertIsNone(n.language())

        datatype = "http://example.org/ns#Blob"
        t = serd.base64(b"BEEF", datatype=serd.uri(datatype))
        t_bytes = base64.b64encode(b"BEEF")
        self.assertEqual(t.type(), serd.NodeType.LITERAL)
        self.assertEqual(bytes(str(t), "utf-8"), t_bytes)
        self.assertEqual(len(t), 8)
        self.assertEqual(
            repr(t),
            'serd.typed_literal("{}", "{}")'.format(
                t_bytes.decode("utf-8"), datatype
            ),
        )
        self.assertEqual(n, eval(repr(n)))
        self.assertEqual(n, self._throughSyntax(n))
        self.assertEqual(t.datatype(), serd.uri(datatype))
        self.assertIsNone(t.language())

    def testVariable(self):
        n = serd.variable("foo")
        self.assertEqual(n.type(), serd.NodeType.VARIABLE)
        self.assertEqual(n, "foo")
        self.assertEqual(len(n), 3)
        self.assertEqual(repr(n), 'serd.variable("foo")')
        self.assertEqual(n, eval(repr(n)))
        # self.assertEqual(n, self._throughSyntax(n))
        self.assertIsNone(n.datatype())
        self.assertIsNone(n.language())

    def testComparison(self):
        a = serd.string("Aardvark")
        b = serd.string("Banana")

        self.assertEqual(a, a)
        self.assertNotEqual(a, b)
        self.assertLess(a, b)
        self.assertLessEqual(a, b)
        self.assertLessEqual(a, a)
        self.assertGreater(b, a)
        self.assertGreaterEqual(b, a)
        self.assertGreaterEqual(b, b)

    def testHash(self):
        nodes = [
            serd.plain_literal("hello"),
            serd.plain_literal("hello", "en"),
            serd.typed_literal("hello", "http://example.org/hex"),
            serd.blank("hello"),
            serd.curie("eg:hello"),
            serd.uri("http://example.org"),
        ]

        # Check that all node types have a distinct hash
        for lhs, rhs in itertools.combinations(nodes, r=2):
            self.assertNotEqual(hash(lhs), hash(rhs))

        # Check that nodes work in a set
        self.assertEqual(len(set(nodes)), len(nodes))


class EnvTests(unittest.TestCase):
    def testEquality(self):
        uri = serd.uri("http://example.org/")
        env1 = serd.Env()
        env2 = serd.Env()
        self.assertEqual(env1, env2)

        env2.set_base_uri(uri)
        self.assertNotEqual(env1, env2)

        env2.set_base_uri(None)
        self.assertEqual(env1, env2)

        env2.set_prefix("eg", uri)
        self.assertNotEqual(env1, env2)

        env1.set_prefix(serd.string("eg"), uri)
        self.assertEqual(env1, env2)

    def testBaseUri(self):
        env = serd.Env()
        self.assertIsNone(env.base_uri())

        base = serd.uri("http://example.org/")
        env.set_base_uri(base)
        self.assertEqual(env.base_uri(), base)

    def testInitialBaseUri(self):
        base = serd.uri("http://example.org/")
        env = serd.Env(base)
        self.assertEqual(env.base_uri(), base)

    def testQualify(self):
        base = serd.uri("http://example.org/")
        uri = serd.uri("http://example.org/name")
        env = serd.Env(base)

        self.assertIsNone(env.qualify(uri))

        env.set_prefix("eg", base)
        self.assertEqual(env.qualify(uri), "eg:name")

    def testExpand(self):
        base = serd.uri("http://example.org/")
        curie = serd.curie("eg:name")
        env = serd.Env(base)

        self.assertIsNone(env.expand(curie))

        env.set_prefix("eg", base)
        self.assertEqual(
            env.expand(curie), serd.uri("http://example.org/name")
        )


class ModelTests(unittest.TestCase):
    def setUp(self):
        self.world = serd.World()
        self.s = serd.uri("http://example.org/s")
        self.p = serd.uri("http://example.org/p")
        self.o = serd.uri("http://example.org/o")
        self.o1 = serd.uri("http://example.org/o1")
        self.o2 = serd.uri("http://example.org/o2")
        self.g = serd.uri("http://example.org/g")
        self.x = serd.uri("http://example.org/x")

    def testConstruction(self):
        flags = serd.ModelFlags.INDEX_SPO | serd.ModelFlags.INDEX_GRAPHS
        model = serd.Model(self.world, flags)
        self.assertEqual(model.flags(), flags)
        self.assertNotEqual(model.flags(), serd.ModelFlags.INDEX_SPO)
        self.assertEqual(model.world(), self.world)

    def testInsertErase(self):
        model = serd.Model(self.world, serd.ModelFlags.INDEX_SPO)

        model.insert((self.s, self.p, self.o))
        self.assertEqual(len(model), 1)
        model.erase(iter(model))
        self.assertEqual(len(model), 0)

        statement = serd.Statement(self.s, self.p, self.o)
        model += statement
        self.assertEqual(len(model), 1)
        del model[statement]
        self.assertEqual(len(model), 0)

    def testSize(self):
        model = serd.Model(self.world, serd.ModelFlags.INDEX_SPO)
        self.assertEqual(model.size(), 0)
        self.assertEqual(len(model), 0)
        self.assertTrue(model.empty())

        model.insert((self.s, self.p, self.o))
        self.assertEqual(model.size(), 1)
        self.assertEqual(len(model), 1)
        self.assertFalse(model.empty())

        model.erase(iter(model))
        self.assertEqual(model.size(), 0)
        self.assertEqual(len(model), 0)
        self.assertTrue(model.empty())

    # def testBeginEnd(self):
    #     s, p, o, g = self.s, self.p, self.o, self.g
    #     model = serd.Model(self.world, serd.ModelFlags.INDEX_SPO)

    #     self.assertEqual(model.begin(), model.end())

    #     model.insert((s, p, o, g))
    #     self.assertNotEqual(model.begin(), model.end())

    # def testFind(self):
    #     s, p, o, g, x = self.s, self.p, self.o, self.g, self.x
    #     flags = serd.ModelFlags.INDEX_SPO | serd.ModelFlags.INDEX_GRAPHS
    #     model = serd.Model(self.world, flags)
    #     in_statement = serd.Statement(s, p, o, g)
    #     out_statement = serd.Statement(x, p, o, g)

    #     model += in_statement
    #     self.assertEqual(model.find(out_statement), model.end())
    #     self.assertNotEqual(model.find(in_statement), model.end())

    def testGet(self):
        s, p, o, g = self.s, self.p, self.o, self.g
        flags = serd.ModelFlags.INDEX_SPO | serd.ModelFlags.INDEX_GRAPHS
        model = serd.Model(self.world, flags)

        model.insert((s, p, o, g))
        self.assertEqual(model.get(None, p, o, g), s)
        self.assertEqual(model.get(s, None, o, g), p)
        self.assertEqual(model.get(s, p, None, g), o)
        self.assertEqual(model.get(s, p, o, None), g)

    def testAsk(self):
        s, p, o, g, x = self.s, self.p, self.o, self.g, self.x
        flags = serd.ModelFlags.INDEX_SPO | serd.ModelFlags.INDEX_GRAPHS
        model = serd.Model(self.world, flags)
        model.insert((s, p, o, g))

        self.assertTrue(model.ask(s, p, o, g))
        self.assertIn(serd.Statement(s, p, o, g), model)
        self.assertIn((s, p, o, g), model)

        self.assertFalse(model.ask(x, p, o, g))
        self.assertNotIn(serd.Statement(x, p, o, g), model)
        self.assertNotIn((x, p, o, g), model)

        self.assertTrue(model.ask(None, p, o, g))
        self.assertTrue(model.ask(s, None, o, g))
        self.assertTrue(model.ask(s, p, None, g))
        self.assertTrue(model.ask(s, p, o, None))

        self.assertFalse(model.ask(None, x, o, g))
        self.assertFalse(model.ask(s, None, x, g))
        self.assertFalse(model.ask(s, p, None, x))
        self.assertFalse(model.ask(x, p, o, None))

    def testCount(self):
        s, p, o1, o2, g, x = self.s, self.p, self.o1, self.o2, self.g, self.x
        flags = serd.ModelFlags.INDEX_SPO | serd.ModelFlags.INDEX_GRAPHS
        model = serd.Model(self.world, flags)
        model.insert((s, p, o1, g))
        model.insert((s, p, o2, g))

        self.assertEqual(model.count(s, p, o1, g), 1)
        self.assertEqual(model.count(s, p, None, g), 2)
        self.assertEqual(model.count(s, p, x, g), 0)


class StatementTests(unittest.TestCase):
    def setUp(self):
        self.s = serd.uri("http://example.org/s")
        self.p = serd.uri("http://example.org/p")
        self.o = serd.uri("http://example.org/o")
        self.g = serd.uri("http://example.org/g")
        self.cursor = serd.Cursor("foo.ttl", 1, 0)

    def testGet(self):
        s, p, o, g = self.s, self.p, self.o, self.g
        statement = serd.Statement(s, p, o, g, self.cursor)

        self.assertEqual(statement[serd.Field.SUBJECT], s)
        self.assertEqual(statement[serd.Field.PREDICATE], p)
        self.assertEqual(statement[serd.Field.OBJECT], o)
        self.assertEqual(statement[serd.Field.GRAPH], g)

        self.assertEqual(statement[0], s)
        self.assertEqual(statement[1], p)
        self.assertEqual(statement[2], o)
        self.assertEqual(statement[3], g)

        with self.assertRaises(IndexError):
            statement[-1]

        with self.assertRaises(IndexError):
            statement[4]

    def testAllFields(self):
        s, p, o, g = self.s, self.p, self.o, self.g
        statement = serd.Statement(s, p, o, g, self.cursor)

        self.assertEqual(statement.node(serd.Field.SUBJECT), s)
        self.assertEqual(statement.node(serd.Field.PREDICATE), p)
        self.assertEqual(statement.node(serd.Field.OBJECT), o)
        self.assertEqual(statement.node(serd.Field.GRAPH), g)

        self.assertEqual(statement.subject(), s)
        self.assertEqual(statement.predicate(), p)
        self.assertEqual(statement.object(), o)
        self.assertEqual(statement.graph(), g)

        self.assertEqual(statement.cursor(), self.cursor)

    def testNoGraph(self):
        s, p, o = self.s, self.p, self.o
        statement = serd.Statement(s, p, o, None, self.cursor)

        self.assertEqual(statement.node(serd.Field.SUBJECT), s)
        self.assertEqual(statement.node(serd.Field.PREDICATE), p)
        self.assertEqual(statement.node(serd.Field.OBJECT), o)
        self.assertIsNone(statement.node(serd.Field.GRAPH))

        self.assertEqual(statement.subject(), s)
        self.assertEqual(statement.predicate(), p)
        self.assertEqual(statement.object(), o)
        self.assertIsNone(statement.graph())

        self.assertEqual(statement.cursor(), self.cursor)

    def testNoCursor(self):
        s, p, o, g = self.s, self.p, self.o, self.g
        statement = serd.Statement(s, p, o, g)

        self.assertEqual(statement.node(serd.Field.SUBJECT), s)
        self.assertEqual(statement.node(serd.Field.PREDICATE), p)
        self.assertEqual(statement.node(serd.Field.OBJECT), o)
        self.assertEqual(statement.node(serd.Field.GRAPH), g)

        self.assertEqual(statement.subject(), s)
        self.assertEqual(statement.predicate(), p)
        self.assertEqual(statement.object(), o)
        self.assertEqual(statement.graph(), g)

        self.assertIsNone(statement.cursor())

    def testNoGraphOrCursor(self):
        s, p, o = self.s, self.p, self.o
        statement = serd.Statement(s, p, o)

        self.assertEqual(statement.node(serd.Field.SUBJECT), s)
        self.assertEqual(statement.node(serd.Field.PREDICATE), p)
        self.assertEqual(statement.node(serd.Field.OBJECT), o)
        self.assertIsNone(statement.node(serd.Field.GRAPH))

        self.assertEqual(statement.subject(), s)
        self.assertEqual(statement.predicate(), p)
        self.assertEqual(statement.object(), o)
        self.assertIsNone(statement.graph())

        self.assertIsNone(statement.cursor())

    def testComparison(self):
        s, p, o, g = self.s, self.p, self.o, self.g
        statement1 = serd.Statement(s, p, o, g)
        statement2 = serd.Statement(o, p, s, g)

        self.assertEqual(statement1, statement1)
        self.assertNotEqual(statement1, statement2)

    def testMatches(self):
        s, p, o, g = self.s, self.p, self.o, self.g
        x = serd.uri("http://example.org/x")
        statement = serd.Statement(s, p, o, g)

        self.assertTrue(statement.matches(s, p, o, g))
        self.assertTrue(statement.matches(None, p, o, g))
        self.assertTrue(statement.matches(s, None, o, g))
        self.assertTrue(statement.matches(s, p, None, g))
        self.assertTrue(statement.matches(s, p, o, None))

        self.assertFalse(statement.matches(x, p, o, g))
        self.assertFalse(statement.matches(s, x, o, g))
        self.assertFalse(statement.matches(s, p, x, g))
        self.assertFalse(statement.matches(s, p, o, x))

    def testIteration(self):
        triple = serd.Statement(self.s, self.p, self.o)
        quad = serd.Statement(self.s, self.p, self.o, self.g)

        self.assertEqual([n for n in triple], [self.s, self.p, self.o])
        self.assertEqual([n for n in quad], [self.s, self.p, self.o, self.g])

    def testStr(self):
        self.assertEqual(
            str(serd.Statement(self.s, self.p, self.o)),
            "<http://example.org/s> <http://example.org/p> <http://example.org/o>",
        )

        self.assertEqual(
            str(serd.Statement(self.s, self.p, self.o, self.g)),
            "<http://example.org/s> <http://example.org/p> <http://example.org/o> <http://example.org/g>",
        )

    def testRepr(self):
        self.assertEqual(
            repr(serd.Statement(self.s, self.p, self.o)),
            'serd.Statement(serd.uri("http://example.org/s"), serd.uri("http://example.org/p"), serd.uri("http://example.org/o"))',
        )

        self.assertEqual(
            repr(serd.Statement(self.s, self.p, self.o, self.g)),
            'serd.Statement(serd.uri("http://example.org/s"), serd.uri("http://example.org/p"), serd.uri("http://example.org/o"), serd.uri("http://example.org/g"))',
        )


class RangeTests(unittest.TestCase):
    def setUp(self):
        self.world = serd.World()
        self.s = serd.uri("http://example.org/s")
        self.p = serd.uri("http://example.org/p")
        self.p1 = serd.uri("http://example.org/p1")
        self.p2 = serd.uri("http://example.org/p2")
        self.o1 = serd.uri("http://example.org/o1")
        self.o2 = serd.uri("http://example.org/o2")
        self.g = serd.uri("http://example.org/g")

    def testFront(self):
        model = serd.Model(self.world, serd.ModelFlags.INDEX_SPO)

        model.insert((self.s, self.p, self.o1))
        self.assertEqual(
            model.all().front(), serd.Statement(self.s, self.p, self.o1)
        )

    def testEmpty(self):
        model = serd.Model(self.world, serd.ModelFlags.INDEX_SPO)

        self.assertTrue(model.all().empty())
        self.assertFalse(model.all())

        model.insert((self.s, self.p, self.o1))
        self.assertFalse(model.all().empty())
        self.assertTrue(model.all())

    def testIteration(self):
        model = serd.Model(self.world, serd.ModelFlags.INDEX_SPO)

        model.insert((self.s, self.p, self.o1))
        model.insert((self.s, self.p, self.o2))

        i = iter(model.all())
        self.assertEqual(next(i), serd.Statement(self.s, self.p, self.o1))
        self.assertEqual(next(i), serd.Statement(self.s, self.p, self.o2))
        with self.assertRaises(StopIteration):
            next(i)

    def testEmptyIteration(self):
        model = serd.Model(self.world)
        count = 0

        for s in model:
            count += 1

        self.assertEqual(count, 0)

        for s in model.all():
            count += 1

        self.assertEqual(count, 0)

    def testInsertErase(self):
        model1 = serd.Model(self.world, serd.ModelFlags.INDEX_SPO)
        model2 = serd.Model(self.world, serd.ModelFlags.INDEX_SPO)

        model1.insert((self.s, self.p1, self.o1))
        model1.insert((self.s, self.p1, self.o2))
        model1.insert((self.s, self.p2, self.o1))
        model1.insert((self.s, self.p2, self.o2))

        model2.insert(model1.range((self.s, self.p1, None)))

        self.assertEqual(
            [s for s in model2],
            [
                serd.Statement(self.s, self.p1, self.o1),
                serd.Statement(self.s, self.p1, self.o2),
            ],
        )

        model1.erase(model1.range((self.s, self.p2, None)))
        self.assertEqual(model1, model2)


class CursorTests(unittest.TestCase):
    def testStringConstruction(self):
        cur = serd.Cursor("foo.ttl", 3, 4)
        self.assertEqual(cur.name(), "foo.ttl")
        self.assertEqual(cur.line(), 3)
        self.assertEqual(cur.column(), 4)

    def testNodeConstruction(self):
        name = serd.string("foo.ttl")
        cur = serd.Cursor(name, 5, 6)
        self.assertEqual(cur.name(), name)
        self.assertEqual(cur.line(), 5)
        self.assertEqual(cur.column(), 6)

    def testComparison(self):
        self.assertEqual(
            serd.Cursor("foo.ttl", 1, 2), serd.Cursor("foo.ttl", 1, 2)
        )
        self.assertNotEqual(
            serd.Cursor("foo.ttl", 9, 2), serd.Cursor("foo.ttl", 1, 2)
        )
        self.assertNotEqual(
            serd.Cursor("foo.ttl", 1, 9), serd.Cursor("foo.ttl", 1, 2)
        )
        self.assertNotEqual(
            serd.Cursor("bar.ttl", 1, 2), serd.Cursor("foo.ttl", 1, 2)
        )


class EventTests(unittest.TestCase):
    def testRepr(self):
        base = serd.uri("http://example.org/base")
        ns = serd.uri("http://example.org/ns")

        self.assertEqual(
            repr(serd.Event.base(base)),
            'serd.Event.base("http://example.org/base")',
        )

        self.assertEqual(
            repr(serd.Event.prefix("eg", ns)),
            'serd.Event.prefix("eg", "http://example.org/ns")',
        )

        s = serd.blank("s")
        p = serd.uri("http://example.org/p")
        o = serd.uri("http://example.org/o")
        g = serd.uri("http://example.org/g")
        statement = serd.Statement(s, p, o, g)

        self.assertEqual(
            repr(serd.Event.statement(statement)),
            'serd.Event.statement(serd.Statement(serd.blank("s"), serd.uri("http://example.org/p"), serd.uri("http://example.org/o"), serd.uri("http://example.org/g")))',
        )

        self.assertEqual(
            repr(
                serd.Event.statement(
                    statement,
                    serd.StatementFlags.EMPTY_S | serd.StatementFlags.ANON_O,
                )
            ),
            'serd.Event.statement(serd.Statement(serd.blank("s"), serd.uri("http://example.org/p"), serd.uri("http://example.org/o"), serd.uri("http://example.org/g")), serd.StatementFlags.EMPTY_S | serd.StatementFlags.ANON_O)',
        )

        self.assertEqual(
            repr(serd.Event.end(s)), 'serd.Event.end(serd.blank("s"))'
        )


class ReaderTests(unittest.TestCase):
    def setUp(self):
        self.world = serd.World()
        self.temp_dir = tempfile.mkdtemp()
        self.ttl_path = os.path.join(self.temp_dir, "input.ttl")
        self.s = serd.uri("http://example.org/s")
        self.p1 = serd.uri("http://example.org/p1")
        self.p2 = serd.uri("http://example.org/p2")
        self.o1 = serd.uri("http://example.org/o1")
        self.o2 = serd.uri("http://example.org/o2")

        self.ttl_document = """@prefix eg: <http://example.org/> .
@base <http://example.org/base> .
eg:s eg:p1 eg:o1 ;
eg:p2 eg:o2 .
"""
        self.events = [
            serd.Event.prefix("eg", "http://example.org/"),
            serd.Event.base("http://example.org/base"),
            serd.Event.statement(serd.Statement(self.s, self.p1, self.o1)),
            serd.Event.statement(serd.Statement(self.s, self.p2, self.o2)),
        ]

        with open(self.ttl_path, "w") as f:
            f.write(self.ttl_document)

    def tearDown(self):
        shutil.rmtree(self.temp_dir)

    def testReadFileToSink(self):
        class TestSink(serd.Sink):
            def __init__(self):
                super().__init__()
                self.events = []

            def __call__(self, event):
                self.events += [event]
                return serd.Status.SUCCESS

        s, p1, p2, o1, o2 = self.s, self.p1, self.p2, self.o1, self.o2

        env = serd.Env()
        source = serd.FileSource(self.ttl_path)
        sink = TestSink()
        reader = serd.Reader(
            self.world, serd.Syntax.TURTLE, 0, env, sink, 4096
        )

        self.assertEqual(reader.start(source), serd.Status.SUCCESS)
        self.assertEqual(sink.events, [])
        self.assertEqual(reader.read_document(), serd.Status.SUCCESS)
        self.assertEqual(reader.finish(), serd.Status.SUCCESS)
        self.assertEqual(sink.events, self.events)

    def testReadFileToFunction(self):
        captured_events = []

        def sink(event):
            captured_events.append(event)

        s, p1, p2, o1, o2 = self.s, self.p1, self.p2, self.o1, self.o2

        env = serd.Env()
        source = serd.FileSource(self.ttl_path)
        reader = serd.Reader(
            self.world, serd.Syntax.TURTLE, 0, env, sink, 4096
        )

        self.assertEqual(reader.start(source), serd.Status.SUCCESS)
        self.assertEqual(captured_events, [])
        self.assertEqual(reader.read_document(), serd.Status.SUCCESS)
        self.assertEqual(reader.finish(), serd.Status.SUCCESS)
        self.assertEqual(captured_events, self.events)

    def testReadStringToFunction(self):
        captured_events = []

        def sink(event):
            captured_events.append(event)

        s, p1, p2, o1, o2 = self.s, self.p1, self.p2, self.o1, self.o2

        env = serd.Env()
        source = serd.StringSource(self.ttl_document)
        reader = serd.Reader(
            self.world, serd.Syntax.TURTLE, 0, env, sink, 4096
        )

        self.assertEqual(reader.start(source), serd.Status.SUCCESS)
        self.assertEqual(captured_events, [])
        self.assertEqual(reader.read_document(), serd.Status.SUCCESS)
        self.assertEqual(reader.finish(), serd.Status.SUCCESS)
        self.assertEqual(captured_events, self.events)


class LoadTests(unittest.TestCase):
    def setUp(self):
        self.world = serd.World()
        self.temp_dir = tempfile.mkdtemp()
        self.ttl_path = os.path.join(self.temp_dir, "input.ttl")
        self.ttl_document = r"""@prefix eg: <http://example.org/> .
@base <http://example.org/base> .
eg:s eg:p1 eg:o1 ;
     eg:p2 eg:o2 .
"""

        with open(self.ttl_path, "w") as f:
            f.write(self.ttl_document)

    def tearDown(self):
        shutil.rmtree(self.temp_dir)

    def testLoad(self):
        s = serd.uri("http://example.org/s")
        p1 = serd.uri("http://example.org/p1")
        p2 = serd.uri("http://example.org/p2")
        o1 = serd.uri("http://example.org/o1")
        o2 = serd.uri("http://example.org/o2")

        model = self.world.load(self.ttl_path)

        self.assertEqual(
            [statement for statement in model],
            [
                serd.Statement(s, p1, o1),
                serd.Statement(s, p2, o2),
            ],
        )

    def testLoadString(self):
        s = serd.uri("http://example.org/s")
        p1 = serd.uri("http://example.org/p1")
        p2 = serd.uri("http://example.org/p2")
        o1 = serd.uri("http://example.org/o1")
        o2 = serd.uri("http://example.org/o2")

        model = self.world.loads(self.ttl_document)

        self.assertEqual(
            [statement for statement in model],
            [
                serd.Statement(s, p1, o1),
                serd.Statement(s, p2, o2),
            ],
        )


class DumpTests(unittest.TestCase):
    def setUp(self):
        self.world = serd.World()
        self.temp_dir = tempfile.mkdtemp()
        self.ttl_path = os.path.join(self.temp_dir, "output.ttl")

        self.ttl_document = r"""<http://example.org/s>
	<http://example.org/p> <http://example.org/o1> ,
		<http://example.org/o2> .
"""

        self.s = serd.uri("http://example.org/s")
        self.p = serd.uri("http://example.org/p")
        self.o = serd.uri("http://example.org/o")
        self.o1 = serd.uri("http://example.org/o1")
        self.o2 = serd.uri("http://example.org/o2")
        self.g = serd.uri("http://example.org/g")
        self.x = serd.uri("http://example.org/x")

    def tearDown(self):
        shutil.rmtree(self.temp_dir)

    def testDumpFile(self):
        s, p, o, o1, o2 = self.s, self.p, self.o, self.o1, self.o2
        g, x = self.g, self.x

        flags = serd.ModelFlags.INDEX_SPO
        model = serd.Model(self.world, flags)

        model.insert((s, p, o1))
        model.insert((s, p, o2))

        self.world.dump(model, self.ttl_path)

        with open(self.ttl_path, "r") as output:
            self.assertEqual(output.read(), self.ttl_document)

    def testDumpString(self):
        s, p, o, o1, o2 = self.s, self.p, self.o, self.o1, self.o2
        g, x = self.g, self.x

        flags = serd.ModelFlags.INDEX_SPO
        model = serd.Model(self.world, flags)

        model.insert((s, p, o1))
        model.insert((s, p, o2))

        self.assertEqual(self.world.dumps(model), self.ttl_document)
