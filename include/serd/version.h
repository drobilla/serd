// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_VERSION_H
#define SERD_VERSION_H

#include "serd/attributes.h"

SERD_BEGIN_DECLS

/**
   @defgroup serd_version Version
   @ingroup serd_library

   Serd uses a single [semantic version number](https://semver.org) which
   reflects changes to the C library ABI.

   @{
*/

/**
   The major version number of the serd library.

   Semver: Increments when incompatible API changes are made.
*/
#define SERD_MAJOR_VERSION 1

/**
   The minor version number of the serd library.

   Semver: Increments when functionality is added in a backwards compatible
   manner.
*/
#define SERD_MINOR_VERSION 1

/**
   The micro version number of the serd library.

   Semver: Increments when changes are made that don't affect the API, such as
   performance improvements or bug fixes.
*/
#define SERD_MICRO_VERSION 1

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_VERSION_H
