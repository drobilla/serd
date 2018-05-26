// Copyright 2019-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_STATIC_NODES_H
#define SERD_STATIC_NODES_H

#include "serd/serd.h"

#include "node.h"
#include "serd_internal.h"

typedef struct StaticNode {
  SerdNode node;
  char     buf[sizeof(NS_XSD "base64Binary") + sizeof(SerdNode)];
} StaticNode;

#define DEFINE_XSD_NODE(name)                 \
  static const StaticNode serd_xsd_##name = { \
    {sizeof(NS_XSD #name) - 1, 0, SERD_URI}, NS_XSD #name};

DEFINE_XSD_NODE(base64Binary)
DEFINE_XSD_NODE(decimal)
DEFINE_XSD_NODE(integer)

#endif // SERD_STATIC_NODES_H
