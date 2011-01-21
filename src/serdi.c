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
	SerdWriter     writer;
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

	SerdURI     base_uri = SERD_URI_NULL;
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
	state->base_uri_str     = base_uri_str;
	state->base_uri         = base_uri;
	serd_writer_set_base_uri(state->writer, &base_uri);

	return true;
}

static bool
event_prefix(void*             handle,
             const SerdString* name,
             const SerdString* uri_string)
{
	State* const state = (State*)handle;
	if (!serd_uri_string_has_scheme(uri_string->buf)) {
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
	return serd_writer_write_statement(
		state->writer,
		graph,
		subject,   subject_type,
		predicate, predicate_type,
		object,    object_type,    object_datatype, object_lang);
}

int
print_usage(const char* name, bool error)
{
	FILE* const os = error ? stderr : stdout;
	fprintf(os, "Usage: %s INPUT [BASE_URI]\n", name);
	fprintf(os, "Read and write RDF syntax.\n");
	return error ? 1 : 0;
}

static size_t
file_sink(const void* buf, size_t len, void* stream)
{
	FILE* file = (FILE*)stream;
	return fwrite(buf, 1, len, file);
}

int
main(int argc, char** argv)
{
	if (argc != 2 && argc != 3) {
		return print_usage(argv[0], true);
	}

	const uint8_t* in_filename = (const uint8_t*)argv[1];

	if (serd_uri_string_has_scheme(in_filename)) {
		// Input is an absolute URI, ensure it's a file: URI and chop scheme
		if (strncmp((const char*)in_filename, "file:", 5)) {
			fprintf(stderr, "unsupported URI scheme `%s'\n", in_filename);
			return 1;
		} else if (!strncmp((const char*)in_filename, "file:///", 7)) {
			in_filename += 7;
		} else {
			in_filename += 5;
		}
	}

	SerdString* base_uri_str = NULL;
	SerdURI     base_uri;
	if (argc > 2) {  // Base URI given on command line
		const uint8_t* const in_base_uri = (const uint8_t*)argv[2];
		if (!serd_uri_parse((const uint8_t*)in_base_uri, &base_uri)) {
			fprintf(stderr, "invalid base URI `%s'\n", argv[2]);
			return 1;
		}
		base_uri_str = serd_string_new(in_base_uri);
	} else {  // Use input file URI
		base_uri_str = serd_string_new(in_filename);
	}

	serd_uri_parse(base_uri_str->buf, &base_uri);

	FILE* const in_fd  = fopen((const char*)in_filename,  "r");
	FILE*       out_fd = stdout;

	if (!in_fd) {
		fprintf(stderr, "failed to open file %s\n", in_filename);
		return 1;
	}

	SerdNamespaces ns = serd_namespaces_new();
	State state = {
		serd_writer_new(SERD_NTRIPLES, ns, &base_uri, file_sink, out_fd),
		ns, base_uri_str, base_uri
	};

	SerdReader reader = serd_reader_new(
		SERD_TURTLE, &state, event_base, event_prefix, event_statement);

	const bool success = serd_reader_read_file(reader, in_fd, in_filename);
	serd_reader_free(reader);
	fclose(in_fd);
	serd_namespaces_free(state.ns);
	serd_writer_free(state.writer);
	serd_string_free(state.base_uri_str);

	if (success) {
		return 0;
	}

	return 1;
}
