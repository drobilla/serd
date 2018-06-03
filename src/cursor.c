/*
  Copyright 2018 David Robillard <http://drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "cursor.h"

#include <stdlib.h>
#include <string.h>

SerdCursor*
serd_cursor_new(const SerdNode* name, unsigned line, unsigned col)
{
	SerdCursor* cursor = (SerdCursor*)malloc(sizeof(SerdCursor));

	cursor->file = name;
	cursor->line = line;
	cursor->col  = col;
	return cursor;
}

SerdCursor*
serd_cursor_copy(const SerdCursor* cursor)
{
	if (!cursor) {
		return NULL;
	}

	SerdCursor* copy = (SerdCursor*)malloc(sizeof(SerdCursor));
	memcpy(copy, cursor, sizeof(SerdCursor));
	return copy;
}

void
serd_cursor_free(SerdCursor* cursor)
{
	free(cursor);
}

bool
serd_cursor_equals(const SerdCursor* l, const SerdCursor* r)
{
	return (l == r || (l && r && serd_node_equals(l->file, r->file) &&
	                   l->line == r->line && l->col == r->col));
}

const SerdNode*
serd_cursor_get_name(const SerdCursor* cursor)
{
	return cursor->file;
}

unsigned
serd_cursor_get_line(const SerdCursor* cursor)
{
	return cursor->line;
}

unsigned
serd_cursor_get_column(const SerdCursor* cursor)
{
	return cursor->col;
}
