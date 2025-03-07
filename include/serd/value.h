// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_VALUE_H
#define SERD_VALUE_H

#include <serd/attributes.h>
#include <serd/object_view.h>

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
  SERD_NO_VALUE, ///< Sentinel for unknown datatypes or errors
  SERD_BOOL,     ///< xsd:boolean (bool)
  SERD_DOUBLE,   ///< xsd:double (double)
  SERD_FLOAT,    ///< xsd:float (float)
  SERD_LONG,     ///< xsd:long (int64_t)
  SERD_INT,      ///< xsd:integer (int32_t)
  SERD_SHORT,    ///< xsd:short (int16_t)
  SERD_BYTE,     ///< xsd:byte (int8_t)
  SERD_ULONG,    ///< xsd:unsignedLong (uint64_t)
  SERD_UINT,     ///< xsd:unsignedInt (uint32_t)
  SERD_USHORT,   ///< xsd:unsignedShort (uint16_t)
  SERD_UBYTE,    ///< xsd:unsignedByte (uint8_t)
} SerdValueType;

/// The data of a #SerdValue (the actual machine-native primitive)
typedef union {
  bool     as_bool;   ///< #SERD_BOOL
  double   as_double; ///< #SERD_DOUBLE
  float    as_float;  ///< #SERD_FLOAT
  int64_t  as_long;   ///< #SERD_LONG
  int32_t  as_int;    ///< #SERD_INT
  int16_t  as_short;  ///< #SERD_SHORT
  int8_t   as_byte;   ///< #SERD_BYTE
  uint64_t as_ulong;  ///< #SERD_ULONG
  uint32_t as_uint;   ///< #SERD_UINT
  uint16_t as_ushort; ///< #SERD_USHORT
  uint8_t  as_ubyte;  ///< #SERD_UBYTE
} SerdValueData;

/// A primitive value with a type tag
typedef struct {
  SerdValueType type; ///< Type that selects which data field is valid
  SerdValueData data; ///< Data union
} SerdValue;

/// Convenience constructor to make a #SERD_NO_VALUE (non-)value
SERD_CONST_API SerdValue
serd_no_value(void);

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
   Return the primitive value of a literal node.

   This will return a typed numeric value if the node can be read as one, or
   nothing otherwise.

   @param node A view of a literal node to parse as a number.

   @return The primitive value of `node`, if possible and supported.
*/
SERD_API SerdValue
serd_parse_value(SerdObjectView node);

/**
   Return the primitive value of a node as a specific type of number.

   This is like serd_parse_value(), but will coerce the value of the node to the
   requested type if possible.

   @param node A view of a literal node to parse as a number.

   @param type The desired numeric datatype of the result.

   @param lossy Whether lossy conversions can be used.  If this is false, then
   this function only succeeds if the value could be converted back to the
   original datatype of the node without loss.  Otherwise, precision may be
   reduced or values may be truncated to fit the result.

   @return The value of `node` as a #SerdValue, or nothing.
*/
SERD_API SerdValue
serd_parse_value_as(SerdObjectView node, SerdValueType type, bool lossy);

/**
   @}
*/

SERD_END_DECLS

#endif // SERD_VALUE_H
