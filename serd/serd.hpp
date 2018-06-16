/*
  Copyright 2018 David Robillard <http://drobilla.net>

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

/// @file serd.hpp C++ API for Serd, a lightweight RDF syntax library

#ifndef SERD_SERD_HPP
#define SERD_SERD_HPP

#include "serd/detail/Copyable.hpp"
#include "serd/detail/Flags.hpp"
#include "serd/detail/Optional.hpp"
#include "serd/detail/StringView.hpp"
#include "serd/detail/Wrapper.hpp"
#include "serd/serd.h"

#include <cassert>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

/**
   @defgroup serdxx Serdxx
   C++ bindings for Serd, a lightweight RDF syntax library.
   @{
*/

namespace serd {

/**
   Sink function for string output

   Similar semantics to `SerdWriteFunc` (and in turn `fwrite`), but takes char*
   for convenience and may set errno for more informative error reporting than
   supported by `SerdStreamErrorFunc`.

   @return Number of elements (bytes) written, which is short on error.
*/
using WriteFunc = std::function<size_t(const char*, size_t)>;


namespace detail {

class TextSink
{
public:
	explicit TextSink(WriteFunc write_func) : _write_func(std::move(write_func))
	{
	}

	explicit TextSink(std::ostream& stream)
	    : _write_func([&](const char* str, size_t len) {
		    stream.write(str, std::streamsize(len));
		    return stream.good() ? len : size_t(0);
	    })
	{
	}

	static inline size_t
	s_write(const void* buf, size_t size, size_t nmemb, void* sink) noexcept
	{
		assert(size == 1);
		auto* self = static_cast<TextSink*>(sink);

		try {
			return self->_write_func(static_cast<const char*>(buf), nmemb);
		} catch (...) {
		}
		return 0;
	}

private:
	WriteFunc _write_func;
};

} // namespace detail

template <typename T>
using Optional = detail::Optional<T>;

using StringView = detail::StringView;

template <typename Flag>
inline constexpr typename std::enable_if<std::is_enum<Flag>::value,
                                         detail::Flags<Flag>>::type
operator|(const Flag lhs, const Flag rhs) noexcept
{
	return detail::Flags<Flag>{lhs} | rhs;
}

/// Return status code
enum class Status {
	success        = SERD_SUCCESS,        ///< No error
	failure        = SERD_FAILURE,        ///< Non-fatal failure
	err_unknown    = SERD_ERR_UNKNOWN,    ///< Unknown error
	err_bad_syntax = SERD_ERR_BAD_SYNTAX, ///< Invalid syntax
	err_bad_arg    = SERD_ERR_BAD_ARG,    ///< Invalid argument
	err_bad_iter   = SERD_ERR_BAD_ITER,   ///< Use of invalidated iterator
	err_not_found  = SERD_ERR_NOT_FOUND,  ///< Not found
	err_id_clash   = SERD_ERR_ID_CLASH,   ///< Clashing blank node IDs
	err_bad_curie  = SERD_ERR_BAD_CURIE,  ///< Invalid CURIE
	err_internal   = SERD_ERR_INTERNAL,   ///< Unexpected internal error
	err_overflow   = SERD_ERR_OVERFLOW,   ///< Stack overflow
	err_invalid    = SERD_ERR_INVALID,    ///< Invalid data
	err_no_data    = SERD_ERR_NO_DATA,    ///< Unexpected end of input
	err_bad_write  = SERD_ERR_BAD_WRITE   ///< Error writing to file/stream
};

/// RDF syntax type
enum class Syntax {
	Turtle   = SERD_TURTLE,   ///< Terse RDF Triple Language
	NTriples = SERD_NTRIPLES, ///< Line-based RDF triples
	NQuads   = SERD_NQUADS,   ///< Line-based RDF quads (NTriples with graphs)
	TriG     = SERD_TRIG      ///< Terse RDF quads (Turtle with graphs)
};

/// Flags indicating inline abbreviation information for a statement
enum class StatementFlag {
	empty_S = SERD_EMPTY_S, ///< Empty blank node subject
	anon_S  = SERD_ANON_S,  ///< Start of anonymous subject
	anon_O  = SERD_ANON_O,  ///< Start of anonymous object
	list_S  = SERD_LIST_S,  ///< Start of list subject
	list_O  = SERD_LIST_O,  ///< Start of list object
	terse_S = SERD_TERSE_S, ///< Terse serialisation of new subject
	terse_O = SERD_TERSE_O  ///< Terse serialisation of new object
};

using StatementFlags = detail::Flags<StatementFlag>;

/// Flags that control style for a model serialisation
enum class SerialisationFlag {
	no_inline_objects = SERD_NO_INLINE_OBJECTS, ///< Disable object inlining
};

using SerialisationFlags = detail::Flags<SerialisationFlag>;

/**
   Type of a syntactic RDF node

   This is more precise than the type of an abstract RDF node.  An abstract
   node is either a resource, literal, or blank.  In syntax there are two ways
   to refer to a resource (by URI or CURIE) and two ways to refer to a blank
   (by ID or anonymously).  Anonymous (inline) blank nodes are expressed using
   SerdStatementFlags rather than this type.
*/
enum class NodeType {
	/**
	   The type of a nonexistent node

	   This type is useful as a sentinel, but is never emitted by the reader.
	*/
	nothing = SERD_NOTHING,

