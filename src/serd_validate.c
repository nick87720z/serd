/*
  Copyright 2012-2019 David Robillard <http://drobilla.net>

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

#define _BSD_SOURCE 1     // for realpath
#define _DEFAULT_SOURCE 1 // for realpath

#include "serd_config.h"

#include "serd/serd.h"

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define CERRORF(fmt, ...) fprintf(stderr, "serd_validate: " fmt, __VA_ARGS__);

static int
print_version(void)
{
	printf("serd_validate " SERD_VERSION
	       " <http://drobilla.net/software/serd>\n");
	printf("Copyright 2012-2019 David Robillard <http://drobilla.net>.\n"
	       "License: <http://www.opensource.org/licenses/isc>\n"
	       "This is free software; you are free to change and redistribute it."
	       "\nThere is NO WARRANTY, to the extent permitted by law.\n");
	return 0;
}

static int
print_usage(const char* name, bool error)
{
	FILE* const os = error ? stderr : stdout;
	fprintf(os, "Usage: %s [OPTION]... INPUT...\n", name);
	fprintf(os, "Validate RDF data\n\n");
	fprintf(os, "  -h  Display this help and exit\n");
	fprintf(os, "  -l  Print errors on a single line.\n");
	fprintf(os, "  -v  Display version information and exit\n");
	fprintf(os,
	        "Validate RDF data.  This is a simple validator which checks\n"
	        "that all used properties are actually defined.  It does not do\n"
	        "any fancy file retrieval, the files passed on the command line\n"
	        "are the only data that is read.  In other words, you must pass\n"
	        "the definition of all vocabularies used on the command line.\n");
	return error ? 1 : 0;
}

static char*
absolute_path(const char* path)
{
#ifdef _WIN32
	char* out = (char*)malloc(MAX_PATH);
	GetFullPathName(path, MAX_PATH, out, NULL);
	return out;
#else
	return realpath(path, NULL);
#endif
}

static int
missing_arg(const char* name, char opt)
{
	CERRORF("option requires an argument -- '%c'\n", opt);
	return print_usage(name, true);
}

int
main(int argc, char** argv)
{
	if (argc < 2) {
		return print_usage(argv[0], true);
	}

	int    a          = 1;
	size_t stack_size = 4194304;
	for (; a < argc && argv[a][0] == '-'; ++a) {
		if (argv[a][1] == 'h') {
			return print_usage(argv[0], false);
		} else if (argv[a][1] == 'k') {
			if (++a == argc) {
				return missing_arg(argv[0], 'k');
			}
			char*      endptr = NULL;
			const long size   = strtol(argv[a], &endptr, 10);
			if (size <= 0 || size == LONG_MAX || *endptr != '\0') {
				CERRORF("invalid stack size `%s'\n", argv[a]);
				return 1;
			}
			stack_size = (size_t)size;
		} else if (argv[a][1] == 'v') {
			return print_version();
		} else {
			CERRORF("invalid option -- '%s'\n", argv[a] + 1);
			return print_usage(argv[0], true);
		}
	}

	SerdWorld*           world    = serd_world_new();
	const SerdModelFlags indices  = SERD_INDEX_SPO | SERD_INDEX_OPS;
	const SerdModelFlags flags    = indices | SERD_STORE_CURSORS;
	SerdModel*           model    = serd_model_new(world, flags);
	SerdEnv*             env      = serd_env_new(NULL);
	SerdInserter*        inserter = serd_inserter_new(model, env, NULL);
	SerdReader*          reader   = serd_reader_new(
		world, SERD_TURTLE, serd_inserter_get_sink(inserter), stack_size);

	for (; a < argc; ++a) {
		const char* input   = argv[a];
		char*       in_path = absolute_path(input);

		if (!in_path) {
			CERRORF("unable to open file %s\n", input);
			continue;
		}

		SerdNode* base_uri_node = serd_new_file_uri(in_path, NULL);

		serd_env_set_base_uri(env, base_uri_node);
		SerdStatus st = serd_reader_start_file(reader, input, true);
		st = st ? st : serd_reader_read_document(reader);
		st = st ? st : serd_reader_finish(reader);

		if (st) {
			CERRORF("error reading %s: %s\n", in_path, serd_strerror(st));
			return 1;
		}

		serd_node_free(base_uri_node);
		free(in_path);
	}

	serd_reader_free(reader);
	serd_inserter_free(inserter);
	serd_env_free(env);

	const SerdStatus st = serd_validate(model);

	serd_model_free(model);
	serd_world_free(world);

	return (int)st;
}
