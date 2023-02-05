Encode Test Suite
=================

This suite tests hex-encoding of "extended" (multi-byte) characters in URIs.
Characters are encoded while writing the output, and this operation (and test
suite) can be reversed by reading the output with decoding enabled.