	/**
	   Literal value

	   A literal optionally has either a language, or a datatype (not both).
	*/
	literal = SERD_LITERAL,

	/**
	   URI (absolute or relative)

	   Value is an unquoted URI string, which is either a relative reference
	   with respect to the current base URI (e.g. "foo/bar"), or an absolute
	   URI (e.g. "http://example.org/foo").
	   @see <a href="http://tools.ietf.org/html/rfc3986">RFC3986</a>.
	*/
	URI = SERD_URI,

	/**
	   CURIE, a shortened URI

	   Value is an unquoted CURIE string relative to the current environment,
	   e.g. "rdf:type".
	   @see <a href="http://www.w3.org/TR/curie">CURIE Syntax 1.0</a>
	*/
	CURIE = SERD_CURIE,

	/**
	   A blank node

	   Value is a blank node ID, e.g. "id3", which is meaningful only within
	   this serialisation.
	   @see <a href="http://www.w3.org/TeamSubmission/turtle#nodeID">Turtle
	   <tt>nodeID</tt></a>
	*/
	blank = SERD_BLANK
};

/// Flags indicating certain string properties relevant to serialisation
enum class NodeFlag {
	has_newline  = SERD_HAS_NEWLINE,  ///< Contains line breaks
	has_quote    = SERD_HAS_QUOTE,    ///< Contains quotes
	has_datatype = SERD_HAS_DATATYPE, ///< Literal node has datatype
	has_language = SERD_HAS_LANGUAGE  ///< Literal node has language
};

using NodeFlags = detail::Flags<NodeFlag>;

/// Field in a statement
enum class Field {
	subject   = SERD_SUBJECT,   ///< Subject
	predicate = SERD_PREDICATE, ///< Predicate ("key")
	object    = SERD_OBJECT,    ///< Object    ("value")
	graph     = SERD_GRAPH      ///< Graph     ("context")
};

/// Indexing option
enum class ModelFlag {
	index_SPO     = SERD_INDEX_SPO,    ///< Subject,   Predicate, Object
	index_SOP     = SERD_INDEX_SOP,    ///< Subject,   Object,    Predicate
	index_OPS     = SERD_INDEX_OPS,    ///< Object,    Predicate, Subject
	index_OSP     = SERD_INDEX_OSP,    ///< Object,    Subject,   Predicate
	index_PSO     = SERD_INDEX_PSO,    ///< Predicate, Subject,   Object
	index_POS     = SERD_INDEX_POS,    ///< Predicate, Object,    Subject
	index_graphs  = SERD_INDEX_GRAPHS, ///< Support multiple graphs in model
	store_cursors = SERD_STORE_CURSORS ///< Store original cursor of statements
};

using ModelFlags = detail::Flags<ModelFlag>;

/// Log message level
enum class LogLevel {
	emerg   = SERD_LOG_LEVEL_EMERG,   ///< Emergency, system is unusable
	alert   = SERD_LOG_LEVEL_ALERT,   ///< Action must be taken immediately
	crit    = SERD_LOG_LEVEL_CRIT,    ///< Critical condition
	err     = SERD_LOG_LEVEL_ERR,     ///< Error
	warning = SERD_LOG_LEVEL_WARNING, ///< Warning
	notice  = SERD_LOG_LEVEL_NOTICE,  ///< Normal but significant condition
	info    = SERD_LOG_LEVEL_INFO,    ///< Informational message
	debug   = SERD_LOG_LEVEL_DEBUG    ///< Debug message
};

/// Reader flags
enum class ReaderFlag {
	strict = SERD_READ_STRICT  ///< Do not tolerate invalid input
};

using ReaderFlags = detail::Flags<ReaderFlag>;

/**
   Writer flags

   The style of the writer output can be controlled by ORing together
   values from this enumeration.  Note that some options are only supported
   for some syntaxes (e.g. NTriples does not support abbreviation and is
   always ASCII).
*/
enum class WriterFlag {
	ascii = SERD_WRITE_ASCII, ///< Escape all non-ASCII characters
	terse = SERD_WRITE_TERSE  ///< Write terser output without newlines
};

using WriterFlags = detail::Flags<WriterFlag>;

/**
   @name String Utilities
   @{
*/

inline const char*
strerror(const Status status)
{
	return serd_strerror(static_cast<SerdStatus>(status));
}

/**
   @}
   @name Node
   @{
*/

template <typename CObj>
using NodeHandle = detail::
        Copyable<CObj, serd_node_copy, serd_node_equals, serd_node_free>;

template <typename CObj>
class NodeBase;

using Node     = NodeBase<SerdNode>;
using NodeView = NodeBase<const SerdNode>;

template <typename CObj>
class NodeBase : public NodeHandle<CObj>
{
public:
	using Base = NodeHandle<CObj>;
	using Base::cobj;

	explicit NodeBase(CObj* ptr) : Base(ptr) {}

	template <typename C>
	// NOLINTNEXTLINE(hicpp-explicit-conversions)
	NodeBase(const NodeBase<C>& node) : Base{node}
	{
	}

	NodeType    type() const { return NodeType(serd_node_get_type(cobj())); }
	const char* c_str() const { return serd_node_get_string(cobj()); }
	StringView  str() const { return StringView{c_str(), length()}; }
	size_t      size() const { return serd_node_get_length(cobj()); }
	size_t      length() const { return serd_node_get_length(cobj()); }

