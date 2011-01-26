/* Serd, a lightweight RDF syntax library.
 * Copyright 2011 David Robillard <d@drobilla.net>
 * 
 * Serd is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Serd is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "serd/serd.h"
#include "serd-config.h"

typedef struct {
	SerdWriter writer;
	SerdEnv    env;
	SerdNode   base_uri_node;
	SerdURI    base_uri;
} State;

static uint8_t*
copy_string(const uint8_t* str, size_t* n_bytes)
{
	const size_t   len = strlen((const char*)str);
	uint8_t* const ret = malloc(len + 1);
	memcpy(ret, str, len + 1);
	*n_bytes = len + 1;
	return ret;
}

static bool
event_base(void*           handle,
           const SerdNode* uri_node)
{
	State* const state         = (State*)handle;
	SerdNode     base_uri_node = *uri_node;
	SerdURI      base_uri;
	if (!serd_uri_parse(uri_node->buf, &base_uri)) {
		return false;
	}

	SerdURI abs_base_uri;
	if (!serd_uri_resolve(&base_uri, &state->base_uri, &abs_base_uri)) {
		fprintf(stderr, "error: failed to resolve new base URI\n");
		return false;
	}
	base_uri_node = serd_node_new_uri(&abs_base_uri, &base_uri);

	serd_node_free(&state->base_uri_node);
	state->base_uri_node = base_uri_node;
	state->base_uri      = base_uri;
	serd_writer_set_base_uri(state->writer, &base_uri);
	return true;
}

static bool
event_prefix(void*           handle,
             const SerdNode* name,
             const SerdNode* uri_node)
{
	State* const state = (State*)handle;
	if (!serd_uri_string_has_scheme(uri_node->buf)) {
		SerdURI uri;
		if (!serd_uri_parse(uri_node->buf, &uri)) {
			return false;
		}
		SerdURI abs_uri;
		if (!serd_uri_resolve(&uri, &state->base_uri, &abs_uri)) {
			return false;
		}
		SerdURI  base_uri;
		SerdNode base_uri_node = serd_node_new_uri(&abs_uri, &base_uri);
		serd_env_add(state->env, name, &base_uri_node);
		serd_node_free(&base_uri_node);
	} else {
		serd_env_add(state->env, name, uri_node);
	}
	serd_writer_set_prefix(state->writer, name, uri_node);
	return true;
}

static bool
event_statement(void*           handle,
                const SerdNode* graph,
                const SerdNode* subject,
                const SerdNode* predicate,
                const SerdNode* object,
                const SerdNode* object_datatype,
                const SerdNode* object_lang)
{
	return serd_writer_write_statement(
		((State*)handle)->writer,
		graph, subject, predicate, object, object_datatype, object_lang);
}

static bool
event_end(void*           handle,
          const SerdNode* node)
{
	State* const state = (State*)handle;
	return serd_writer_end_anon(state->writer, node);
}

int
print_version()
{
	printf("serdi " SERD_VERSION " <http://drobilla.net/software/serd>\n");
	printf("Copyright (C) 2011 David Robillard <http://drobilla.net>.\n"
	       "\nLicense: GNU LGPL version 3 or later "
	       "<http://gnu.org/licenses/lgpl.html>.\n"
	       "This is free software; you are free to change and redistribute it."
	       "\nThere is NO WARRANTY, to the extent permitted by law.\n");
	return 0;
}

int
print_usage(const char* name, bool error)
{
	FILE* const os = error ? stderr : stdout;
	fprintf(os, "Usage: %s [OPTION]... INPUT [BASE_URI]\n", name);
	fprintf(os, "Read and write RDF syntax.\n\n");
	fprintf(os, "  -h           Display this help and exit\n");
	fprintf(os, "  -o SYNTAX    Output syntax (`turtle' or `ntriples')\n");
	fprintf(os, "  -s INPUT     Parse INPUT as string (terminates options)\n");
	fprintf(os, "  -v           Display version information and exit\n");
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
	if (argc < 2) {
		return print_usage(argv[0], true);
	}

	FILE*       in_fd         = NULL;
	SerdSyntax  output_syntax = SERD_NTRIPLES;
	bool        from_file     = true;
	const char* in_name       = NULL;
	int a = 1;
	for (; a < argc && argv[a][0] == '-'; ++a) {
		if (argv[a][1] == '\0') {
			in_name = "(stdin)";
			in_fd   = stdin;
			break;
		} else if (argv[a][1] == 'h') {
			return print_usage(argv[0], false);
		} else if (argv[a][1] == 'v') {
			return print_version();
		} else if (argv[a][1] == 's') {
			in_name = "(string)";
			from_file = false;
			++a;
			break;
		} else if (argv[a][1] == 'o') {
			if (++a == argc) {
				fprintf(stderr, "missing value for -o\n");
				return 1;
			}
			if (!strcmp(argv[a], "turtle")) {
				output_syntax = SERD_TURTLE;
			} else if (!strcmp(argv[a], "ntriples")) {
				output_syntax = SERD_NTRIPLES;
			} else {
				fprintf(stderr, "unknown output format `%s'\n",  argv[a]);
				return 1;
			}
		} else {
			fprintf(stderr, "unknown option `%s'\n", argv[a]);
			return print_usage(argv[0], true);
		}
	}

	const uint8_t* input = (const uint8_t*)argv[a++];
	if (from_file) {
		in_name = in_name ? in_name : (const char*)input;
		if (!in_fd) {
			if (serd_uri_string_has_scheme(input)) {
				// INPUT is an absolute URI, ensure it a file and chop scheme
				if (strncmp((const char*)input, "file:", 5)) {
					fprintf(stderr, "unsupported URI scheme `%s'\n", input);
					return 1;
				} else if (!strncmp((const char*)input, "file://", 7)) {
					input += 7;
				} else {
					input += 5;
				}
			}
			in_fd  = fopen((const char*)input,  "r");
			if (!in_fd) {
				fprintf(stderr, "failed to open file %s\n", input);
				return 1;
			}
		}
	}

	uint8_t* base_uri_str     = NULL;
	size_t   base_uri_n_bytes = 0;
	SerdURI  base_uri;
	if (a < argc) {  // Base URI given on command line
		const uint8_t* const in_base_uri = (const uint8_t*)argv[a++];
		if (!serd_uri_parse((const uint8_t*)in_base_uri, &base_uri)) {
			fprintf(stderr, "invalid base URI `%s'\n", argv[2]);
			return 1;
		}
		base_uri_str = copy_string(in_base_uri, &base_uri_n_bytes);
	} else if (from_file) {  // Use input file URI
		base_uri_str = copy_string(input, &base_uri_n_bytes);
	} else {
		base_uri_str = copy_string((const uint8_t*)"", &base_uri_n_bytes);
	}

	if (!serd_uri_parse(base_uri_str, &base_uri)) {
		fprintf(stderr, "invalid base URI `%s'\n", base_uri_str);
	}

	FILE*   out_fd = stdout;
	SerdEnv env    = serd_env_new();

	SerdStyle output_style = (output_syntax == SERD_NTRIPLES)
		? SERD_STYLE_ASCII
		: SERD_STYLE_ABBREVIATED;

	const SerdNode base_uri_node = { SERD_URI,
	                                 base_uri_n_bytes,
	                                 base_uri_n_bytes - 1,
	                                 base_uri_str };

	State state = {
		serd_writer_new(output_syntax, output_style,
		                env, &base_uri, file_sink, out_fd),
		env, base_uri_node, base_uri
	};

	SerdReader reader = serd_reader_new(
		SERD_TURTLE, &state,
		event_base, event_prefix, event_statement, event_end);

	const bool success = (from_file)
		? serd_reader_read_file(reader, in_fd, input)
		: serd_reader_read_string(reader, input);

	serd_reader_free(reader);

	if (from_file) {
		fclose(in_fd);
	}

	serd_writer_finish(state.writer);
	serd_writer_free(state.writer);

	serd_env_free(state.env);

	serd_node_free(&state.base_uri_node);

	if (success) {
		return 0;
	}

	return 1;
}
