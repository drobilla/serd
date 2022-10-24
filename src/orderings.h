// Copyright 2011-2024 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef SERD_SRC_ORDERINGS_H
#define SERD_SRC_ORDERINGS_H

#define N_STATEMENT_ORDERS 12U

/// Lookup table of ordered indices for each #SerdStatementOrder
static const unsigned char orderings[N_STATEMENT_ORDERS][4] = {
  {0U, 1U, 2U, 3U}, // SPOG
  {0U, 2U, 1U, 3U}, // SOPG
  {2U, 1U, 0U, 3U}, // OPSG
  {2U, 0U, 1U, 3U}, // OPSG
  {1U, 0U, 2U, 3U}, // PSOG
  {1U, 2U, 0U, 3U}, // PSOG
  {3U, 0U, 1U, 2U}, // GSPO
  {3U, 0U, 2U, 1U}, // GSPO
  {3U, 2U, 1U, 0U}, // GOPS
  {3U, 2U, 0U, 1U}, // GOPS
  {3U, 1U, 0U, 2U}, // GPSO
  {3U, 1U, 2U, 0U}  // GPSO
};

#endif // SERD_SRC_ORDERINGS_H
