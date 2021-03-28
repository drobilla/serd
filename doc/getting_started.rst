###############
Getting Started
###############

***********
Downloading
***********

Serd is distributed in several ways.
There are no "official" binaries, only source code releases which must be compiled.
However, many operating system distributions do package binaries.
Check if your package manager has a reasonably recent package,
if so,
that is the easiest and most reliable installation method.

Release announcements with links to source code archives can be found at `<https://drobilla.net/category/serd/>`_.
All release archives and their signatures are available in the directory `<http://download.drobilla.net/>`_.

The code can also be checked out of `git <https://gitlab.com/drobilla/serd>`_::

  git clone https://gitlab.com/drobilla/serd.git

*********
Compiling
*********

Serd uses the `meson <https://mesonbuild.com/>`_ build system.
From within an extracted release archive or repository checkout,
the library can be built and tested with default options like so::

  meson setup build
  cd build
  ninja test

There are many configuration options,
which can be displayed by running ``meson configure``.

See the `meson documentation <https://mesonbuild.com/Quick-guide.html>`_ for more details on using meson.

**********
Installing
**********

If the library compiled successfully,
then ``meson install`` can be used to install it.
Note that you may need superuser privileges::

  meson install

The installation prefix can be changed by setting the ``prefix`` option, for example::

  meson configure -Dprefix=/opt/serd

If you do not want to install anything,
you can also "vendor" the code in your project
(provided, of course, that you adhere to the terms of the license).
If you are using meson,
then it should simply work as a subproject without modification.
Otherwise,
you will need to set up the build yourself.

*********
Including
*********

Serd installs a `pkg-config <https://www.freedesktop.org/wiki/Software/pkg-config/>`_ file,
which can be used to set the appropriate compiler and linker flags for projects to use it.
If installed to a standard prefix,
then it should show up in ``pkg-config`` automatically::

  pkg-config --list-all | grep serd

If not, you may need to adjust the ``PKG_CONFIG_PATH`` environment variable to include the installation prefix, for example::

  export PKG_CONFIG_PATH=/opt/serd/lib/pkgconfig
  pkg-config --list-all | grep serd

Most popular build systems natively support pkg-config.
For example, in meson::

  serd_dep = dependency('serd-1')

On systems where pkg-config is not available,
you will need to set up compiler and linker flags manually,
by adding something like ``-I/opt/serd/include/serd-1``,
and ``-lserd-1``, respectively.

Once things are set up, you should be able to include the API header and start using Serd in your code.
