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

#include "reader.h"
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
#include <string.h>

#define SERDI_ERROR(msg)       fprintf(stderr, "serdi: " msg)
#define SERDI_ERRORF(fmt, ...) fprintf(stderr, "serdi: " fmt, __VA_ARGS__)

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
	fprintf(os, "  -V           Validate inputs.\n");
	fprintf(os, "  -a           Write ASCII output if possible.\n");
	fprintf(os, "  -b           Fast bulk output for large serialisations.\n");
	fprintf(os, "  -c PREFIX    Chop PREFIX from matching blank node IDs.\n");
	fprintf(os, "  -e           Eat input one character at a time.\n");
	fprintf(os, "  -f           Fast serialisation without inlining.\n");
	fprintf(os, "  -g PATTERN   Grep statements matching PATTERN.\n");
	fprintf(os, "  -h           Display this help and exit.\n");
	fprintf(os, "  -i SYNTAX    Input syntax: turtle/ntriples/trig/nquads.\n");
	fprintf(os, "  -k BYTES     Parser stack size.\n");
	fprintf(os, "  -l           Lax (non-strict) parsing.\n");
	fprintf(os, "  -m           Build and serialise a model (no streaming).\n");
	fprintf(os, "  -n           Normalise literals.\n");
	fprintf(os, "  -o SYNTAX    Output syntax: turtle/ntriples/nquads.\n");
	fprintf(os, "  -p PREFIX    Add PREFIX to blank node IDs.\n");
	fprintf(os, "  -q           Suppress all output except data.\n");
	fprintf(os, "  -r ROOT_URI  Keep relative URIs within ROOT_URI.\n");
	fprintf(os, "  -s INPUT     Parse INPUT as string.\n");
	fprintf(os, "  -t           Write terser output without newlines.\n");
	fprintf(os, "  -v           Display version information and exit.\n");
	fprintf(os, "  -x           Support parsing variable nodes like `?x'.\n");
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

static SerdStatus
on_filter_statement(void*                handle,
                    SerdStatementFlags   flags,
                    const SerdStatement* statement)
{
	(void)flags;

	serd_filter_set_statement((SerdFilter*)handle,
	                          serd_statement_get_subject(statement),
	                          serd_statement_get_predicate(statement),
	                          serd_statement_get_object(statement),
	                          serd_statement_get_graph(statement));

	return SERD_SUCCESS;
}

static SerdFilter*
parse_filter(SerdWorld* world, const SerdSink* sink, const char* str)
{
	SerdFilter* filter  = serd_filter_new(sink);
	SerdSink*   in_sink = serd_sink_new(filter, NULL, NULL);

	serd_sink_set_statement_func(in_sink, on_filter_statement);

	SerdReader* reader = serd_reader_new(
	        world, SERD_NQUADS, SERD_READ_VARIABLES, in_sink, 4096);

	serd_reader_start_string(reader, str, NULL);
	serd_reader_read_document(reader);

	serd_reader_free(reader);
	serd_sink_free(in_sink);

	return filter;
}

static SerdStatus
read_file(SerdWorld* const      world,
          SerdSyntax            syntax,
          const SerdReaderFlags flags,
          const SerdSink* const sink,
          const size_t          stack_size,
          const char*           filename,
          const char*           add_prefix,
          bool                  bulk_read)
{
	syntax = syntax ? syntax : serd_guess_syntax(filename);
	syntax = syntax ? syntax : SERD_TRIG;

	SerdStatus  st = SERD_SUCCESS;
	SerdReader* reader =
	        serd_reader_new(world, syntax, flags, sink, stack_size);

	serd_reader_add_blank_prefix(reader, add_prefix);

	if (!strcmp(filename, "-")) {
		SerdNode* name = serd_new_string("stdin");
		st             = serd_reader_start_stream(reader,
		                                          serd_file_read_byte,
		                                          (SerdStreamErrorFunc)ferror,
		                                          stdin,
		                                          name,
		                                          1);
		serd_node_free(name);
	} else {
		st = serd_reader_start_file(reader, filename, bulk_read);
	}

	st = st ? st : serd_reader_read_document(reader);

	serd_reader_free(reader);

	return st;
}

