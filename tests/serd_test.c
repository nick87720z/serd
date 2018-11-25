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

#undef NDEBUG

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "serd/serd.h"

#ifndef INFINITY
#    define INFINITY (DBL_MAX + DBL_MAX)
#endif
#ifndef NAN
#    define NAN (INFINITY - INFINITY)
#endif

#define NS_XSD "http://www.w3.org/2001/XMLSchema#"

static void
test_strtod(double dbl, double max_delta)
{
	char buf[1024];
	snprintf(buf, sizeof(buf), "%f", dbl);

	size_t       end = 0;
	const double out = serd_strtod(buf, &end);

	const double diff = fabs(out - dbl);
	assert(diff <= max_delta);
	assert(end == strlen(buf));
}

static SerdStatus
count_prefixes(void* handle, const SerdNode* name, const SerdNode* uri)
{
	(void)name;
	(void)uri;

	++*(int*)handle;
	return SERD_SUCCESS;
}

static SerdStatus
count_statements(void*                handle,
                 SerdStatementFlags   flags,
                 const SerdStatement* statement)
{
	(void)flags;
	(void)statement;

	++*(size_t*)handle;
	return SERD_SUCCESS;
}

static void
test_file_uri(const char* hostname,
              const char* path,
              const char* expected_uri,
              const char* expected_path)
{
	if (!expected_path) {
		expected_path = path;
	}

	SerdNode*   node         = serd_new_file_uri(path, hostname);
	const char* node_str     = serd_node_get_string(node);
	char*       out_hostname = NULL;
	char*       out_path     = serd_file_uri_parse(node_str, &out_hostname);
	assert(!strcmp(node_str, expected_uri));
	assert((hostname && out_hostname) || (!hostname && !out_hostname));
	assert(!strcmp(out_path, expected_path));

	serd_free(out_path);
	serd_free(out_hostname);
	serd_node_free(node);
}

static int
check_rel_uri(const char*     uri,
              const SerdNode* base,
              const SerdNode* root,
              const char*     expected)
{
	SerdNode* rel = serd_new_relative_uri(uri, base, root);
	const int ret = strcmp(serd_node_get_string(rel), expected);
	serd_node_free(rel);
	assert(!ret);
	return ret;
}

static int
test_get_blank(void)
{
	SerdWorld* world = serd_world_new();
	char       expected[8];

	for (unsigned i = 0; i < 32; ++i) {
		const SerdNode* blank = serd_world_get_blank(world);

		snprintf(expected, sizeof(expected), "b%u", i + 1);
		assert(!strcmp(serd_node_get_string(blank), expected));
	}

	serd_world_free(world);
	return 0;
}

