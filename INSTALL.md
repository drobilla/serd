Installation Instructions
=========================

Prerequisites
-------------

To build from source, you will need:

 * A relatively modern C compiler (GCC, Clang, and MSVC are known to work).

 * [Meson](http://mesonbuild.com/), which depends on
   [Python](http://python.org/).

Configuration
-------------

The build is configured with the `meson setup` command, which creates a new
build directory with the given name:

    meson setup build

All available options can be inspected with the `meson configure` command from
within the build directory:

    cd build
    meson configure

Options can be set by passing a (C compiler style) "define" flag to `meson
configure`:

    meson configure -Dtests=false -Dprefix=/opt/mypackage/

It is also possible to set options in the same way during the original setup
command:

    meson setup -Dtests=false -Dprefix=/opt/mypackage/ build

Building
--------

Assuming you're building on the command line as above, the build is executed by
running `ninja` in the build directory:

    ninja

A target can also be passed to ninja, for example, to run the tests:

    ninja test

Meson can also generate a project for several popular IDEs, see the `backend`
option for details.

Installation
------------

Installation is also done via a ninja target:

    ninja install

You may need to acquire root permissions to install to a system-wide prefix.
The `DESTDIR` environment can be set during this command to add a path to the
installation prefix (which is useful for packaging):

    DESTDIR=/tmp/mypackage/ ninja install

Compiler Configuration
----------------------

Several standard environment variables can be used to control how compilers are
invoked:

 * `CC`:       Path to C compiler
 * `CFLAGS`:   C compiler options
 * `CXX`:      Path to C++ compiler
 * `CXXFLAGS`: C++ compiler options
 * `CPPFLAGS`: C preprocessor options
 * `LDFLAGS`:  Linker options

The value of these environment variables is recorded during `meson setup`,
they have no effect at any other stage.

Note that there are also meson options that do the same thing as most of these
environment variables, they are supported for convenience and compatibility
with the conventions of other build systems.

Library Versioning
------------------

Libraries in this project use semantic versioning <http://semver.org/>.

Several major versions can be installed in parallel.  The shared library name,
include directory, and pkg-config file are suffixed with the major version
number.  For example, a library named "foo" at version 1.x.y might install:

    /usr/include/foo-1/foo/foo.h
    /usr/lib/foo-1.so.1.x.y
    /usr/lib/pkgconfig/foo-1.pc

Dependencies can check for the pkg-config package "foo-1".

Distribution packages should be versioned and/or named accordingly.  For
example, on most systems only one package of a given name can be installed at a
time, so the package that contains the above should be named "foo-1", not "foo"
(since "foo-2" may exist in the future).
