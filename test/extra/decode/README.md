Decode Test Suite
=================

This suite tests hex-decoding of characters in URIs.  Characters are decoded
while reading the input, to the maximum extent possible when decoding an entire
URI (further hex-decoding is only possible when working with URI components
individually).  This means that characters in the unreserved set (ALPHA / DIGIT
/ "-" / "." / "_" / "~") will be decoded, but serd never encodes these.  So,
this operation is more of a normalizing step, and is (like this test suite) not
reversible.