	NodeFlags flags() const { return NodeFlags(serd_node_get_flags(cobj())); }

	Optional<NodeView> datatype() const;
	Optional<NodeView> language() const;

	Node resolve(NodeView base) const
	{
		return Node(serd_node_resolve(cobj(), base.cobj()));
	}

	bool operator<(NodeView node) const
	{
		return serd_node_compare(cobj(), node.cobj()) < 0;
	}

	explicit operator std::string() const
	{
		return std::string(c_str(), length());
	}

	explicit operator StringView() const
	{
		return StringView(c_str(), length());
	}

	const char* begin() const { return c_str(); }
	const char* end() const { return c_str() + length(); }
	bool        empty() const { return length() == 0; }
};

template <typename CObj>
Optional<NodeView>
NodeBase<CObj>::datatype() const
{
	return NodeView(serd_node_get_datatype(cobj()));
}

template <typename CObj>
Optional<NodeView>
NodeBase<CObj>::language() const
{
	return NodeView(serd_node_get_language(cobj()));
}

inline std::ostream&
operator<<(std::ostream& os, const NodeView& node)
{
	return os << node.c_str();
}

inline Node
make_string(StringView str)
{
	return Node(serd_new_string(str.data()));
}

inline Node
make_plain_literal(StringView str, StringView lang)
{
	return Node(serd_new_plain_literal(str.data(), lang.data()));
}

inline Node
make_typed_literal(StringView str, const NodeView& datatype)
{
	return Node(serd_new_typed_literal(str.data(), datatype.cobj()));
}

inline Node
make_blank(StringView str)
{
	return Node(serd_new_blank(str.data()));
}

inline Node
make_curie(StringView str)
{
	return Node(serd_new_curie(str.data()));
}

inline Node
make_uri(StringView str)
{
	return Node(serd_new_uri(str.data()));
}

inline Node
make_resolved_uri(StringView str, const NodeView& base)
{
	return Node(serd_new_resolved_uri(str.data(), base.cobj()));
}

inline Node
make_file_uri(StringView path)
{
	return Node(serd_new_file_uri(path.data(), nullptr));
}

inline Node
make_file_uri(StringView path, StringView hostname)
{
	return Node(serd_new_file_uri(path.data(), hostname.data()));
}

inline Node
make_relative_uri(StringView         str,
                  const NodeView&    base,
                  Optional<NodeView> root = {})
{
	return Node(serd_new_relative_uri(str.data(), base.cobj(), root.cobj()));
}

inline Node
make_decimal(double d, unsigned frac_digits, Optional<NodeView> datatype = {})
{
	return Node(serd_new_decimal(d, frac_digits, datatype.cobj()));
}

inline Node
make_integer(int64_t i, Optional<NodeView> datatype = {})
{
	return Node(serd_new_integer(i, datatype.cobj()));
}

inline Node
make_blob(const void*        buf,
          size_t             size,
          bool               wrap_lines,
          Optional<NodeView> datatype = {})
{
	return Node(serd_new_blob(buf, size, wrap_lines, datatype.cobj()));
}

/**
   @}
   @name URI
   @{
*/

inline std::string
file_uri_parse(StringView uri, std::string* hostname = nullptr)
{
	char* c_hostname = nullptr;
	char* c_path     = serd_file_uri_parse(uri.data(), &c_hostname);
	if (hostname && c_hostname) {
		*hostname = c_hostname;
	}
	const std::string path(c_path);
	serd_free(c_hostname);
	serd_free(c_path);
	return path;
}

/**
   A parsed URI

   This directly refers to slices in other strings, it does not own any memory
   itself.  Thus, URIs can be parsed and/or resolved against a base URI
   in-place without allocating memory.
*/
class URI
{
public:
	/**
	   Component of a URI.

	   Note that there is a distinction between a component being non-present
	   and present but empty.  For example, "file:///path" has an empty
	   authority, while "file:/path" has no authority.  A non-present component
	   has its `data()` pointer set to null, while an empty component has a
	   data pointer, but length zero.
	*/
	using Component = StringView;

	explicit URI(StringView str) : _uri(SERD_URI_NULL)
	{
		serd_uri_parse(str.data(), &_uri);
	}

	explicit URI(const NodeView& node) : _uri(SERD_URI_NULL)
	{
		serd_uri_parse(node.c_str(), &_uri);
	}

	explicit URI(const SerdURI& uri) : _uri(uri) {}

	Component scheme() const { return make_component(_uri.scheme); }
	Component authority() const { return make_component(_uri.authority); }
	Component path_base() const { return make_component(_uri.path_base); }
	Component path() const { return make_component(_uri.path); }
	Component query() const { return make_component(_uri.query); }
	Component fragment() const { return make_component(_uri.fragment); }

	/// Return this URI resolved against `base`
	URI resolve(const URI& base) const
	{
		SerdURI resolved = SERD_URI_NULL;
		serd_uri_resolve(&_uri, &base._uri, &resolved);
		return URI{resolved};
	}

	const SerdURI* cobj() const { return &_uri; }

private:
	static Component make_component(const SerdStringView slice)
	{
		return slice.buf ? Component{slice.buf, slice.len} : Component{};
	}

