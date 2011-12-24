/*
  Copyright 2011 David Robillard <http://drobilla.net>

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

#include "serd_internal.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	SerdEnv*    env;
	SerdWriter* writer;
} State;

int
print_version()
{
	printf("serdi " SERD_VERSION " <http://drobilla.net/software/serd>\n");
	printf("Copyright 2011 David Robillard <http://drobilla.net>.\n"
	       "License: <http://www.opensource.org/licenses/isc-license>\n"
	       "This is free software; you are free to change and redistribute it."
	       "\nThere is NO WARRANTY, to the extent permitted by law.\n");
	return 0;
}

int
print_usage(const char* name, bool error)
{
	FILE* const os = error ? stderr : stdout;
	fprintf(os, "Usage: %s [OPTION]... INPUT [BASE_URI]\n", name);
	fprintf(os, "Read and write RDF syntax.\n");
	fprintf(os, "Use - for INPUT to read from standard input.\n\n");
	fprintf(os, "  -b           Fast bulk output for large serialisations.\n");
	fprintf(os, "  -f           Keep full URIs in input (don't qualify).\n");
	fprintf(os, "  -h           Display this help and exit\n");
	fprintf(os, "  -i SYNTAX    Input syntax (`turtle' or `ntriples')\n");
	fprintf(os, "  -o SYNTAX    Output syntax (`turtle' or `ntriples')\n");
	fprintf(os, "  -s INPUT     Parse INPUT as string (terminates options)\n");
	fprintf(os, "  -p PREFIX    Add PREFIX to blank node IDs\n");
	fprintf(os, "  -c PREFIX    Chop PREFIX from matching blank node IDs\n");
	fprintf(os, "  -v           Display version information and exit\n");
	return error ? 1 : 0;
}

bool
set_syntax(SerdSyntax* syntax, const char* name)
{
	if (!strcmp(name, "turtle")) {
		*syntax = SERD_TURTLE;
	} else if (!strcmp(name, "ntriples")) {
		*syntax = SERD_NTRIPLES;
	} else {
		fprintf(stderr, "Unknown input format `%s'\n", name);
		return false;
	}
	return true;
}

int
main(int argc, char** argv)
{
	if (argc < 2) {
		return print_usage(argv[0], true);
	}

	FILE*          in_fd         = NULL;
	SerdSyntax     input_syntax  = SERD_TURTLE;
	SerdSyntax     output_syntax = SERD_NTRIPLES;
	bool           from_file     = true;
	bool           bulk_write    = false;
	bool           full_uris     = false;
	const uint8_t* in_name       = NULL;
	const uint8_t* add_prefix    = NULL;
	const uint8_t* chop_prefix   = NULL;
	int a = 1;
	for (; a < argc && argv[a][0] == '-'; ++a) {
		if (argv[a][1] == '\0') {
			in_name = (const uint8_t*)"(stdin)";
			in_fd   = stdin;
			break;
		} else if (argv[a][1] == 'b') {
			bulk_write = true;
		} else if (argv[a][1] == 'f') {
			full_uris = true;
		} else if (argv[a][1] == 'h') {
			return print_usage(argv[0], false);
		} else if (argv[a][1] == 'v') {
			return print_version();
		} else if (argv[a][1] == 's') {
			in_name = (const uint8_t*)"(string)";
			from_file = false;
			++a;
			break;
		} else if (argv[a][1] == 'i') {
			if (++a == argc) {
				fprintf(stderr, "Missing value for -i\n");
				return 1;
			}
			if (!set_syntax(&input_syntax, argv[a])) {
				return 1;
			}
		} else if (argv[a][1] == 'o') {
			if (++a == argc) {
				fprintf(stderr, "Missing value for -o\n");
				return 1;
			}
			if (!set_syntax(&output_syntax, argv[a])) {
				return 1;
			}
		} else if (argv[a][1] == 'p') {
			if (++a == argc) {
				fprintf(stderr, "Missing value for -p\n");
				return 1;
			}
			add_prefix = (const uint8_t*)argv[a];
		} else if (argv[a][1] == 'c') {
			if (++a == argc) {
				fprintf(stderr, "Missing value for -c\n");
				return 1;
			}
			chop_prefix = (const uint8_t*)argv[a];
		} else {
			fprintf(stderr, "Unknown option `%s'\n", argv[a]);
			return print_usage(argv[0], true);
		}
	}

	if (a == argc) {
		fprintf(stderr, "Missing input\n");
		return 1;
	}

	const uint8_t* input = (const uint8_t*)argv[a++];
	if (from_file) {
		in_name = in_name ? in_name : input;
		if (!in_fd) {
			input = serd_uri_to_path(in_name);
			if (!(in_fd = serd_fopen((const char*)input, "r"))) {
				return 1;
			}
		}
	}

	const uint8_t* base_uri_str = NULL;
	if (a < argc) {  // Base URI given on command line
		base_uri_str = (const uint8_t*)argv[a];
	} else if (from_file) {  // Use input file URI
		base_uri_str = input;
	} else {
		base_uri_str = (const uint8_t*)"";
	}

	SerdURI  base_uri = SERD_URI_NULL;
	SerdNode base_uri_node = serd_node_new_uri_from_string(
		base_uri_str, &base_uri, &base_uri);

	FILE*    out_fd = stdout;
	SerdEnv* env    = serd_env_new(&base_uri_node);

	SerdStyle output_style = 0;
	if (output_syntax == SERD_NTRIPLES) {
		output_style |= SERD_STYLE_ASCII;
	} else {
		output_style |= SERD_STYLE_ABBREVIATED;
		if (!full_uris) {
			output_style |= SERD_STYLE_CURIED;
		}
	}

	if (input_syntax != SERD_NTRIPLES) {  // Base URI may change (@base)
		output_style |= SERD_STYLE_RESOLVED;
	}

	if (bulk_write) {
		output_style |= SERD_STYLE_BULK;
	}

	SerdWriter* writer = serd_writer_new(
		output_syntax, output_style, env, &base_uri, serd_file_sink, out_fd);

	if (chop_prefix) {
		serd_writer_chop_blank_prefix(writer, chop_prefix);
	}

	State state = { env, writer };

	SerdReader* reader = serd_reader_new(
		input_syntax, state.writer, NULL,
		(SerdBaseSink)serd_writer_set_base_uri,
		(SerdPrefixSink)serd_writer_set_prefix,
		(SerdStatementSink)serd_writer_write_statement,
		(SerdEndSink)serd_writer_end_anon);

	if (add_prefix) {
		serd_reader_add_blank_prefix(reader, add_prefix);
	}

	const SerdStatus status = (from_file)
		? serd_reader_read_file_handle(reader, in_fd, in_name)
		: serd_reader_read_string(reader, input);

	serd_reader_free(reader);

	if (from_file) {
		fclose(in_fd);
	}

	serd_writer_finish(state.writer);
	serd_writer_free(state.writer);
	serd_env_free(state.env);
	serd_node_free(&base_uri_node);

	return (status == SERD_SUCCESS) ? 0 : 1;
}
