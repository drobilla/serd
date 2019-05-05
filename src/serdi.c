/*
  Copyright 2011-2017 David Robillard <http://drobilla.net>

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

#include "serd_config.h"
#include "system.h"

#include "serd/serd.h"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define SERDI_ERROR(msg)       fprintf(stderr, "serdi: " msg);
#define SERDI_ERRORF(fmt, ...) fprintf(stderr, "serdi: " fmt, __VA_ARGS__);

static int
print_version(void)
{
	printf("serdi " SERD_VERSION " <http://drobilla.net/software/serd>\n");
	printf("Copyright 2011-2017 David Robillard <http://drobilla.net>.\n"
	       "License: <http://www.opensource.org/licenses/isc>\n"
	       "This is free software; you are free to change and redistribute it."
	       "\nThere is NO WARRANTY, to the extent permitted by law.\n");
	return 0;
}

static int
print_usage(const char* name, bool error)
{
	FILE* const os = error ? stderr : stdout;
	fprintf(os, "%s", error ? "\n" : "");
	fprintf(os, "Usage: %s [OPTION]... INPUT...\n", name);
	fprintf(os, "Read and write RDF syntax.\n");
	fprintf(os, "Use - for INPUT to read from standard input.\n\n");
	fprintf(os, "  -I BASE_URI  Input base URI.\n");
	fprintf(os, "  -a           Write ASCII output if possible.\n");
	fprintf(os, "  -b           Fast bulk output for large serialisations.\n");
	fprintf(os, "  -c PREFIX    Chop PREFIX from matching blank node IDs.\n");
	fprintf(os, "  -e           Eat input one character at a time.\n");
	fprintf(os, "  -f           Fast serialisation without inlining.\n");
	fprintf(os, "  -h           Display this help and exit.\n");
	fprintf(os, "  -i SYNTAX    Input syntax: turtle/ntriples/trig/nquads.\n");
	fprintf(os, "  -k BYTES     Parser stack size.\n");
	fprintf(os, "  -l           Lax (non-strict) parsing.\n");
	fprintf(os, "  -m           Build and serialise a model (no streaming).\n");
	fprintf(os, "  -o SYNTAX    Output syntax: turtle/ntriples/nquads.\n");
	fprintf(os, "  -p PREFIX    Add PREFIX to blank node IDs.\n");
	fprintf(os, "  -q           Suppress all output except data.\n");
	fprintf(os, "  -r ROOT_URI  Keep relative URIs within ROOT_URI.\n");
	fprintf(os, "  -s INPUT     Parse INPUT as string.\n");
	fprintf(os, "  -t           Write terser output without newlines.\n");
	fprintf(os, "  -v           Display version information and exit.\n");
	return error ? 1 : 0;
}

static int
missing_arg(const char* name, char opt)
{
	SERDI_ERRORF("option requires an argument -- '%c'\n", opt);
	return print_usage(name, true);
}

static SerdStatus
quiet_error_func(void* handle, const SerdLogEntry* entry)
{
	(void)handle;
	(void)entry;
	return SERD_SUCCESS;
}

int
main(int argc, char** argv)
{
	if (argc < 2) {
		return print_usage(argv[0], true);
	}

	SerdNode*       base          = NULL;
	SerdSyntax      input_syntax  = (SerdSyntax)0;
	SerdSyntax      output_syntax = (SerdSyntax)0;
	SerdReaderFlags reader_flags  = 0;
	SerdWriterFlags writer_flags  = 0;
	bool            from_stdin    = false;
	bool            bulk_read     = true;
	bool            bulk_write    = false;
	bool            no_inline     = false;
	bool            use_model     = false;
	bool            quiet         = false;
	size_t          stack_size    = 4194304;
	const char*     input_string  = NULL;
	const char*     add_prefix    = NULL;
	const char*     chop_prefix   = NULL;
	const char*     root_uri      = NULL;
	int             a             = 1;
	for (; a < argc && argv[a][0] == '-'; ++a) {
		if (argv[a][1] == '\0') {
			from_stdin = true;
			break;
		} else if (argv[a][1] == 'I') {
			if (++a == argc) {
				return missing_arg(argv[0], 'I');
			}
			base = serd_new_uri(argv[a]);
		} else if (argv[a][1] == 'a') {
			writer_flags |= SERD_WRITE_ASCII;
		} else if (argv[a][1] == 'b') {
			bulk_write = true;
		} else if (argv[a][1] == 'e') {
			bulk_read = false;
		} else if (argv[a][1] == 'f') {
			no_inline = true;
		} else if (argv[a][1] == 'h') {
			return print_usage(argv[0], false);
		} else if (argv[a][1] == 'l') {
			reader_flags |= SERD_READ_LAX;
			writer_flags |= SERD_WRITE_LAX;
		} else if (argv[a][1] == 'm') {
			use_model = true;
		} else if (argv[a][1] == 'q') {
			quiet = true;
		} else if (argv[a][1] == 'v') {
			return print_version();
		} else if (argv[a][1] == 's') {
			if (++a == argc) {
				return missing_arg(argv[0], 's');
			}
			input_string = argv[a];
		} else if (argv[a][1] == 't') {
			writer_flags |= SERD_WRITE_TERSE;
		} else if (argv[a][1] == 'i') {
			if (++a == argc) {
				return missing_arg(argv[0], 'i');
			} else if (!(input_syntax = serd_syntax_by_name(argv[a]))) {
				return print_usage(argv[0], true);
			}
		} else if (argv[a][1] == 'k') {
			if (++a == argc) {
				return missing_arg(argv[0], 'k');
			}
			char*      endptr = NULL;
			const long size   = strtol(argv[a], &endptr, 10);
			if (size <= 0 || size == LONG_MAX || *endptr != '\0') {
				SERDI_ERRORF("invalid stack size `%s'\n", argv[a]);
				return 1;
			}
			stack_size = (size_t)size;
		} else if (argv[a][1] == 'o') {
			if (++a == argc) {
				return missing_arg(argv[0], 'o');
			} else if (!(output_syntax = serd_syntax_by_name(argv[a]))) {
				return print_usage(argv[0], true);
			}
		} else if (argv[a][1] == 'p') {
			if (++a == argc) {
				return missing_arg(argv[0], 'p');
			}
			add_prefix = argv[a];
		} else if (argv[a][1] == 'c') {
			if (++a == argc) {
				return missing_arg(argv[0], 'c');
			}
			chop_prefix = argv[a];
		} else if (argv[a][1] == 'r') {
			if (++a == argc) {
				return missing_arg(argv[0], 'r');
			}
			root_uri = argv[a];
		} else {
			SERDI_ERRORF("invalid option -- '%s'\n", argv[a] + 1);
			return print_usage(argv[0], true);
		}
	}

	if (a == argc && !input_string) {
		SERDI_ERROR("missing input\n");
		return 1;
	}

#ifdef _WIN32
	_setmode(_fileno(stdin), _O_BINARY);
	_setmode(_fileno(stdout), _O_BINARY);
#endif

	const char* input = argv[a++];

	if (!input_syntax && !input || !(input_syntax = serd_guess_syntax(input))) {
		input_syntax = SERD_TRIG;
	}

	const bool input_has_graphs = serd_syntax_has_graphs(input_syntax);
	if (!output_syntax) {
		output_syntax = input_has_graphs ? SERD_NQUADS : SERD_NTRIPLES;
	}

	if (!base && input) {
		base = serd_new_file_uri(input, NULL);
	}

	FILE*      out_fd = stdout;
	SerdWorld* world  = serd_world_new();
	SerdEnv*   env    = serd_env_new(base);

	const SerdSerialisationFlags serialisation_flags =
		no_inline ? SERD_NO_INLINE_OBJECTS : 0U;

	SerdByteSink* byte_sink = serd_byte_sink_new(
		(SerdWriteFunc)fwrite, out_fd, bulk_write ? 4096U : 1U);

	SerdWriter* writer = serd_writer_new(world,
	                                     output_syntax,
	                                     writer_flags,
	                                     env,
	                                     (SerdWriteFunc)serd_byte_sink_write,
	                                     byte_sink);

	SerdReader*     reader   = NULL;
	SerdModel*      model    = NULL;
	SerdInserter*   inserter = NULL;
	const SerdSink* sink     = NULL;
	if (use_model) {
		const SerdModelFlags flags =
		        SERD_INDEX_SPO | (input_has_graphs ? SERD_INDEX_GRAPHS : 0U) |
		        (no_inline ? 0U : SERD_INDEX_OPS);
		model    = serd_model_new(world, flags);
		inserter = serd_inserter_new(model, env, NULL);
		sink     = serd_inserter_get_sink(inserter);
	} else {
		sink = serd_writer_get_sink(writer);
	}

	reader = serd_reader_new(
		world, input_syntax, reader_flags, sink, stack_size);

	if (quiet) {
		serd_world_set_log_func(world, quiet_error_func, NULL);
	}

	SerdNode* root = serd_new_uri(root_uri);
	serd_writer_set_root_uri(writer, root);
	serd_writer_chop_blank_prefix(writer, chop_prefix);
	serd_reader_add_blank_prefix(reader, add_prefix);
	serd_node_free(root);

	SerdStatus st         = SERD_SUCCESS;
	SerdNode*  input_name = NULL;
	if (input_string) {
		input_name = serd_new_string("string");
		st         = serd_reader_start_string(reader, input_string, input_name);
	} else if (from_stdin) {
		input_name = serd_new_string("stdin");
		st         = serd_reader_start_stream(reader,
		                                      serd_file_read_byte,
		                                      (SerdStreamErrorFunc)ferror,
		                                      stdin,
		                                      input_name,
		                                      1);
	} else {
		st = serd_reader_start_file(reader, input, bulk_read);
	}

	if (!st) {
		st = serd_reader_read_document(reader);
	}

	serd_reader_finish(reader);

	if (st <= SERD_FAILURE && use_model) {
		const SerdSink* wsink = serd_writer_get_sink(writer);
		serd_env_write_prefixes(env, wsink);

		SerdRange* range = serd_model_all(model);
		st = serd_range_serialise(range, wsink, serialisation_flags);
		serd_range_free(range);
	}

	serd_node_free(input_name);
	serd_inserter_free(inserter);
	serd_model_free(model);
	serd_reader_free(reader);
	serd_writer_free(writer);
	serd_byte_sink_free(byte_sink);
	serd_env_free(env);
	serd_node_free(base);
	serd_world_free(world);

	if (fclose(stdout)) {
		perror("serdi: write error");
		st = SERD_ERR_UNKNOWN;
	}

	return (st > SERD_FAILURE) ? 1 : 0;
}