	SerdURI _uri;
};

inline std::ostream&
operator<<(std::ostream& os, const URI& uri)
{
	detail::TextSink sink{os};
	serd_uri_serialise(uri.cobj(), detail::TextSink::s_write, &sink);
	return os;
}

/**
   @}
   @name Cursor
   @{
*/

template <typename CObj>
using CursorHandle = detail::
        Copyable<CObj, serd_cursor_copy, serd_cursor_equals, serd_cursor_free>;

template <typename CObj>
class CursorWrapper : public CursorHandle<CObj>
{
public:
	using Base = CursorHandle<CObj>;
	using Base::cobj;

	explicit CursorWrapper(CObj* const cursor) : Base(cursor) {}

	template <typename C>
	// NOLINTNEXTLINE(hicpp-explicit-conversions)
	CursorWrapper(const CursorWrapper<C>& cursor) : CursorWrapper{cursor}
	{
	}

	NodeView name() const { return NodeView(serd_cursor_get_name(cobj())); }
	unsigned line() const { return serd_cursor_get_line(cobj()); }
	unsigned column() const { return serd_cursor_get_column(cobj()); }
};

using CursorView = CursorWrapper<const SerdCursor>;

/// Extra data managed by mutable (user created) Cursor
struct CursorData
{
	Node name_node;
};

class Cursor : private CursorData, public CursorWrapper<SerdCursor>
{
public:
	Cursor(const NodeView& name, const unsigned line, const unsigned col)
	    : CursorData{Node{name}}
	    , CursorWrapper{serd_cursor_new(name_node.cobj(), line, col)}
	{
	}

	template <typename C>
	// NOLINTNEXTLINE(hicpp-explicit-conversions)
	Cursor(const CursorWrapper<C>& cursor)
	    : Cursor(cursor.name(), cursor.line(), cursor.column())
	{
	}

private:
	friend class detail::Optional<Cursor>;
	friend class Statement;
	explicit Cursor(std::nullptr_t)
	    : CursorData{Node{nullptr}}, CursorWrapper{nullptr}
	{
	}
};

/**
   @}
   @name Logging
   @{
*/

/**
   @}
   @name World
   @{
*/

using LogFields = std::map<StringView, StringView>;

using MessageFunc =
        std::function<Status(StringView, LogLevel, LogFields, std::string)>;

class World : public detail::Wrapper<SerdWorld, serd_world_free>
{
public:
	World() : Wrapper(serd_world_new()) {}

	explicit World(SerdWorld* ptr) : Wrapper(ptr) {}

	NodeView get_blank() { return NodeView(serd_world_get_blank(cobj())); }

	void set_message_func(MessageFunc msg_func)
	{
		_msg_func = std::move(msg_func);
		serd_world_set_message_func(cobj(), s_message_func, this);
	}

	SERD_LOG_FUNC(5, 6)
	Status log(StringView        domain,
	           const LogLevel    level,
	           const LogFields&  fields,
	           const char* const fmt,
	           ...)
	{
		va_list args;
		va_start(args, fmt);

		std::unique_ptr<SerdLogField[]> c_fields(
		        new SerdLogField[fields.size()]);
		size_t index = 0;
		for (const auto& f : fields) {
			c_fields[index].key   = f.first.c_str();
			c_fields[index].value = f.second.c_str();
			++index;
		}

		const SerdStatus st = serd_world_vlogf(cobj(),
		                                       domain.c_str(),
		                                       static_cast<SerdLogLevel>(level),
		                                       c_fields.get(),
		                                       fields.size(),
		                                       fmt,
		                                       args);
		va_end(args);
		return static_cast<Status>(st);
	}

private:
	static std::string format(const char* fmt, va_list args) noexcept
	{
		va_list args_copy;
		va_copy(args_copy, args);
		const int n_bytes = vsnprintf(nullptr, 0, fmt, args_copy);
		va_end(args_copy);

#if __cplusplus >= 201703L
		std::string result(n_bytes, '\0');
		vsnprintf(result.data(), n_bytes + 1, fmt, args);
#else
		std::vector<char> str(n_bytes + 1U, '\0');
		vsnprintf(str.data(), n_bytes + 1U, fmt, args);
		std::string result(str.data(), size_t(n_bytes));
#endif
		return result;
	}

	static SerdStatus s_message_func(void*               handle,
	                                 const char*         domain,
	                                 SerdLogLevel        level,
	                                 const SerdLogField* fields,
	                                 size_t              n_fields,
	                                 const char*         fmt,
	                                 va_list             args) noexcept
	{
		const auto* const self = static_cast<const World*>(handle);
		try {
			LogFields cxx_fields;
			for (size_t i = 0; i < n_fields; ++i) {
				cxx_fields.emplace(fields[i].key, fields[i].value);
			}

			return static_cast<SerdStatus>(
			        self->_msg_func(domain,
			                        static_cast<LogLevel>(level),
			                        cxx_fields,
			                        format(fmt, args)));
		} catch (...) {
			return SERD_ERR_INTERNAL;
		}
	}