int
main(int argc, char** argv)
{
	if (argc < 2) {
		return print_usage(argv[0], true);
	}

	SerdNode*       base          = NULL;
	SerdSyntax      input_syntax  = SERD_SYNTAX_EMPTY;
	SerdSyntax      output_syntax = SERD_SYNTAX_EMPTY;
	SerdReaderFlags reader_flags  = 0;
	SerdWriterFlags writer_flags  = 0;
	bool            bulk_read     = true;
	bool            bulk_write    = false;
	bool            no_inline     = false;
	bool            osyntax_set   = false;
	bool            validate      = false;
	bool            use_model     = false;
	bool            normalise     = false;
	bool            quiet         = false;
	size_t          stack_size    = 4194304;
	const char*     input_string  = NULL;
	const char*     pattern       = NULL;
	const char*     add_prefix    = "";
	const char*     chop_prefix   = NULL;
	const char*     root_uri      = NULL;
	int             a             = 1;
	for (; a < argc && argv[a][0] == '-'; ++a) {
		if (argv[a][1] == '\0') {
			break;
		} else if (argv[a][1] == 'I') {
			if (++a == argc) {
				return missing_arg(argv[0], 'I');
			}
			base = serd_new_uri(argv[a]);
		} else if (argv[a][1] == 'V') {
			validate = use_model = true;
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
		} else if (argv[a][1] == 'n') {
			normalise = true;
		} else if (argv[a][1] == 'g') {
			if (++a == argc) {
				return missing_arg(argv[0], 's');
			}
			pattern = argv[a];
		} else if (argv[a][1] == 'q') {
			quiet = true;
		} else if (argv[a][1] == 'v') {
			return print_version();
		} else if (argv[a][1] == 'x') {
			reader_flags |= SERD_READ_VARIABLES;
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
			osyntax_set = true;
			if (++a == argc) {
				return missing_arg(argv[0], 'o');
			} else if (!strcmp(argv[a], "empty")) {
				output_syntax = SERD_SYNTAX_EMPTY;
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

	bool input_has_graphs = serd_syntax_has_graphs(input_syntax);
	for (int i = a; i < argc; ++i) {
		if (serd_syntax_has_graphs(serd_guess_syntax(argv[i]))) {
			input_has_graphs = true;
			break;
		}
	}

	if (!output_syntax && !osyntax_set) {
		output_syntax = input_has_graphs ? SERD_NQUADS : SERD_NTRIPLES;
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

	SerdModel*      model    = NULL;
	SerdInserter*   inserter = NULL;
	const SerdSink* out_sink = NULL;
	if (use_model) {
		const SerdModelFlags flags =
		        SERD_INDEX_SPO | (input_has_graphs ? SERD_INDEX_GRAPHS : 0U) |
		        (no_inline ? 0U : SERD_INDEX_OPS) |
		        (validate ? SERD_STORE_CURSORS : 0U);

		model    = serd_model_new(world, flags);
		inserter = serd_inserter_new(model, env, NULL);
		out_sink = serd_inserter_get_sink(inserter);
	} else {
		out_sink = serd_writer_get_sink(writer);
	}

	const SerdSink* sink = out_sink;

	SerdNormaliser* normaliser = NULL;
	if (normalise) {
		normaliser = serd_normaliser_new(out_sink);
		sink       = serd_normaliser_get_sink(normaliser);
	}

	SerdFilter* filter = NULL;
	if (pattern) {
		filter = parse_filter(world, sink, pattern);
		sink   = serd_filter_get_sink(filter);
	}

	if (quiet) {
		serd_world_set_log_func(world, quiet_error_func, NULL);
	}

	SerdNode* root = serd_new_uri(root_uri);
	serd_writer_set_root_uri(writer, root);
	serd_writer_chop_blank_prefix(writer, chop_prefix);
	serd_node_free(root);

	SerdStatus st         = SERD_SUCCESS;
	SerdNode*  input_name = NULL;
	if (input_string) {
		SerdReader* reader =
		        serd_reader_new(world,
		                        input_syntax ? input_syntax : SERD_TRIG,
		                        reader_flags,
		                        sink,
		                        stack_size);
		serd_reader_add_blank_prefix(reader, add_prefix);

		SerdNode* name = serd_new_string("string");
		if (!(st = serd_reader_start_string(reader, input_string, name))) {
			st = serd_reader_read_document(reader);
		}
		serd_node_free(name);
		serd_reader_free(reader);
	}

	char** inputs     = argv + a;
	int    n_inputs   = argc - a;
	size_t prefix_len = 0;
	char*  prefix     = NULL;
	if (n_inputs > 1) {
		prefix_len = 8 + strlen(add_prefix);
		prefix     = (char*)calloc(1, prefix_len);
	}

	for (int i = 0; i < n_inputs; ++i) {
		if (!base) {
			SerdNode* file_uri = serd_new_file_uri(inputs[i], NULL);
			serd_env_set_base_uri(env, file_uri);
			serd_node_free(file_uri);
		}

		if (n_inputs > 1) {
			snprintf(prefix, prefix_len, "f%d%s", i, add_prefix);
		}

		if ((st = read_file(world,
		                    input_syntax,
		                    reader_flags,
		                    sink,
		                    stack_size,
		                    inputs[i],
		                    n_inputs > 1 ? prefix : add_prefix,
		                    bulk_read))) {
			break;
		}
	}
	free(prefix);

	if (!st && validate) {
		st = serd_validate(model);
	}

	if (st <= SERD_FAILURE && use_model) {
		const SerdSink* wsink = serd_writer_get_sink(writer);
		serd_env_write_prefixes(env, wsink);

		SerdRange* range = serd_model_all(model);
		st = serd_range_serialise(range, wsink, serialisation_flags);
		serd_range_free(range);
	}

	serd_filter_free(filter);
	serd_normaliser_free(normaliser);
	serd_node_free(input_name);
	serd_inserter_free(inserter);
	serd_model_free(model);
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
