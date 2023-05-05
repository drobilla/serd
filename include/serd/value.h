// Copyright 2011-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_VALUE_H
#define SERD_VALUE_H

#include "serd/attributes.h"

#include <stdbool.h>
#include <stdint.h>

SERD_BEGIN_DECLS

/**
   @defgroup serd_node_value Values
   @ingroup serd_data

   Serd supports reading and writing machine-native numbers, called "values",
   in a standards-conformant and portable way.  The value structure is used in
   the API to allow passing and returning a primitive value of any supported
   type.  Note that this is just an API convenience, literal nodes themselves
   always store their values as strings.

   @{
*/

/// The type of a #SerdValue
typedef enum {
  SERD_NOTHING, ///< Sentinel for unknown datatypes or errors
  SERD_BOOL,    ///< xsd:boolean (bool)
  SERD_DOUBLE,  ///< xsd:double (double)
  SERD_FLOAT,   ///< xsd:float (float)
  SERD_LONG,    ///< xsd:long (int64_t)
  SERD_INT,     ///< xsd:integer (int32_t)
  SERD_SHORT,   ///< xsd:short (int16_t)
  SERD_BYTE,    ///< xsd:byte (int8_t)
  SERD_ULONG,   ///< xsd:unsignedLong (uint64_t)
  SERD_UINT,    ///< xsd:unsignedInt (uint32_t)
  SERD_USHORT,  ///< xsd:unsignedShort (uint16_t)
  SERD_UBYTE,   ///< xsd:unsignedByte (uint8_t)
} SerdValueType;

/// The data of a #SerdValue (the actual machine-native primitive)
typedef union {
  bool     as_bool;
  double   as_double;
  float    as_float;
  int64_t  as_long;
  int32_t  as_int;
  int16_t  as_short;
  int8_t   as_byte;
  uint64_t as_ulong;
  uint32_t as_uint;
  uint16_t as_ushort;
  uint8_t  as_ubyte;
} SerdValueData;

/// A primitive value with a type tag
typedef struct {
  SerdValueType type;
  SerdValueData data;
} SerdValue;

/// Convenience constructor to make a #SERD_NOTHING (non-)value
SERD_CONST_API SerdValue
serd_nothing(void);

/// Convenience constructor to make a #SERD_BOOL value
SERD_CONST_API SerdValue
serd_bool(bool v);

/// Convenience constructor to make a #SERD_DOUBLE value
SERD_CONST_API SerdValue
serd_double(double v);

/// Convenience constructor to make a #SERD_FLOAT value
SERD_CONST_API SerdValue
serd_float(float v);

/// Convenience constructor to make a #SERD_LONG value
SERD_CONST_API SerdValue
serd_long(int64_t v);

/// Convenience constructor to make a #SERD_INT value
SERD_CONST_API SerdValue
serd_int(int32_t v);

/// Convenience constructor to make a #SERD_SHORT value
SERD_CONST_API SerdValue
serd_short(int16_t v);

/// Convenience constructor to make a #SERD_BYTE value
SERD_CONST_API SerdValue
serd_byte(int8_t v);

/// Convenience constructor to make a #SERD_ULONG value
SERD_CONST_API SerdValue
serd_ulong(uint64_t v);

/// Convenience constructor to make a #SERD_UINT value
SERD_CONST_API SerdValue
serd_uint(uint32_t v);

/// Convenience constructor to make a #SERD_USHORT value
SERD_CONST_API SerdValue
serd_ushort(uint16_t v);

/// Convenience constructor to make a #SERD_UBYTE value
SERD_CONST_API SerdValue
serd_ubyte(uint8_t v);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_VALUE_H
