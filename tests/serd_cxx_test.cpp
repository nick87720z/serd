/*
  Copyright 2018-2019 David Robillard <http://drobilla.net>

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

#include "serd/serd.h"
#include "serd/serd.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

template <typename T>
static int
test_move_only(T&& obj)
{
	static_assert(!std::is_copy_constructible<T>::value, "");
	static_assert(!std::is_copy_assignable<T>::value, "");

	const auto* const ptr = obj.cobj();

	// Move construct
	T moved{std::forward<T>(obj)};
	assert(moved.cobj() == ptr);
	assert(!obj.cobj()); // NOLINT(bugprone-use-after-move)

	// Move assign
	obj = std::move(moved);
	assert(obj.cobj() == ptr);
	assert(!moved.cobj()); // NOLINT(bugprone-use-after-move)

	return 0;
}

template <typename T>
static int
test_copy_move(const T& obj)
{
	T copy{obj};
	assert(copy == obj);

	T moved{std::move(copy)};
	assert(moved == obj);
	// assert(copy != obj); // NOLINT(bugprone-use-after-move)

	T copy_assigned{obj};
	copy_assigned = obj;
	assert(copy_assigned == obj);

	T move_assigned{obj};
	move_assigned = std::move(copy_assigned);
	assert(move_assigned == obj);
	// assert(copy_assigned != obj); // NOLINT(bugprone-use-after-move)

	return 0;
}

static int
test_operators()
{
	int st = 0;

	serd::World world;

	serd::Model model(world, serd::ModelFlag::index_SPO);
	model.insert(serd::make_uri("http://example.org/s"),
	             serd::make_uri("http://example.org/p"),
	             serd::make_uri("http://example.org/o"));

	serd::Sink sink;
	serd::Env  env;

	std::ostringstream stream;

	// st |= test_move_only(serd::World{});
	st |= test_copy_move(serd::Statement{*model.begin()});
	st |= test_copy_move(
	        serd::Cursor{serd::make_uri("http://example.org/doc"), 1, 2});
	st |= test_copy_move(model.begin()->cursor());
	st |= test_copy_move(serd::Env{});
	st |= test_move_only(
	        serd::Reader{world, serd::Syntax::Turtle, {}, sink, 4096});
	st |= test_copy_move(model.begin());
	st |= test_copy_move(model.all());
	// Sink
	st |= test_copy_move(model);
	// st |= test_move_only(serd::Inserter{model, env});
	// st |= test_move_only(serd::Sink{});

	st |= test_copy_move(serd::Env{});

	return st;
}

template <typename Value>
static int
test_optional(const Value& value, const Value& other)
{
	test_copy_move(value);

	// Truthiness
	assert(!serd::Optional<Value>());
	// assert(!serd::Optional<Value>(nullptr));
	assert(serd::Optional<Value>(value));

	// Comparison and general sanity
	serd::Optional<Value> optional{value};
	assert(optional);
	assert(optional == value);
	assert(optional != other);
	assert(*optional == value);
	assert(optional.cobj() != value.cobj()); // non-const, must be a copy

	// Reset
	optional.reset();
	assert(!optional);
	assert(!optional.cobj());

	// Copying and moving
	Value       nonconst = value;
	const auto* c_ptr    = nonconst.cobj();

	optional = nonconst;
	serd::Optional<Value> copied{optional};
	assert(copied == nonconst);
	assert(copied.cobj() != c_ptr);

	optional = std::move(nonconst);
	serd::Optional<Value> moved{std::move(optional)};
	assert(moved.cobj() == c_ptr);
	assert(!optional); // NOLINT(bugprone-use-after-move)

	serd::Optional<Value> copy_assigned;
	copy_assigned = optional;
	assert(copy_assigned == optional);
	assert(copy_assigned.cobj() != c_ptr);

	serd::Optional<Value> move_assigned;
	move_assigned = std::move(moved);
	assert(move_assigned.cobj() == c_ptr);
	assert(!optional);

	serd::Optional<Value> nullopt_assigned;
	nullopt_assigned = {};
	assert(!nullopt_assigned.cobj());

	return 0;
}

static int
test_optional()
{
	test_optional(serd::make_string("value"), serd::make_string("other"));

	{
		serd::World world;

		serd::Model value(world, serd::ModelFlag::index_SPO);
		value.insert(serd::make_uri("http://example.org/s1"),
		             serd::make_uri("http://example.org/p1"),
		             serd::make_uri("http://example.org/o1"));

		serd::Model other(world, serd::ModelFlag::index_SPO);
		value.insert(serd::make_uri("http://example.org/s2"),
		             serd::make_uri("http://example.org/p2"),
		             serd::make_uri("http://example.org/o2"));

		test_optional(value, other);
	}

	return 0;
}

static int
test_node(const serd::Node& node)
{
	test_copy_move(node);

	if (node.datatype()) {
		return test_node(*node.datatype());
	} else if (node.language()) {
		return test_node(*node.language());
	}
	return 0;
}

static int
test_nodes()
{
	const auto type = serd::make_uri("http://example.org/Type");
	const auto base = serd::make_uri("http://example.org/");
	const auto root = serd::make_uri("http://example.org/");

	assert(base.type() == serd::NodeType::URI);
	assert(base.size() == strlen("http://example.org/"));
	assert(base == root);
	assert(base < type);
	assert(!base.empty());
	assert(std::count(base.begin(), base.end(), '/') == 3);

	const auto relative = serd::make_uri("rel/uri");
	const auto resolved = relative.resolve(base);
	assert((std::string)resolved == "http://example.org/rel/uri");
	assert((serd::StringView)resolved == "http://example.org/rel/uri");

	const auto string = serd::make_string("hello\n\"world\"");
	assert(string.flags() ==
	       (serd::NodeFlag::has_newline | serd::NodeFlag::has_quote));

	const auto number = serd::make_integer(42);
	assert(number.flags() == serd::NodeFlag::has_datatype);
	assert(number.datatype() ==
	       serd::make_uri("http://www.w3.org/2001/XMLSchema#integer"));

	const auto tagged = serd::make_plain_literal("hallo", "de");
	assert(tagged.flags() == serd::NodeFlag::has_language);
	assert(tagged.language() == serd::make_string("de"));

	assert(!test_node(serd::make_string("hello")));
	assert(!test_node(serd::make_plain_literal("hello", "en")));
	assert(!test_node(serd::make_typed_literal("hello", type)));
	assert(!test_node(serd::make_blank("blank")));
	assert(!test_node(serd::make_curie("eg:curie")));
	assert(!test_node(serd::make_uri("http://example.org/thing")));
	assert(!test_node(serd::make_resolved_uri("thing", base)));
	assert(!test_node(serd::make_file_uri("/foo/bar", "host")));
	assert(!test_node(serd::make_file_uri("/foo/bar")));
	assert(!test_node(serd::make_file_uri("/foo/bar", "host")));
	assert(!test_node(serd::make_file_uri("/foo/bar")));
	assert(!test_node(serd::make_relative_uri("http://example.org/a", base)));
	assert(!test_node(
	        serd::make_relative_uri("http://example.org/a", base, root)));
	assert(!test_node(serd::make_decimal(1.2, 7)));
	assert(!test_node(serd::make_decimal(3.4, 7, type)));
	assert(!test_node(serd::make_integer(56)));
	assert(!test_node(serd::make_integer(78, type)));
	assert(!test_node(serd::make_blob("blob", 4, true)));
	assert(!test_node(serd::make_blob("blob", 4, true, type)));

	return 0;
}

static int
test_uri()
{
	const auto no_authority = serd::URI("file:/path");
	assert(no_authority.scheme() == "file");
	assert(!no_authority.authority().data());
	assert(no_authority.path() == "/path");

	const auto empty_authority = serd::URI("file:///path");
	assert(empty_authority.scheme() == "file");
	assert(empty_authority.authority().data());
	assert(empty_authority.authority().empty());
	assert(empty_authority.path() == "/path");

	const auto base = serd::URI("http://example.org/base/");
	assert(base.scheme() == "http");
	assert(base.authority() == "example.org");
	assert(!base.path_base().data());
	assert(base.path() == "/base/");
	assert(!base.query().data());
	assert(!base.fragment().data());

	const auto rel = serd::URI("relative/path?query#fragment");
	assert(!rel.scheme().data());
	assert(!rel.authority().data());
	assert(!rel.path_base().data());
	assert(rel.path() == "relative/path");
	assert(rel.query() == "query");
	assert(rel.fragment() == "#fragment");

	const auto resolved = rel.resolve(base);
	assert(resolved.scheme() == "http");
	assert(resolved.authority() == "example.org");
	assert(resolved.path_base() == "/base/");
	assert(resolved.path() == "relative/path");
	assert(resolved.query() == "query");
	assert(resolved.fragment() == "#fragment");

	std::ostringstream ss;
	ss << resolved;
	assert(ss.str() == "http://example.org/base/relative/path?query#fragment");

	return 0;
}

static int
test_reader()
{
	size_t            n_statements{};
	std::stringstream stream{};
	serd::Sink        sink;

	sink.set_statement_func(
	        [&](const serd::StatementFlags, const serd::Statement& statement) {
		        ++n_statements;
		        stream << statement.subject() << " " << statement.predicate()
		               << " " << statement.object() << std::endl;
		        return serd::Status::success;
	        });

	serd::World  world;
	serd::Reader reader(
		world, serd::Syntax::Turtle, serd::ReaderFlag::strict, sink, 4096);

	// Read from string
	reader.start_string("@prefix eg: <http://example.org> ."
	                    "eg:s eg:p eg:o1 , eg:o2 .");
	reader.read_document();

	assert(n_statements == 2);
	assert(stream.str() == "eg:s eg:p eg:o1\neg:s eg:p eg:o2\n");

	// Read from stream
	std::stringstream ss("eg:s eg:p eg:o3 , eg:o4 .");
	reader.start_stream(ss);
	reader.read_document();

	assert(n_statements == 4);
	assert(stream.str() == "eg:s eg:p eg:o1\n"
	                       "eg:s eg:p eg:o2\n"
	                       "eg:s eg:p eg:o3\n"
	                       "eg:s eg:p eg:o4\n");

	return 0;
}

static serd::Status
write_test_doc(serd::Writer& writer)
{
	const auto& sink = writer.sink();

	sink.base(serd::make_uri("http://drobilla.net/base/"));
	sink.prefix(serd::make_string("eg"), serd::make_uri("http://example.org/"));
	sink.write({},
	           serd::make_uri("http://drobilla.net/base/s"),
	           serd::make_uri("http://example.org/p"),
	           serd::make_uri("http://drobilla.net/o"));

	return writer.finish();
}

static const char* writer_test_doc = "@base <http://drobilla.net/base/> .\n"
                                     "@prefix eg: <http://example.org/> .\n"
                                     "\n"
                                     "<s>\n"
                                     "\teg:p <../o> .\n";

static int
test_writer_ostream()
{
	serd::World        world;
	serd::Env          env;
	std::ostringstream stream;
	serd::Writer       writer(world, serd::Syntax::Turtle, {}, env, stream);

	write_test_doc(writer);
	assert(stream.str() == writer_test_doc);

	{
		std::ofstream bad_file("/does/not/exist");
		bad_file.clear();
		bad_file.exceptions(std::ifstream::badbit);
		writer = serd::Writer(world, serd::Syntax::Turtle, {}, env, bad_file);
		const serd::Status st =
		        writer.sink().base(serd::make_uri("http://drobilla.net/base/"));
		assert(st == serd::Status::err_bad_write);
	}

	return 0;
}

static int
test_writer_string_sink()
{
	serd::World  world;
	serd::Env    env;
	std::string  output;
	serd::Writer writer(world,
	                    serd::Syntax::Turtle,
	                    {},
	                    env,
	                    [&output](const char* str, size_t len) {
		                    output += str;
		                    return len;
	                    });

	write_test_doc(writer);
	assert(output == writer_test_doc);

	return 0;
}

static int
test_env()
{
	serd::Env env;

	const auto base = serd::make_uri("http://drobilla.net/");
	env.set_base_uri(base);
	assert(env.base_uri() == base);

	env.set_prefix(serd::make_string("eg"),
	               serd::make_uri("http://example.org/"));

	assert(env.qualify(serd::make_uri("http://example.org/foo")) ==
	       serd::make_curie("eg:foo"));

	assert(env.expand(serd::make_uri("foo")) ==
	       serd::make_uri("http://drobilla.net/foo"));

	serd::Env copied{env};
	assert(copied.qualify(serd::make_uri("http://example.org/foo")) ==
	       serd::make_curie("eg:foo"));

	assert(copied.expand(serd::make_uri("foo")) ==
	       serd::make_uri("http://drobilla.net/foo"));

	serd::Env assigned;
	assigned   = env;
	auto curie = env.qualify(serd::make_uri("http://example.org/foo"));

	assert(assigned.qualify(serd::make_uri("http://example.org/foo")) ==
	       serd::make_curie("eg:foo"));

	assert(assigned.expand(serd::make_uri("foo")) ==
	       serd::make_uri("http://drobilla.net/foo"));

	return 0;
}

static int
test_model()
{
	serd::World world;
	serd::Model model(world,
	                  serd::ModelFlag::index_SPO | serd::ModelFlag::index_OPS);

	assert(model.empty());

	const auto s  = serd::make_uri("http://example.org/s");
	const auto p  = serd::make_uri("http://example.org/p");
	const auto o1 = serd::make_uri("http://example.org/o1");
	const auto o2 = serd::make_uri("http://example.org/o2");

	serd::NodeView b = world.get_blank();
	auto           r = b.resolve(s);

	model.insert(s, p, o1);
	model.insert(s, p, o2);

	assert(!model.empty());
	assert(model.size() == 2);
	assert(model.ask(s, p, o1));
	assert(model.ask(s, p, o1));
	assert(!model.ask(s, p, s));

	size_t total_count = 0;
	for (const auto& statement : model) {
		assert(statement.subject() == s);
		assert(statement.predicate() == p);
		assert(statement.object() == o1 || statement.object() == o2);
		++total_count;
	}
	assert(total_count == 2);

	size_t o1_count = 0;
	for (const auto& statement : model.range({}, {}, o1)) {
		assert(statement.subject() == s);
		assert(statement.predicate() == p);
		assert(statement.object() == o1);
		++o1_count;
	}
	assert(o1_count == 1);

	size_t o2_count = 0;
	for (const auto& statement : model.range({}, {}, o2)) {
		assert(statement.subject() == s);
		assert(statement.predicate() == p);
		assert(statement.object() == o2);
		++o2_count;
	}
	assert(o2_count == 1);

	serd::Model copy(model);
	assert(copy == model);

	copy.insert(s, p, s);
	assert(copy != model);

	return 0;
}

static int
test_log()
{
	serd::World world;
	bool        called = false;
	world.set_message_func([&called](const serd::StringView domain,
	                                 const serd::LogLevel   level,
	                                 const serd::LogFields& fields,
	                                 const std::string&     msg) {
		assert(fields.at("TEST_EXTRA") == "extra field");
		assert(level == serd::LogLevel::err);
		assert(msg == "bad argument to something: 42\n");
		called = true;
		return serd::Status::success;
	});

	world.log("test",
	          serd::LogLevel::err,
	          {{"TEST_EXTRA", "extra field"}},
	          "bad argument to %s: %d\n",
	          "something",
	          42);

	assert(called);

	return 0;
}

int
main()
{
	using TestFunc = int (*)();

	constexpr std::array<TestFunc, 10> tests{{test_operators,
	                                          test_optional,
	                                          test_nodes,
	                                          test_uri,
	                                          test_env,
	                                          test_reader,
	                                          test_writer_ostream,
	                                          test_writer_string_sink,
	                                          test_model,
	                                          test_log}};

	int failed = 0;
	for (const auto& test : tests) {
		failed += test();
	}

	if (failed == 0) {
		std::cerr << "All " << tests.size() << " tests passed" << std::endl;
	} else {
		std::cerr << failed << "/" << tests.size() << " tests failed"
		          << std::endl;
	}

	return failed;
}