	MessageFunc _msg_func;
};

/**
   @}
   @name Statement
   @{
*/

template <typename CObj>
using StatementHandle = detail::Copyable<CObj,
                                         serd_statement_copy,
                                         serd_statement_equals,
                                         serd_statement_free>;

template <typename CObj>
class StatementBase;

using StatementView = StatementBase<const SerdStatement>;

/// Extra data managed by mutable (user created) Statement
struct StatementData
{
	Node             _subject;
	Node             _predicate;
	Node             _object;
	Node             _graph;
	Optional<Cursor> _cursor;
};

template <typename CObj>
class StatementBase : public StatementHandle<CObj>
{
public:
	using Base = StatementHandle<CObj>;
	using Base::cobj;

	explicit StatementBase(CObj* statement) : Base{statement} {}

	template <typename C>
	// NOLINTNEXTLINE(hicpp-explicit-conversions)
	StatementBase(const StatementBase<C>& statement) : Base{statement}
	{
	}

	NodeView node(Field field) const
	{
		return NodeView(
		        serd_statement_get_node(cobj(), static_cast<SerdField>(field)));
	}

	NodeView subject() const
	{
		return NodeView(serd_statement_get_subject(cobj()));
	}

	NodeView predicate() const
	{
		return NodeView(serd_statement_get_predicate(cobj()));
	}

	NodeView object() const
	{
		return NodeView(serd_statement_get_object(cobj()));
	}

	NodeView graph() const
	{
		return NodeView(serd_statement_get_graph(cobj()));
	}

	Optional<CursorView> cursor() const
	{
		return CursorView(serd_statement_get_cursor(cobj()));
	}

	StatementBase*       operator->() { return this; }
	const StatementBase* operator->() const { return this; }
};

class Statement : public StatementData, public StatementBase<SerdStatement>
{
public:
	Statement(const NodeView&      s,
	          const NodeView&      p,
	          const NodeView&      o,
	          const NodeView&      g,
	          Optional<CursorView> cursor)
	    : StatementData{s, p, o, g, cursor ? *cursor : Optional<Cursor>{}}
	    , StatementBase{serd_statement_new(_subject.cobj(),
	                                       _predicate.cobj(),
	                                       _object.cobj(),
	                                       _graph.cobj(),
	                                       _cursor.cobj())}
	{
	}

	template <typename C>
	// NOLINTNEXTLINE(hicpp-explicit-conversions)
	Statement(const StatementBase<C>& statement)
	    : StatementData{statement.subject(),
	                    statement.predicate(),
	                    statement.object(),
	                    statement.graph(),
	                    statement.cursor() ? *statement.cursor()
	                                       : Optional<Cursor>{}}
	    , StatementBase{statement}
	{
	}
};

/**
   @}
   @name Sink
   @{
*/

/**
   Sink function for base URI changes

   Called whenever the base URI of the serialisation changes.
*/
using BaseFunc = std::function<Status(NodeView)>;

/**
   Sink function for namespace definitions

   Called whenever a prefix is defined in the serialisation.
*/
using PrefixFunc = std::function<Status(NodeView name, NodeView uri)>;

/**
   Sink function for statements

   Called for every RDF statement in the serialisation.
*/
using StatementFunc = std::function<Status(StatementFlags, StatementView)>;

/**
   Sink function for anonymous node end markers

   This is called to indicate that the given anonymous node will no longer be
   referred to by any future statements (the anonymous serialisation of the
   node is finished).
*/
using EndFunc = std::function<Status(NodeView)>;

template <typename CSink>
class SinkBase : public detail::Wrapper<CSink, serd_sink_free>
{
public:
	explicit SinkBase(CSink* ptr) : detail::Wrapper<CSink, serd_sink_free>(ptr)
	{
	}

	template <typename C>
	// NOLINTNEXTLINE(hicpp-explicit-conversions)
	SinkBase(const SinkBase<C>& sink)
	    : detail::Wrapper<CSink, serd_sink_free>(sink)
	{
	}

	Status base(const NodeView& uri) const
	{
		return Status(serd_sink_write_base(this->cobj(), uri.cobj()));
	}

	Status prefix(const NodeView& name, const NodeView& uri) const
	{
		return Status(
		        serd_sink_write_prefix(this->cobj(), name.cobj(), uri.cobj()));
	}

	Status statement(StatementFlags flags, StatementView statement) const
	{
		return Status(serd_sink_write_statement(
		        this->cobj(), flags, statement.cobj()));
	}

	Status write(StatementFlags     flags,
	             const NodeView&    subject,
	             const NodeView&    predicate,
	             const NodeView&    object,
	             Optional<NodeView> graph = {}) const
	{
		return Status(serd_sink_write(this->cobj(),
		                              flags,
		                              subject.cobj(),
		                              predicate.cobj(),
		                              object.cobj(),
		                              graph.cobj()));
	}

	Status end(const NodeView& node) const
	{
		return Status(serd_sink_write_end(this->cobj(), node.cobj()));
	}

private:
	static SerdStatus s_base(void* handle, const SerdNode* uri) noexcept
	{
		auto* const sink = static_cast<const SinkBase*>(handle);
		return sink->_base_func ? SerdStatus(sink->_base_func(NodeView(uri)))
		                        : SERD_SUCCESS;
	}

	static SerdStatus
	s_prefix(void* handle, const SerdNode* name, const SerdNode* uri) noexcept
	{
		auto* const sink = static_cast<const SinkBase*>(handle);
		return sink->_prefix_func ? SerdStatus(sink->_prefix_func(
		                                    NodeView(name), NodeView(uri)))
		                          : SERD_SUCCESS;
	}

