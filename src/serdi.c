/*
  Copyright 2011 David Robillard <http://drobilla.net>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
  AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
  AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
  OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
  THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "serd/serd.h"
#include "serd-config.h"

typedef struct {
	SerdEnv       env;
	SerdReadState read_state;
	SerdWriter    writer;
} State;

static bool
event_base(void*           handle,
           const SerdNode* uri_node)
{
	State* const state = (State*)handle;

	if (serd_read_state_set_base_uri(state->read_state, uri_node)) {
		SerdURI base_uri;
		serd_read_state_get_base_uri(state->read_state, &base_uri);
		serd_writer_set_base_uri(state->writer, &base_uri);
		return true;
	}
	return false;
}

static bool
event_prefix(void*           handle,
             const SerdNode* name,
             const SerdNode* uri_node)
{
	State* const state = (State*)handle;
	
	serd_read_state_set_prefix(state->read_state, name, uri_node);
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
			in_fd = fopen((const char*)input, "r");
			if (!in_fd) {
				fprintf(stderr, "failed to open file %s\n", input);
				return 1;
			}
		}
	}

	const uint8_t* base_uri_str = NULL;
	SerdURI        base_uri;
	if (a < argc) {  // Base URI given on command line
		const uint8_t* const in_base_uri = (const uint8_t*)argv[a++];
		if (!serd_uri_parse((const uint8_t*)in_base_uri, &base_uri)) {
			fprintf(stderr, "invalid base URI `%s'\n", argv[2]);
			return 1;
		}
		base_uri_str = in_base_uri;
	} else if (from_file) {  // Use input file URI
		base_uri_str = input;
	} else {
		base_uri_str = (const uint8_t*)"";
	}

	if (!serd_uri_parse(base_uri_str, &base_uri)) {
		fprintf(stderr, "invalid base URI `%s'\n", base_uri_str);
	}

	FILE*   out_fd = stdout;
	SerdEnv env    = serd_env_new();

	SerdStyle output_style = SERD_STYLE_RESOLVED;
	if (output_syntax == SERD_NTRIPLES) {
		output_style |= SERD_STYLE_ASCII;
	} else {
		output_style |= SERD_STYLE_ABBREVIATED;
	}

	SerdReadState read_state = serd_read_state_new(env, base_uri_str);

	serd_read_state_get_base_uri(read_state, &base_uri);

	SerdWriter writer = serd_writer_new(
		output_syntax, output_style, env, &base_uri, file_sink, out_fd);

	State state = { env, read_state, writer };

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
	serd_read_state_free(state.read_state);
	serd_env_free(state.env);

	if (success) {
		return 0;
	}

	return 1;
}
