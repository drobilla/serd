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
#include <string.h>

#include "serd/serd.h"

typedef struct {
	FILE*          out_fd;
	SerdNamespaces ns;
	SerdString*    base_uri_str;
	SerdURI        base_uri;
} State;

static bool
event_base(void*             handle,
           const SerdString* uri_str)
{
	State* const state = (State*)handle;

	SerdURI uri;
	if (!serd_uri_parse(uri_str->buf, &uri)) {
		return false;
	}

	SerdURI     base_uri = {{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},false};
	SerdString* base_uri_str;
	if (!uri.scheme.len) {
		// URI has no scheme (relative by definition), resolve
		SerdURI abs_base_uri;
		if (!serd_uri_resolve(&uri, &state->base_uri, &abs_base_uri)) {
			fprintf(stderr, "error: failed to resolve new base URI\n");
			assert(false);
			return false;
		}
		base_uri_str = serd_uri_serialise(&abs_base_uri, &base_uri);
		// FIXME: double parse
		serd_uri_parse(base_uri_str->buf, &base_uri);
	} else {
		// Absolute URI, use literally as new base URI
		base_uri_str = serd_string_copy(uri_str);
		// FIXME: double parse
		serd_uri_parse(base_uri_str->buf, &base_uri);
	}

	// Replace the old base URI
	free(state->base_uri_str);
	state->base_uri_str = base_uri_str;
	state->base_uri     = base_uri;

	return true;
}

static bool
event_prefix(void*             handle,
             const SerdString* name,
             const SerdString* uri_string)
{
	State* const state = (State*)handle;
	if (serd_uri_string_is_relative(uri_string->buf)) {
		SerdURI uri;
		if (!serd_uri_parse(uri_string->buf, &uri)) {
			return false;
		}
		SerdURI abs_uri;
		if (!serd_uri_resolve(&uri, &state->base_uri, &abs_uri)) {
			return false;
		}
		SerdURI     new_abs_uri;
		SerdString* abs_uri_string = serd_uri_serialise(&abs_uri, &new_abs_uri);
		serd_namespaces_add(state->ns, name, abs_uri_string);
	} else {
		serd_namespaces_add(state->ns, name, uri_string);
	}
	return true;
}

static inline bool
write_node(State*            state,
           const SerdString* str,
           SerdNodeType      type,
           const SerdString* datatype,
           const SerdString* lang)
{
	SerdRange uri_prefix;
	SerdRange uri_suffix;
	switch (type) {
	case BLANK:
		fwrite("_:", 1, 2, state->out_fd);
		fwrite(str->buf, 1, str->n_bytes - 1, state->out_fd);
		break;
	case QNAME:
		if (!serd_namespaces_expand(state->ns, str, &uri_prefix, &uri_suffix)) {
			fprintf(stderr, "error: undefined namespace prefix `%s'\n", str->buf);
			return false;
		}
		fwrite("<", 1, 1, state->out_fd);
		fwrite(uri_prefix.buf, 1, uri_prefix.len - 1, state->out_fd);
		fwrite(uri_suffix.buf, 1, uri_suffix.len - 1, state->out_fd);
		fwrite(">", 1, 1, state->out_fd);
		break;
	case URI:
		if (serd_uri_string_is_relative(str->buf)) {
			SerdURI uri;
			if (serd_uri_parse(str->buf, &uri)) {
				SerdURI abs_uri;
				if (serd_uri_resolve(&uri, &state->base_uri, &abs_uri)) {
					fwrite("<", 1, 1, state->out_fd);
					serd_uri_write(&abs_uri, state->out_fd);
					fwrite(">", 1, 1, state->out_fd);
					return true;
				}
			}
		} else {
			fwrite("<", 1, 1, state->out_fd);
			fwrite(str->buf, 1, str->n_bytes - 1, state->out_fd);
			fwrite(">", 1, 1, state->out_fd);
			return true;
		}
		return false;
	case LITERAL:
		fwrite("\"", 1, 1, state->out_fd);
		for (size_t i = 0; i < str->n_bytes - 1; ++i) {
			const char c = str->buf[i];
			switch (c) {
			case '\\': fwrite("\\\\", 1, 2, state->out_fd); break;
			case '\n': fwrite("\\n",  1, 2, state->out_fd); break;
			case '\r': fwrite("\\r",  1, 2, state->out_fd); break;
			case '\t': fwrite("\\t",  1, 2, state->out_fd); break;
			case '"':  fwrite("\\\"", 1, 2, state->out_fd); break;
			default:
				fwrite(&c, 1, 1, state->out_fd);
			}
		}
		fwrite("\"", 1, 1, state->out_fd);
		if (lang) {
			fwrite("@\"", 1, 2, state->out_fd);
			fwrite(lang->buf, 1, lang->n_bytes - 1, state->out_fd);
			fwrite("\"", 1, 1, state->out_fd);
		} else if (datatype) {
			fwrite("^^", 1, 2, state->out_fd);
			write_node(state, datatype, URI, NULL, NULL);
		}
		break;
	}
	return true;
}

static bool
event_statement(void*             handle,
                const SerdString* graph,
                const SerdString* subject,
                SerdNodeType      subject_type,
                const SerdString* predicate,
                SerdNodeType      predicate_type,
                const SerdString* object,
                SerdNodeType      object_type,
                const SerdString* object_datatype,
                const SerdString* object_lang)
{
	State* const state = (State*)handle;
	FILE* const  fd    = state->out_fd;
	write_node(state, subject, subject_type, NULL, NULL);
	fwrite(" ", 1, 1, fd);
	write_node(state, predicate, predicate_type, NULL, NULL);
	fwrite(" ", 1, 1, fd);
	write_node(state, object, object_type, object_datatype, object_lang);
	fwrite(" .\n", 1, 3, fd);
	return true;
}

int
main(int argc, char** argv)
{
	if (/*argc != 2 && */argc != 3) {
		fprintf(stderr, "Bad parameters\n");
		return 1;
	}

	const uint8_t* const in_filename  = (uint8_t*)argv[1];
	const uint8_t*       base_uri_str = in_filename;

	SerdURI base_uri;
	if (argc > 2) {
		base_uri_str = (const uint8_t*)argv[2];
		if (!serd_uri_parse(base_uri_str, &base_uri)) {
			fprintf(stderr, "invalid base uri: %s\n", base_uri_str);
			return 1;
		}

	}

	FILE* const in_fd  = fopen((const char*)in_filename,  "r");
	FILE*       out_fd = stdout;

	if (!in_fd) {
		fprintf(stderr, "failed to open file\n");
		return 1;
	}

	//SerdURI null_uri = {{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}};
	State state = { out_fd, serd_namespaces_new(), serd_string_new(base_uri_str), base_uri };

	SerdReader reader = serd_reader_new(
		SERD_TURTLE, &state, event_base, event_prefix, event_statement);

	const bool success = serd_reader_read_file(reader, in_fd, in_filename);
	serd_reader_free(reader);
	fclose(in_fd);
	serd_namespaces_free(state.ns);
	free(state.base_uri_str);

	if (success) {
		return 0;
	}

	return 1;
}