	static SerdStatus s_statement(void*                handle,
	                              SerdStatementFlags   flags,
	                              const SerdStatement* statement) noexcept
	{
		auto* const sink = static_cast<const SinkBase*>(handle);
		return sink->_statement_func ? SerdStatus(sink->_statement_func(
		                                       StatementFlags(flags),
		                                       StatementView(statement)))
		                             : SERD_SUCCESS;
	}

	static SerdStatus s_end(void* handle, const SerdNode* node) noexcept
	{
		auto* const sink = static_cast<const SinkBase*>(handle);
		return sink->_end_func ? SerdStatus(sink->_end_func(NodeView(node)))
		                       : SERD_SUCCESS;
	}
};

class SinkView : public SinkBase<const SerdSink>
{
public:
	explicit SinkView(const SerdSink* ptr) : SinkBase<const SerdSink>(ptr) {}

	// NOLINTNEXTLINE(hicpp-explicit-conversions)
	SinkView(const SinkBase<SerdSink>& sink)
	    : SinkBase<const SerdSink>(sink.cobj())
	{
	}
};

class Sink : public SinkBase<SerdSink>
{
public:
	Sink() : SinkBase(serd_sink_new(this, nullptr)) {}

	// EnvRef env() const { return serd_sink_get_env(cobj()); }

	Status set_base_func(BaseFunc base_func)
	{
		_base_func = std::move(base_func);
		return Status(serd_sink_set_base_func(cobj(), s_base));
	}

	Status set_prefix_func(PrefixFunc prefix_func)
	{
		_prefix_func = std::move(prefix_func);
		return Status(serd_sink_set_prefix_func(cobj(), s_prefix));
	}

	Status set_statement_func(StatementFunc statement_func)
	{
		_statement_func = std::move(statement_func);
		return Status(serd_sink_set_statement_func(cobj(), s_statement));
	}

	Status set_end_func(EndFunc end_func)
	{
		_end_func = std::move(end_func);
		return Status(serd_sink_set_end_func(cobj(), s_end));
	}

private:
	static SerdStatus s_base(void* handle, const SerdNode* uri) noexcept
	{
		auto* const sink = static_cast<const Sink*>(handle);
		return sink->_base_func ? SerdStatus(sink->_base_func(NodeView(uri)))
		                        : SERD_SUCCESS;
	}

	static SerdStatus
	s_prefix(void* handle, const SerdNode* name, const SerdNode* uri) noexcept
	{
		auto* const sink = static_cast<const Sink*>(handle);
		return sink->_prefix_func ? SerdStatus(sink->_prefix_func(
		                                    NodeView(name), NodeView(uri)))
		                          : SERD_SUCCESS;
	}

	static SerdStatus s_statement(void*                handle,
	                              SerdStatementFlags   flags,
	                              const SerdStatement* statement) noexcept
	{
		auto* const sink = static_cast<const Sink*>(handle);
		return sink->_statement_func ? SerdStatus(sink->_statement_func(
		                                       StatementFlags(flags),
		                                       StatementView(statement)))
		                             : SERD_SUCCESS;
	}

	static SerdStatus s_end(void* handle, const SerdNode* node) noexcept
	{
		auto* const sink = static_cast<const Sink*>(handle);
		return sink->_end_func ? SerdStatus(sink->_end_func(NodeView(node)))
		                       : SERD_SUCCESS;
	}

	BaseFunc      _base_func;
	PrefixFunc    _prefix_func;
	StatementFunc _statement_func;
	EndFunc       _end_func;
};

/**
   @}
   @name Environment
   @{
*/

template <typename CObj>
using EnvHandle =
        detail::Copyable<CObj, serd_env_copy, serd_env_equals, serd_env_free>;

template <typename CObj>
class EnvWrapper : public EnvHandle<CObj>
{
public:
	using Base = EnvHandle<CObj>;
	using Base::cobj;

	explicit EnvWrapper(CObj* ptr) : Base(ptr) {}

	/// Return the base URI
	NodeView base_uri() const
	{
		return NodeView(serd_env_get_base_uri(cobj()));
	}

	/// Qualify `uri` into a CURIE if possible
	Optional<Node> qualify(const NodeView& uri) const
	{
		return Node(serd_env_qualify(cobj(), uri.cobj()));
	}

	/// Expand `node` into an absolute URI if possible
	Optional<Node> expand(const NodeView& node) const
	{
		return Node(serd_env_expand(cobj(), node.cobj()));
	}

	/// Send all prefixes to `sink`
	void write_prefixes(SinkView sink) const
	{
		serd_env_write_prefixes(cobj(), sink.cobj());
	}
};

using EnvView = EnvWrapper<const SerdEnv>;

class Env : public EnvWrapper<SerdEnv>
{
public:
	Env() : EnvWrapper(serd_env_new(nullptr)) {}
	explicit Env(const NodeView& base) : EnvWrapper(serd_env_new(base.cobj()))
	{
	}
	// explicit Env(SerdEnv* ptr) : EnvWrapper(ptr) {}
	// explicit Env(const SerdEnv* ptr) : EnvWrapper(serd_env_copy(ptr)) {}
	// explicit Env(const EnvRef& ref) : EnvWrapper(serd_env_copy(ref.cobj()))
	// {}

