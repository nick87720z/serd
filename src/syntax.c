/*
  Copyright 2011-2018 David Robillard <http://drobilla.net>

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

#include "string_utils.h"

#include "serd/serd.h"

#include <stdbool.h>
#include <string.h>

typedef struct {
	SerdSyntax  syntax;
	const char* name;
	const char* extension;
} Syntax;

static const Syntax syntaxes[] = {
	{SERD_TURTLE,   "turtle",   ".ttl"},
	{SERD_NTRIPLES, "ntriples", ".nt"},
	{SERD_NQUADS,   "nquads",   ".nq"},
	{SERD_TRIG,     "trig",     ".trig"},
	{(SerdSyntax)0, NULL, NULL}
};

SerdSyntax
serd_syntax_by_name(const char* const name)
{
	for (const Syntax* s = syntaxes; s->name; ++s) {
		if (!serd_strncasecmp(s->name, name, strlen(name))) {
			return s->syntax;
		}
	}
	return (SerdSyntax)0;
}

SerdSyntax
serd_guess_syntax(const char* const filename)
{
	const char* ext = strrchr(filename, '.');
	if (ext) {
		for (const Syntax* s = syntaxes; s->name; ++s) {
			if (!serd_strncasecmp(s->extension, ext, strlen(ext))) {
				return s->syntax;
			}
		}
	}
	return (SerdSyntax)0;
}

bool
serd_syntax_has_graphs(const SerdSyntax syntax)
{
	return syntax == SERD_NQUADS || syntax == SERD_TRIG;
}
