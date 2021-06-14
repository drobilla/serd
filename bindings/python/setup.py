# Copyright 2020-2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: CC0-1.0 OR ISC

#!/usr/bin/env python3

import os

from setuptools import setup, find_packages, Extension

try:
    from Cython.Build import cythonize
except ImportError:
    cythonize = None


# https://cython.readthedocs.io/en/latest/src/userguide/source_files_and_compilation.html#distributing-cython-modules
def no_cythonize(extensions, **_ignore):
    for extension in extensions:
        sources = []
        for sfile in extension.sources:
            path, ext = os.path.splitext(sfile)
            if ext in (".pyx", ".py"):
                if extension.language == "c++":
                    ext = ".cpp"
                else:
                    ext = ".c"
                sfile = path + ext
            sources.append(sfile)
        extension.sources[:] = sources
    return extensions


extensions = [Extension("serd", ["serd.pyx"], libraries=["serd-1"])]

CYTHONIZE = bool(int(os.getenv("CYTHONIZE", 0))) and cythonize is not None

if CYTHONIZE:
    compiler_directives = {
        "language_level": 3,
        "embedsignature": True,
    }
    extensions = cythonize(extensions, compiler_directives=compiler_directives)
else:
    extensions = no_cythonize(extensions)

# with open("requirements.txt") as fp:
#     install_requires = fp.read().strip().split("\n")

# with open("requirements-dev.txt") as fp:
#     dev_requires = fp.read().strip().split("\n")

install_requires = []
dev_requires = []

setup(
    # install_requires=install_requires,
    # extras_require={
    #     "dev": dev_requires,
    #     "docs": ["sphinx", "sphinx-rtd-theme"],
    # },
    name="serd",
    version="1.0.1",  # FIXME
    description="A lightweight library for working with RDF data",
    long_description=open("README.md", "r").read(),
    long_description_content_type="text/markdown",
    url="https://gitlab.com/drobilla/serd",
    author="David Robillard",
    author_email="d@drobilla.net",
    zip_safe=False,
    license="ISC",
    # packages=["serd"],
    ext_modules=extensions,
    install_requires=[],
    classifiers=[
        "Development Status :: 5 - Production/Stable",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: ISC License (ISCL)",
        "Operating System :: POSIX",
        "Programming Language :: C",
        "Programming Language :: Cython",
        "Programming Language :: Python",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.6",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
    ],
)