	/// Set the base URI
	Status set_base_uri(const NodeView& uri)
	{
		return Status(serd_env_set_base_uri(cobj(), uri.cobj()));
	}

	/// Set a namespace prefix
	Status set_prefix(const NodeView& name, const NodeView& uri)
	{
		return Status(serd_env_set_prefix(cobj(), name.cobj(), uri.cobj()));
	}

	/// Set a namespace prefix
	Status set_prefix(StringView name, StringView uri)
	{
		return Status(serd_env_set_prefix_from_strings(
		        cobj(), name.data(), uri.data()));
	}
};

/**
   @}
   @name Reader
   @{
*/

class Reader : public detail::Wrapper<SerdReader, serd_reader_free>
{
public:
	Reader(World&      world,
	       Syntax      syntax,
	       ReaderFlags flags,
	       SinkView    sink,
	       size_t      stack_size)
	    : Wrapper(serd_reader_new(world.cobj(),
	                              SerdSyntax(syntax),
	                              SerdReaderFlags(flags),
	                              sink.cobj(),
	                              stack_size))
	{
	}

	void add_blank_prefix(StringView prefix)
	{
		serd_reader_add_blank_prefix(cobj(), prefix.data());
	}

	Status start_file(StringView uri, bool bulk = true)
	{
		return Status(serd_reader_start_file(cobj(), uri.data(), bulk));
	}

	Status start_file(const NodeView& uri, bool bulk = true)
	{
		if (uri.type() != NodeType::URI) {
			return Status::err_bad_arg;
		}

		return Status(start_file(file_uri_parse(uri.str()), bulk));
	}

	Status start_stream(FILE*              stream,
	                    Optional<NodeView> name      = {},
	                    size_t             page_size = 1)
	{
		return Status(serd_reader_start_stream(
		        cobj(),
		        reinterpret_cast<SerdReadFunc>(fread),
		        reinterpret_cast<SerdStreamErrorFunc>(ferror),
		        stream,
		        name.cobj(),
		        page_size));
	}

	Status start_stream(std::istream&      stream,
	                    Optional<NodeView> name      = {},
	                    size_t             page_size = 1)
	{
		return Status(serd_reader_start_stream(cobj(),
		                                       s_stream_read,
		                                       s_stream_error,
		                                       &stream,
		                                       name.cobj(),
		                                       page_size));
	}

	Status start_string(StringView utf8, Optional<NodeView> name = {})
	{
		return Status(
		        serd_reader_start_string(cobj(), utf8.data(), name.cobj()));
	}

	Status read_chunk() { return Status(serd_reader_read_chunk(cobj())); }

	Status read_document() { return Status(serd_reader_read_document(cobj())); }

	Status finish() { return Status(serd_reader_finish(cobj())); }

private:
	static inline size_t
	s_stream_read(void* buf, size_t size, size_t nmemb, void* stream) noexcept
	{
		assert(size == 1);
		try {
			auto* const s = static_cast<std::istream*>(stream);
			s->read(static_cast<char*>(buf), std::streamsize(nmemb));
			if (s->good()) {
				return nmemb;
			}
		} catch (...) {
		}
		return 0;
	}

	static inline int s_stream_error(void* stream) noexcept
	{
		try {
			auto* const s = static_cast<std::istream*>(stream);
			return (!(s->good()));
		} catch (...) {
		}
		return 1;
	}
};

/**
   @}
   @name Byte Streams
   @{
*/

/**
   @}
   @name Writer
   @{
*/

class Writer : public detail::Wrapper<SerdWriter, serd_writer_free>
{
public:
	Writer(World&            world,
	       const Syntax      syntax,
	       const WriterFlags flags,
	       Env&              env,
	       WriteFunc         sink)
	    : Wrapper(serd_writer_new(world.cobj(),
	                              SerdSyntax(syntax),
	                              flags,
	                              env.cobj(),
	                              detail::TextSink::s_write,
	                              &_text_sink))
	    , _text_sink(std::move(sink))
	{
	}

	Writer(World&            world,
	       const Syntax      syntax,
	       const WriterFlags flags,
	       Env&              env,
	       std::ostream&     stream)
	    : Wrapper(serd_writer_new(world.cobj(),
	                              SerdSyntax(syntax),
	                              flags,
	                              env.cobj(),
	                              detail::TextSink::s_write,
	                              &_text_sink))
	    , _text_sink(stream)
	{
	}

	SinkView sink() { return SinkView{serd_writer_get_sink(cobj())}; }

	// EnvView env() { return EnvView{serd_writer_get_env(cobj())}; }

	Status set_root_uri(const NodeView& uri)
	{
		return Status(serd_writer_set_root_uri(cobj(), uri.cobj()));
	}

	Status finish() { return Status(serd_writer_finish(cobj())); }

private:
	detail::TextSink _text_sink;
};

/**
   @}
*/

template <typename CObj>
using IterHandle = detail::
        Copyable<CObj, serd_iter_copy, serd_iter_equals, serd_iter_free>;

template <typename CObj>
class IterBase : public IterHandle<CObj>
{
public:
	using Base = IterHandle<CObj>;
	using Base::cobj;

	explicit IterBase(CObj* ptr) : Base(ptr) {}

