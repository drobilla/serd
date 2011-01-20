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

	SerdURI     base_uri = {{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}};
	SerdString* base_uri_str;
	if (!uri.scheme.len) {
		// URI has no scheme (relative by definition), resolve
		SerdURI abs_base_uri;
		if (!serd_uri_resolve(&uri, &state->base_uri, &abs_base_uri)) {
			fprintf(stderr, "error: failed to resolve new base URI\n");
			assert(false);
			return false;
		}
		base_uri_str = serd_string_new_from_uri(&abs_base_uri, &base_uri);
		// FIXME: double parse
		serd_uri_parse(base_uri_str->buf, &base_uri);
	} else {
		// Absolute URI, use literally as new base URI
		base_uri_str = serd_string_copy(uri_str);
		// FIXME: double parse
		serd_uri_parse(base_uri_str->buf, &base_uri);
	}

	// Replace the old base URI
	serd_string_free(state->base_uri_str);
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
		SerdString* abs_uri_string = serd_string_new_from_uri(&abs_uri, &new_abs_uri);
		serd_namespaces_add(state->ns, name, abs_uri_string);
		serd_string_free(abs_uri_string);
	} else {
		serd_namespaces_add(state->ns, name, uri_string);
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
	serd_write_node(fd, &state->base_uri, state->ns,
	                subject_type, subject, NULL, NULL);
	fwrite(" ", 1, 1, fd);
	serd_write_node(fd, &state->base_uri, state->ns,
	                predicate_type, predicate, NULL, NULL);
	fwrite(" ", 1, 1, fd);
	serd_write_node(fd, &state->base_uri, state->ns,
	                object_type, object, object_datatype, object_lang);
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

	State state = { out_fd, serd_namespaces_new(), serd_string_new(base_uri_str), base_uri };

	SerdReader reader = serd_reader_new(
		SERD_TURTLE, &state, event_base, event_prefix, event_statement);

	const bool success = serd_reader_read_file(reader, in_fd, in_filename);
	serd_reader_free(reader);
	fclose(in_fd);
	serd_namespaces_free(state.ns);
	serd_string_free(state.base_uri_str);

	if (success) {
		return 0;
	}

	return 1;
}
