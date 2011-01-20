/* Serd, an RDF serialisation library.
 * Copyright 2011 David Robillard <d@drobilla.net>
 * 
 * Serd is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Serd is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <assert.h>
#include <stdlib.h>

#include "serd/serd.h"

static size_t
file_sink(const uint8_t* buf, size_t len, void* stream)
{
	FILE* file = (FILE*)stream;
	return fwrite(buf, 1, len, file);
}

static inline bool
serd_write_uri(FILE* file, const SerdURI* uri)
{
	return serd_uri_serialise(uri, file_sink, file);
}

static bool
serd_write_ascii(const uint8_t* utf8, size_t n_bytes, FILE* out_fd, const uint8_t esc)
{
	for (size_t i = 0; i < n_bytes;) {
		uint8_t in = utf8[i++];
		switch (in) {
		case '\\': fwrite("\\\\", 1, 2, out_fd); continue;
		case '\n': fwrite("\\n",  1, 2, out_fd); continue;
		case '\r': fwrite("\\r",  1, 2, out_fd); continue;
		case '\t': fwrite("\\t",  1, 2, out_fd); continue;
		case '"':  if (esc == '"') { fwrite("\\\"", 1, 2, out_fd); continue; }
		default: break;
		}

		if (in == esc) {
			fprintf(out_fd, "\\u%04X", esc);
			continue;
		}

		uint32_t c    = 0;
		size_t   size = 0;
		if ((in & 0x80) == 0) {  // Starts with `0'
			size = 1;
			c = in & 0x7F;
			if ((in >= 0x20) && (in <= 0x7E)) {  // Printable ASCII
				fwrite(&in, 1, 1, out_fd);
				continue;
			}
		} else if ((in & 0xE0) == 0xC0) {  // Starts with `110'
			size = 2;
			c = in & 0x1F;
		} else if ((in & 0xF0) == 0xE0) {  // Starts with `1110'
			size = 3;
			c = in & 0x0F;
		} else if ((in & 0xF8) == 0xF0) {  // Starts with `11110'
			size = 4;
			c = in & 0x07;
		} else if ((in & 0xFC) == 0xF8) {  // Starts with `111110'
			size = 5;
			c = in & 0x03;
		} else if ((in & 0xFE) == 0xFC) {  // Starts with `1111110'
			size = 6;
			c = in & 0x01;
		} else {
			fprintf(stderr, "invalid UTF-8 at offset %zu: %X\n", i, in);
			return false;
		}

#define READ_BYTE() do { \
			assert(i < n_bytes); \
			in = utf8[i++] & 0x3f; \
			c <<= 6; \
			c |= in; \
		} while (0)

		switch (size) {
		case 6: READ_BYTE();
		case 5: READ_BYTE();
		case 4: READ_BYTE();
		case 3: READ_BYTE();
		case 2: READ_BYTE();
		}

		if (c < 0xFFFF) {
			fprintf(out_fd, "\\u%04X", c);
		} else {
			fprintf(out_fd, "\\U%08X", c);
		}
	}
	return true;
}

SERD_API
bool
serd_write_node(FILE*             fd,
                const SerdURI*    base_uri,
                SerdNamespaces    ns,
                SerdNodeType      type,
                const SerdString* str,
                const SerdString* datatype,
                const SerdString* lang)
{
	SerdChunk uri_prefix;
	SerdChunk uri_suffix;
	switch (type) {
	case BLANK:
		fwrite("_:", 1, 2, fd);
		fwrite(str->buf, 1, str->n_bytes - 1, fd);
		break;
	case QNAME:
		if (!serd_namespaces_expand(ns, str, &uri_prefix, &uri_suffix)) {
			fprintf(stderr, "error: undefined namespace prefix `%s'\n", str->buf);
			return false;
		}
		fwrite("<", 1, 1, fd);
		serd_write_ascii(uri_prefix.buf, uri_prefix.len, fd, '>');
		serd_write_ascii(uri_suffix.buf, uri_suffix.len, fd, '>');
		fwrite(">", 1, 1, fd);
		break;
	case URI:
		if (serd_uri_string_is_relative(str->buf)) {
			SerdURI uri;
			if (serd_uri_parse(str->buf, &uri)) {
				SerdURI abs_uri;
				if (serd_uri_resolve(&uri, base_uri, &abs_uri)) {
					fwrite("<", 1, 1, fd);
					serd_write_uri(fd, &abs_uri);
					fwrite(">", 1, 1, fd);
					return true;
				}
			}
		} else {
			fwrite("<", 1, 1, fd);
			serd_write_ascii(str->buf, str->n_bytes - 1, fd, '>');
			fwrite(">", 1, 1, fd);
			return true;
		}
		return false;
	case LITERAL:
		fwrite("\"", 1, 1, fd);
		serd_write_ascii(str->buf, str->n_bytes - 1, fd, '"');
		fwrite("\"", 1, 1, fd);
		if (lang) {
			fwrite("@\"", 1, 2, fd);
			fwrite(lang->buf, 1, lang->n_bytes - 1, fd);
			fwrite("\"", 1, 1, fd);
		} else if (datatype) {
			fwrite("^^", 1, 2, fd);
			serd_write_node(fd, base_uri, ns, URI, datatype, NULL, NULL);
		}
		break;
	}
	return true;
}

