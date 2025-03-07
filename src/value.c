// Copyright 2022-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include <exess/exess.h>
#include <serd/object_view.h>
#include <serd/value.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

enum { MAX_VALUE_SIZE = 8U };

SerdValue
serd_no_value(void)
{
  static const SerdValue value = {SERD_NO_VALUE, {0}};
  return value;
}

SerdValue
serd_bool(const bool v)
{
  const SerdValue value = {SERD_BOOL, {v}};
  return value;
}

SerdValue
serd_double(const double v)
{
  SerdValue value      = {SERD_DOUBLE, {0}};
  value.data.as_double = v;
  return value;
}

SerdValue
serd_float(const float v)
{
  SerdValue value     = {SERD_FLOAT, {0}};
  value.data.as_float = v;
  return value;
}

SerdValue
serd_long(const int64_t v)
{
  SerdValue value    = {SERD_LONG, {0}};
  value.data.as_long = v;
  return value;
}

SerdValue
serd_int(const int32_t v)
{
  SerdValue value   = {SERD_INT, {0}};
  value.data.as_int = v;
  return value;
}

SerdValue
serd_short(const int16_t v)
{
  SerdValue value     = {SERD_SHORT, {0}};
  value.data.as_short = v;
  return value;
}

SerdValue
serd_byte(const int8_t v)
{
  SerdValue value    = {SERD_BYTE, {0}};
  value.data.as_byte = v;
  return value;
}

SerdValue
serd_ulong(const uint64_t v)
{
  SerdValue value     = {SERD_ULONG, {0}};
  value.data.as_ulong = v;
  return value;
}

SerdValue
serd_uint(const uint32_t v)
{
  SerdValue value    = {SERD_UINT, {0}};
  value.data.as_uint = v;
  return value;
}

SerdValue
serd_ushort(const uint16_t v)
{
  SerdValue value      = {SERD_USHORT, {0}};
  value.data.as_ushort = v;
  return value;
}

SerdValue
serd_ubyte(const uint8_t v)
{
  SerdValue value     = {SERD_UBYTE, {0}};
  value.data.as_ubyte = v;
  return value;
}

static const ExessDatatype value_type_datatypes[] = {
  EXESS_NOTHING,
  EXESS_BOOLEAN,
  EXESS_DOUBLE,
  EXESS_FLOAT,
  EXESS_LONG,
  EXESS_INT,
  EXESS_SHORT,
  EXESS_BYTE,
  EXESS_ULONG,
  EXESS_UINT,
  EXESS_USHORT,
  EXESS_UBYTE,
};

static const SerdValueType datatype_value_types[] = {
  SERD_NO_VALUE, ///< EXESS_NOTHING
  SERD_BOOL,     ///< EXESS_BOOLEAN
  SERD_DOUBLE,   ///< EXESS_DOUBLE
  SERD_FLOAT,    ///< EXESS_FLOAT
  SERD_LONG,     ///< EXESS_LONG
  SERD_INT,      ///< EXESS_INT
  SERD_SHORT,    ///< EXESS_SHORT
  SERD_BYTE,     ///< EXESS_BYTE
  SERD_ULONG,    ///< EXESS_ULONG
  SERD_UINT,     ///< EXESS_UINT
  SERD_USHORT,   ///< EXESS_USHORT
  SERD_UBYTE,    ///< EXESS_UBYTE
  SERD_DOUBLE,   ///< EXESS_DECIMAL
  SERD_LONG,     ///< EXESS_INTEGER
  SERD_LONG,     ///< EXESS_NON_POSITIVE_INTEGER
  SERD_LONG,     ///< EXESS_NEGATIVE_INTEGER
  SERD_ULONG,    ///< EXESS_NON_NEGATIVE_INTEGER
  SERD_ULONG,    ///< EXESS_POSITIVE_INTEGER
};

static inline SerdValueType
datatype_value_type(const ExessDatatype datatype)
{
  return (datatype > EXESS_POSITIVE_INTEGER) ? SERD_NO_VALUE
                                             : datatype_value_types[datatype];
}

SerdValue
serd_parse_value(const SerdObjectView node)
{
  const ExessDatatype datatype = exess_datatype_from_uri(node.meta.string.data);
  const SerdValueType value_type = datatype_value_type(datatype);
  if (value_type == SERD_NO_VALUE) {
    return serd_no_value();
  }

  EXESS_ALIGN uint8_t       value[MAX_VALUE_SIZE] = {0};
  const ExessVariableResult vr =
    exess_read_value(datatype, node.string.data, sizeof(value), &value);

  if (vr.status) {
    return serd_no_value();
  }

  if (!(datatype >= EXESS_DECIMAL && datatype <= EXESS_POSITIVE_INTEGER)) {
    SerdValue v = {value_type, {false}};
    memcpy(&v.data, &value, vr.write_count);
    return v;
  }

  SerdValueData data = {false};

  const ExessDatatype fixed_datatype =
    (datatype == EXESS_DECIMAL)               ? EXESS_DOUBLE
    : (datatype < EXESS_NON_NEGATIVE_INTEGER) ? EXESS_LONG
                                              : EXESS_ULONG;

  const ExessResult r = exess_coerce_value(0U,
                                           datatype,
                                           exess_value_sizes[datatype],
                                           value,
                                           fixed_datatype,
                                           sizeof(data),
                                           &data);
  if (r.status) {
    return serd_no_value();
  }

  const SerdValue v = {value_type, data};
  return v;
}

SerdValue
serd_parse_value_as(const SerdObjectView node,
                    const SerdValueType  type,
                    const bool           lossy)
{
  // Get the value as it is
  const SerdValue value = serd_parse_value(node);
  if (!value.type || (unsigned)type > SERD_UBYTE || value.type == type) {
    return value;
  }

  const ExessCoercions coercions =
    lossy ? (EXESS_APPROXIMATE | EXESS_ROUND | EXESS_TRUNCATE) : 0U;

  assert(value.type <= SERD_UBYTE);
  assert(type <= SERD_UBYTE);
  const ExessDatatype node_datatype = value_type_datatypes[value.type];
  const ExessDatatype datatype      = value_type_datatypes[type];
  SerdValueData       data          = {false};

  // Coerce to the desired type
  const ExessResult r = exess_coerce_value(coercions,
                                           node_datatype,
                                           exess_value_sizes[node_datatype],
                                           &value.data,
                                           datatype,
                                           exess_value_sizes[datatype],
                                           &data);
  if (r.status) {
    return serd_no_value();
  }

  const SerdValue v = {type, data};
  return v;
}