int
main(void)
{
#define MAX       1000000
#define NUM_TESTS 1000
	for (int i = 0; i < NUM_TESTS; ++i) {
		double dbl = rand() % MAX;
		dbl += (rand() % MAX) / (double)MAX;

		test_strtod(dbl, 1 / (double)MAX);
	}

	const double expt_test_nums[] = {
		2.0E18, -5e19, +8e20, 2e+24, -5e-5, 8e0, 9e-0, 2e+0
	};

	const char* expt_test_strs[] = {
		"02e18", "-5e019", "+8e20", "2E+24", "-5E-5", "8E0", "9e-0", " 2e+0"
	};

	for (unsigned i = 0; i < sizeof(expt_test_nums) / sizeof(double); ++i) {
		const double num   = serd_strtod(expt_test_strs[i], NULL);
		const double delta = fabs(num - expt_test_nums[i]);
		assert(delta <= DBL_EPSILON);
	}

	// Test serd_new_decimal

	const double dbl_test_nums[] = {
		0.0, 9.0, 10.0, .01, 2.05, -16.00001, 5.000000005, 0.0000000001, NAN, INFINITY
	};

	const char* dbl_test_strs[] = {
		"0.0", "9.0", "10.0", "0.01", "2.05", "-16.00001", "5.00000001", "0.0", NULL, NULL
	};

	for (unsigned i = 0; i < sizeof(dbl_test_nums) / sizeof(double); ++i) {
		SerdNode*   node     = serd_new_decimal(dbl_test_nums[i], 8, NULL);
		const char* node_str = serd_node_get_string(node);
		const bool  pass     = (node_str && dbl_test_strs[i])
		                          ? !strcmp(node_str, dbl_test_strs[i])
		                          : (node_str == dbl_test_strs[i]);
		assert(pass);
		const size_t len = node_str ? strlen(node_str) : 0;
		assert(serd_node_get_length(node) == len);
		assert(!dbl_test_strs[i] ||
		       !strcmp(serd_node_get_string(serd_node_get_datatype(node)),
		               NS_XSD "decimal"));
		serd_node_free(node);
	}

	// Test serd_new_integer

	const long int_test_nums[] = {
		0, -0, -23, 23, -12340, 1000, -1000
	};

	const char* int_test_strs[] = {
		"0", "0", "-23", "23", "-12340", "1000", "-1000"
	};

	for (unsigned i = 0; i < sizeof(int_test_nums) / sizeof(double); ++i) {
		SerdNode*   node     = serd_new_integer(int_test_nums[i], NULL);
		const char* node_str = serd_node_get_string(node);
		assert(!strcmp(node_str, int_test_strs[i]));
		const size_t len = strlen(node_str);
		assert(serd_node_get_length(node) == len);
		assert(!strcmp(serd_node_get_string(serd_node_get_datatype(node)),
		               NS_XSD "integer"));
		serd_node_free(node);
	}

	// Test serd_new_blob
	assert(!serd_new_blob(NULL, 0, true, NULL));
	assert(!serd_new_blob("data", 0, true, NULL));
	for (size_t size = 1; size < 256; ++size) {
		uint8_t* data = (uint8_t*)malloc(size);
		for (size_t i = 0; i < size; ++i) {
			data[i] = (uint8_t)(rand() % 256);
		}

		size_t       out_size = 0;
		SerdNode*    blob     = serd_new_blob(data, size, size % 5, NULL);
		const char*  blob_str = serd_node_get_string(blob);
		const size_t len      = serd_node_get_length(blob);

		uint8_t* out = (uint8_t*)malloc(serd_base64_decoded_size(len));
		assert(!serd_base64_decode(out, &out_size, blob_str, len));
		assert(serd_node_get_length(blob) == strlen(blob_str));
		assert(out_size == size);

		for (size_t i = 0; i < size; ++i) {
			assert(out[i] == data[i]);
		}

		assert(!strcmp(serd_node_get_string(serd_node_get_datatype(blob)),
		               NS_XSD "base64Binary"));

		serd_node_free(blob);
		free(out);
		free(data);
	}

	// Test serd_strlen

	const uint8_t str[] = { '"', '5', 0xE2, 0x82, 0xAC, '"', '\n', 0 };

	SerdNodeFlags flags;
	size_t        n_bytes = serd_strlen((const char*)str, &flags);
	assert(n_bytes == 7 && flags == (SERD_HAS_QUOTE|SERD_HAS_NEWLINE));
	assert(serd_strlen((const char*)str, NULL) == 7);

	// Test serd_strerror

	const char* msg = NULL;
	assert(!strcmp((msg = serd_strerror(SERD_SUCCESS)), "Success"));
	for (int i = SERD_FAILURE; i <= SERD_ERR_NO_DATA; ++i) {
		msg = serd_strerror((SerdStatus)i);
		assert(strcmp(msg, "Success"));
	}
	msg = serd_strerror((SerdStatus)-1);

	// Test file URI escaping and parsing

	test_file_uri(NULL, "C:/My 100%",
	              "file:///C:/My%20100%%", NULL);
	test_file_uri("ahost", "C:\\Pointless Space",
	              "file://ahost/C:/Pointless%20Space", "C:/Pointless Space");
	test_file_uri(NULL, "/foo/bar",
	              "file:///foo/bar", NULL);
	test_file_uri("bhost", "/foo/bar",
	              "file://bhost/foo/bar", NULL);
	test_file_uri(NULL, "a/relative <path>",
	              "a/relative%20%3Cpath%3E", NULL);

	// Test tolerance of parsing junk URI escapes

	char* out_path = serd_file_uri_parse("file:///foo/%0Xbar", NULL);
	assert(!strcmp(out_path, "/foo/bar"));
	serd_free(out_path);

	// Test serd_node_equals

	const uint8_t replacement_char_str[] = { 0xEF, 0xBF, 0xBD, 0 };
	SerdNode* lhs = serd_new_string((const char*)replacement_char_str);
	SerdNode* rhs = serd_new_string("123");
	assert(!serd_node_equals(lhs, rhs));

	SerdNode* qnode = serd_new_curie("foo:bar");
	assert(!serd_node_equals(lhs, qnode));
	serd_node_free(qnode);

	assert(!serd_node_copy(NULL));

	serd_node_free(lhs);
	serd_node_free(rhs);

	// Test serd_new_string

	SerdNode* hello = serd_new_string("hello\"");
	assert(serd_node_get_length(hello) == 6);
	assert(serd_node_get_flags(hello) == SERD_HAS_QUOTE);
	assert(!strncmp(serd_node_get_string(hello), "hello\"", 6));

	assert(!serd_new_string(NULL));

	// Test serd_new_literal

	assert(!serd_new_plain_literal(NULL, NULL));
	assert(!serd_new_typed_literal(NULL, NULL));

	assert(!serd_new_typed_literal("bad type", hello));

	SerdNode* hello2 = serd_new_string("hello\"");
	assert(serd_node_get_length(hello2) == 6 &&
	       serd_node_get_flags(hello2) == SERD_HAS_QUOTE &&
	       !strcmp(serd_node_get_string(hello2), "hello\""));

	SerdNode* hello3 = serd_new_plain_literal("hello\"", NULL);
	assert(serd_node_equals(hello2, hello3));

	SerdNode* hello4 = serd_new_typed_literal("hello\"", NULL);
	assert(serd_node_equals(hello4, hello2));

	serd_node_free(hello4);
	serd_node_free(hello3);
	serd_node_free(hello2);

	SerdNode* hello_l = serd_new_plain_literal("hello_l\"", "en");
	assert(serd_node_get_length(hello_l) == 8);
	assert(!strcmp(serd_node_get_string(hello_l), "hello_l\""));
	assert(serd_node_get_flags(hello_l) ==
	       (SERD_HAS_QUOTE | SERD_HAS_LANGUAGE));
	assert(!strcmp(serd_node_get_string(serd_node_get_language(hello_l)),
	               "en"));
	serd_node_free(hello_l);

	SerdNode* eg_Thing = serd_new_uri("http://example.org/Thing");

	SerdNode* hello_dt = serd_new_typed_literal("hello_dt\"", eg_Thing);
	assert(serd_node_get_length(hello_dt) == 9);
	assert(!strcmp(serd_node_get_string(hello_dt), "hello_dt\""));
	assert(serd_node_get_flags(hello_dt) ==
	       (SERD_HAS_QUOTE | SERD_HAS_DATATYPE));
	assert(!strcmp(serd_node_get_string(serd_node_get_datatype(hello_dt)),
	               "http://example.org/Thing"));
	serd_node_free(hello_dt);
	serd_node_free(eg_Thing);

	// Test absolute URI creation

	assert(!serd_new_uri(NULL));

	SerdNode* not_a_uri = serd_new_string("hello");
	SerdNode* root      = serd_new_uri("http://example.org/a/b/ignored");
	SerdNode* base      = serd_new_uri("http://example.org/a/b/c/");
	SerdNode* nil       = serd_new_resolved_uri(NULL, base);
	SerdNode* nil2      = serd_new_resolved_uri("", base);
	assert(!serd_new_resolved_uri("", NULL));
	assert(!serd_new_resolved_uri("", not_a_uri));
	assert(serd_node_get_type(nil) == SERD_URI);
	assert(!strcmp(serd_node_get_string(nil), serd_node_get_string(base)));
	assert(serd_node_get_type(nil2) == SERD_URI);
	assert(!strcmp(serd_node_get_string(nil2), serd_node_get_string(base)));

	// Test relative URI creation

	if (check_rel_uri("http://example.org/a/b/c/foo", base, NULL, "foo") ||
	    check_rel_uri("http://example.org/a/", base, NULL, "../../") ||
	    check_rel_uri("http://example.org/a/", base, root,
	                  "http://example.org/a/") ||
	    check_rel_uri("http://example.org/a/b/x", root, root,
	                  "x") ||
	    check_rel_uri("http://example.org/", base, NULL,
	                  "../../../") ||
	    check_rel_uri("http://drobilla.net/a", base, NULL,
	                  "http://drobilla.net/a")) {
		return 1;
	}

	// Test URI resolution

	assert(!serd_node_resolve(NULL, base));
	assert(!serd_node_resolve(nil, NULL));
	assert(!serd_node_resolve(not_a_uri, base));
	assert(!serd_node_resolve(nil, not_a_uri));

	SerdNode* rel = serd_new_relative_uri(
		"http://example.org/a/b/c/foo", base, NULL);
	SerdNode* resolved = serd_node_resolve(rel, base);
	assert(!strcmp(serd_node_get_string(resolved),
	               "http://example.org/a/b/c/foo"));

	serd_node_free(nil);
	serd_node_free(nil2);
	serd_node_free(not_a_uri);
	serd_node_free(resolved);
	serd_node_free(rel);
	serd_node_free(base);
	serd_node_free(root);

	// Test serd_new_blank

	assert(!serd_new_blank(NULL));

	SerdNode* blank = serd_new_blank("b0");
	assert(serd_node_get_length(blank) == 2);
	assert(serd_node_get_flags(blank) == 0);
	assert(!strcmp(serd_node_get_string(blank), "b0"));
	serd_node_free(blank);

	// Test SerdEnv

	SerdWorld* world = serd_world_new();

	if (test_get_blank()) {
		return 1;
	}

	SerdNode* eg    = serd_new_uri("http://example.org/");
	SerdNode* foo_u = serd_new_uri("http://example.org/foo");
	SerdNode* empty = serd_new_uri("");
	SerdNode* foo_c = serd_new_curie("eg.2:foo");
	SerdNode* b     = serd_new_curie("invalid");
	SerdNode* pre   = serd_new_curie("eg.2");
	SerdEnv*  env   = serd_env_new(NULL);
	serd_env_set_prefix(env, pre, eg);

	assert(!serd_env_get_base_uri(env));
	assert(serd_env_set_base_uri(env, NULL));
	assert(serd_env_set_base_uri(env, empty));
	assert(serd_env_set_base_uri(env, hello));
	assert(!serd_env_get_base_uri(env));

	SerdNode* xnode = serd_env_expand(env, hello);
	assert(!xnode);

	serd_node_free(hello);

	SerdNode* xu = serd_env_expand(env, foo_c);
	assert(!strcmp(serd_node_get_string(xu), "http://example.org/foo"));
	serd_node_free(xu);

	SerdNode* badpre = serd_new_curie("hm:what");
	SerdNode* xbadpre = serd_env_expand(env, badpre);
	assert(!xbadpre);
	serd_node_free(badpre);

	SerdNode* xc = serd_env_expand(env, foo_c);
	assert(serd_node_equals(xc, foo_u));
	serd_node_free(xc);

	assert(serd_env_set_prefix(env, NULL, NULL));

	SerdNode* lit = serd_new_string("hello");
	assert(serd_env_set_prefix(env, b, lit));

	size_t    n_prefixes          = 0;
	SerdSink* count_prefixes_sink = serd_sink_new(&n_prefixes, NULL);
	serd_sink_set_prefix_func(count_prefixes_sink, count_prefixes);
	serd_env_set_prefix(env, pre, eg);
	serd_env_write_prefixes(env, count_prefixes_sink);
	serd_sink_free(count_prefixes_sink);
	assert(n_prefixes == 1);

	SerdNode* shorter_uri = serd_new_uri("urn:foo");
	assert(!serd_env_qualify(env, shorter_uri));
	serd_node_free(shorter_uri);

	SerdNode* qualified = serd_env_qualify(env, foo_u);
	assert(serd_node_equals(qualified, foo_c));

	SerdEnv* env_copy = serd_env_copy(env);
	assert(serd_env_equals(env, env_copy));

	SerdNode* qualified2 = serd_env_expand(env_copy, foo_u);
	assert(serd_node_equals(qualified, foo_c));
	serd_node_free(qualified2);

	serd_env_set_prefix_from_strings(
	        env_copy, "test", "http://example.org/test");
	assert(!serd_env_equals(env, env_copy));

	serd_env_set_prefix_from_strings(env, "test2", "http://example.org/test");
	assert(!serd_env_equals(env, env_copy));

	serd_node_free(qualified);
	serd_node_free(foo_c);
	serd_node_free(empty);
	serd_node_free(foo_u);
	serd_node_free(b);
	serd_node_free(pre);
	serd_node_free(eg);
	serd_env_free(env_copy);

	// Test SerdReader and SerdWriter

	const char* path = "serd_test.ttl";
	FILE* fd = fopen(path, "wb");
	assert(fd);

	SerdWriter* writer = serd_writer_new(world,
	                                     SERD_TURTLE,
	                                     0,
	                                     env,
	                                     (SerdWriteFunc)fwrite,
	                                     fd);
	assert(writer);

	serd_writer_chop_blank_prefix(writer, "tmp");
	serd_writer_chop_blank_prefix(writer, NULL);

	const SerdSink* iface = serd_writer_get_sink(writer);
	assert(serd_sink_write_base(iface, lit));
	assert(serd_sink_write_prefix(iface, lit, lit));
	assert(serd_sink_write_end(iface, NULL));
	assert(serd_writer_get_env(writer) == env);

	uint8_t buf[] = { 0xEF, 0xBF, 0xBD, 0 };
	SerdNode* s = serd_new_uri("");
	SerdNode* p = serd_new_uri("http://example.org/pred");
	SerdNode* o = serd_new_string((char*)buf);

	// Write 3 invalid statements (should write nothing)
	const SerdNode* junk[][5] = { { s,    p,    NULL },
	                              { s,    NULL, o    },
	                              { NULL, p,    o    },
	                              { s,    p,    NULL },
	                              { s,    NULL, o    },
	                              { NULL, p,    o    },
	                              { s,    o,    o    },
	                              { o,    p,    o    },
	                              { s,    p,    NULL },
	                              { NULL, NULL, NULL } };
	for (unsigned i = 0; i < sizeof(junk) / (sizeof(SerdNode*) * 5); ++i) {
		assert(serd_sink_write(
		        iface, 0, junk[i][0], junk[i][1], junk[i][2], 0));
	}

	SerdNode* urn_Type = serd_new_uri("urn:Type");

	SerdNode* t = serd_new_typed_literal((char*)buf, urn_Type);
	SerdNode* l = serd_new_plain_literal((char*)buf, "en");
	const SerdNode* good[][5] = { { s, p, o },
	                              { s, p, o },
	                              { s, p, t },
	                              { s, p, l },
	                              { s, p, l },
	                              { s, p, t },
	                              { s, p, l },
	                              { s, p, o },
	                              { s, p, o },
	                              { s, p, o } };
	for (unsigned i = 0; i < sizeof(good) / (sizeof(SerdNode*) * 5); ++i) {
		assert(!serd_sink_write(
		        iface, 0, good[i][0], good[i][1], good[i][2], 0));
	}

	// Write statements with bad UTF-8 (should be replaced)
	const char bad_str[] = { (char)0xFF, (char)0x90, 'h', 'i', 0 };
	SerdNode*  bad_lit   = serd_new_string(bad_str);
	SerdNode*  bad_uri   = serd_new_uri(bad_str);
	assert(!serd_sink_write(iface, 0, s, p, bad_lit, 0));
	assert(!serd_sink_write(iface, 0, s, p, bad_uri, 0));
	serd_node_free(bad_uri);
	serd_node_free(bad_lit);

	// Write 1 valid statement
	serd_node_free(o);
	o = serd_new_string("hello");
	assert(!serd_sink_write(iface, 0, s, p, o, 0));

	serd_writer_free(writer);
	serd_node_free(lit);
	serd_node_free(s);
	serd_node_free(p);
	serd_node_free(o);
	serd_node_free(t);
	serd_node_free(l);
	serd_node_free(urn_Type);

	// Test buffer sink
	SerdBuffer    buffer = { NULL, 0 };
	SerdByteSink* byte_sink =
		serd_byte_sink_new((SerdWriteFunc)serd_buffer_sink, &buffer, 1);

	writer = serd_writer_new(world,
	                         SERD_TURTLE,
	                         0,
	                         env,
	                         (SerdWriteFunc)serd_byte_sink_write,
	                         byte_sink);

	o = serd_new_uri("http://example.org/base");
	assert(!serd_writer_set_base_uri(writer, o));

	serd_node_free(o);
	serd_writer_free(writer);
	serd_byte_sink_free(byte_sink);
	char* out = serd_buffer_sink_finish(&buffer);

	assert(!strcmp(out, "@base <http://example.org/base> .\n"));
	serd_free(out);

	// Rewind and test reader
	fseek(fd, 0, SEEK_SET);

	size_t    n_statements = 0;
	SerdSink* sink         = serd_sink_new(&n_statements, NULL);
	serd_sink_set_statement_func(sink, count_statements);

	SerdReader* reader = serd_reader_new(world, SERD_TURTLE, sink, 4096);
	assert(reader);

	serd_reader_add_blank_prefix(reader, "tmp");
	serd_reader_add_blank_prefix(reader, NULL);

	assert(serd_reader_start_file(reader, "http://notafile", false));
	assert(serd_reader_start_file(reader, "file://invalid", false));
	assert(serd_reader_start_file(reader, "file:///nonexistant", false));

	assert(!serd_reader_start_file(reader, path, true));
	assert(!serd_reader_read_document(reader));
	assert(n_statements == 13);
	serd_reader_finish(reader);

	serd_reader_free(reader);
	serd_sink_free(sink);
	fclose(fd);

	serd_env_free(env);
	serd_world_free(world);

	printf("Success\n");
	return 0;
}