	// template <typename = std::enable_if<!std::is_const<CObj>::value>>
	// IterBase(const IterBase<const CObj, detail::no_free>& other) :
	// Base(other)
	// {
	// }

	StatementView operator*() const
	{
		return StatementView(serd_iter_get(cobj()));
	}

	StatementView operator->() const
	{
		return StatementView{serd_iter_get(cobj())};
	}
};

using IterView = IterBase<const SerdIter>;

class Iter : public IterBase<SerdIter>
{
public:
	explicit Iter(SerdIter* ptr) : IterBase(ptr) {}
	explicit Iter(const SerdIter* ptr) : IterBase(serd_iter_copy(ptr)) {}
	explicit Iter(IterView ref) : IterBase(serd_iter_copy(ref.cobj())) {}

	IterBase& operator++()
	{
		serd_iter_next(this->cobj());
		return *this;
	}
};

class Range : public detail::Copyable<SerdRange,
                                      serd_range_copy,
                                      serd_range_equals,
                                      serd_range_free>
{
public:
	using Base = detail::Copyable<SerdRange,
	                              serd_range_copy,
	                              serd_range_equals,
	                              serd_range_free>;

	explicit Range(SerdRange* r) : Copyable(r) {}
	// explicit Range(const SerdRange* r) : Copyable(r) {}

	Iter begin() const { return Iter(serd_range_begin(cobj())); }
	Iter end() const { return Iter(serd_range_end(cobj())); }

	Status serialise(SinkView sink, SerialisationFlags flags = {})
	{
		return Status(serd_range_serialise(cobj(), sink.cobj(), flags));
	}
};

using ModelHandle = detail::Copyable<SerdModel,
                                     serd_model_copy,
                                     serd_model_equals,
                                     serd_model_free>;

class Model : public ModelHandle
{
public:
	using Base = ModelHandle;

	using value_type     = Statement;
	using iterator       = Iter;
	using const_iterator = Iter;

	Model(World& world, ModelFlags flags)
	    : Base(serd_model_new(world.cobj(), flags))
	{
	}

	Model(World& world, ModelFlag flag)
	    : Base(serd_model_new(world.cobj(), ModelFlags(flag)))
	{
	}

	size_t size() const { return serd_model_size(cobj()); }

	bool empty() const { return serd_model_empty(cobj()); }

	void insert(StatementView s) { serd_model_insert(cobj(), s.cobj()); }

	void insert(const NodeView&    s,
	            const NodeView&    p,
	            const NodeView&    o,
	            Optional<NodeView> g = {})
	{
		serd_model_add(cobj(), s.cobj(), p.cobj(), o.cobj(), g.cobj());
	}

	Iter find(Optional<NodeView> s,
	          Optional<NodeView> p,
	          Optional<NodeView> o,
	          Optional<NodeView> g = {}) const
	{
		return Iter(serd_model_find(
		        cobj(), s.cobj(), p.cobj(), o.cobj(), g.cobj()));
	}

	Range range(Optional<NodeView> s,
	            Optional<NodeView> p,
	            Optional<NodeView> o,
	            Optional<NodeView> g = {}) const
	{
		return Range(serd_model_range(
		        cobj(), s.cobj(), p.cobj(), o.cobj(), g.cobj()));
	}

	Optional<NodeView> get(Optional<NodeView> s,
	                       Optional<NodeView> p,
	                       Optional<NodeView> o,
	                       Optional<NodeView> g = {}) const
	{
		return NodeView(
		        serd_model_get(cobj(), s.cobj(), p.cobj(), o.cobj(), g.cobj()));
	}

	Optional<StatementView> get_statement(Optional<NodeView> s,
	                                      Optional<NodeView> p,
	                                      Optional<NodeView> o,
	                                      Optional<NodeView> g = {}) const
	{
		return StatementView(serd_model_get_statement(
		        cobj(), s.cobj(), p.cobj(), o.cobj(), g.cobj()));
	}

	bool ask(Optional<NodeView> s,
	         Optional<NodeView> p,
	         Optional<NodeView> o,
	         Optional<NodeView> g = {}) const
	{
		return serd_model_ask(cobj(), s.cobj(), p.cobj(), o.cobj(), g.cobj());
	}

	size_t count(Optional<NodeView> s,
	             Optional<NodeView> p,
	             Optional<NodeView> o,
	             Optional<NodeView> g = {}) const
	{
		return serd_model_count(cobj(), s.cobj(), p.cobj(), o.cobj(), g.cobj());
	}

	Range all() const { return Range(serd_model_all(cobj())); }

	iterator begin() const { return iterator(serd_model_begin(cobj())); }

	iterator end() const { return iterator(serd_model_end(cobj())); }

private:
	friend class detail::Optional<Model>;
	explicit Model(std::nullptr_t) : Copyable(nullptr) {}
};

class Inserter : public detail::Wrapper<SerdInserter, serd_inserter_free>
{
public:
	Inserter(Model& model, Env& env, Optional<NodeView> default_graph = {})
	    : Wrapper(serd_inserter_new(model.cobj(),
	                                env.cobj(),
	                                default_graph.cobj()))
	{
	}

	SinkView sink() { return SinkView{serd_inserter_get_sink(cobj())}; }
};

} // namespace serd

/**
   @}
*/

#endif // SERD_SERD_HPP
