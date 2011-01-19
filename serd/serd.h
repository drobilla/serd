/* Serd, an RDF serialisation library.
 * Copyright 2011 David Robillard <d@drobilla.net>
 * 
 * Serd is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Serd is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SERD_SERD_H
#define SERD_SERD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef SERD_SHARED
	#if defined _WIN32 || defined __CYGWIN__
		#define SERD_LIB_IMPORT __declspec(dllimport)
		#define SERD_LIB_EXPORT __declspec(dllexport)
	#else
		#define SERD_LIB_IMPORT __attribute__ ((visibility("default")))
		#define SERD_LIB_EXPORT __attribute__ ((visibility("default")))
	#endif
	#ifdef SERD_INTERNAL
		#define SERD_API SERD_LIB_EXPORT
	#else
		#define SERD_API SERD_LIB_IMPORT
	#endif
#else
	#define SERD_API
#endif

typedef enum {
	SERD_TURTLE   = 1,
	SERD_NTRIPLES = 2
} SerdSyntax;


/** @defgroup string Measured UTF-8 string
 * @{
 */

typedef struct {
	size_t  n_bytes;
	size_t  n_chars;
	uint8_t buf[];
} SerdString;

SERD_API
SerdString*
serd_string_new(const uint8_t* utf8);

SERD_API
SerdString*
serd_string_copy(const SerdString* s);

typedef enum {
	BLANK   = 1,
	URI     = 2,
	QNAME   = 3,
	LITERAL = 4
} SerdNodeType;

/** @} */

    
/** @defgroup uri Parsed URI
 * @{
 */

typedef struct {
	const uint8_t* buf;
	size_t         len;
} SerdRange;

typedef struct {
	SerdRange scheme;
	SerdRange authority;
	SerdRange path_base;
	SerdRange path;
	SerdRange query;
	SerdRange fragment;
	bool      base_uri_has_authority;
} SerdURI;

SERD_API
bool
serd_uri_string_is_relative(const uint8_t* utf8);

SERD_API
bool
serd_uri_parse(const uint8_t* utf8, SerdURI* out);

SERD_API
bool
serd_uri_resolve(const SerdURI* uri, const SerdURI* base, SerdURI* out);

SERD_API
bool
serd_uri_write(const SerdURI* uri, FILE* file);

SERD_API
SerdString*
serd_uri_serialise(const SerdURI* uri,
                   SerdURI*       out);

/** @} */


/** @defgroup reader Reader for RDF syntax
 * @{
 */

typedef struct SerdReaderImpl* SerdReader;

typedef bool (*SerdBaseHandler)(void*             handle,
                                const SerdString* uri);

typedef bool (*SerdPrefixHandler)(void*             handle,
                                  const SerdString* name,
                                  const SerdString* uri);

typedef bool (*SerdStatementHandler)(void*             handle,
                                     const SerdString* graph,
                                     const SerdString* subject,
                                     SerdNodeType      subject_type,
                                     const SerdString* predicate,
                                     SerdNodeType      predicate_type,
                                     const SerdString* object,
                                     SerdNodeType      object_type,
                                     const SerdString* object_lang,
                                     const SerdString* object_datatype);

SERD_API
SerdReader
serd_reader_new(SerdSyntax           syntax,
                void*                handle,
                SerdBaseHandler      base_handler,
                SerdPrefixHandler    prefix_handler,
                SerdStatementHandler statement_handler);

SERD_API
bool
serd_reader_read_file(SerdReader     reader,
                      FILE*          file,
                      const uint8_t* name);

SERD_API
void
serd_reader_free(SerdReader reader);

/** @} */


/** @defgroup namespaces Namespaces (prefixes for CURIEs)
 * @{
 */

typedef struct SerdNamespacesImpl* SerdNamespaces;

SERD_API
SerdNamespaces
serd_namespaces_new();

SERD_API
void
serd_namespaces_free(SerdNamespaces ns);

SERD_API
void
serd_namespaces_add(SerdNamespaces    ns,
                    const SerdString* name,
                    const SerdString* uri);

SERD_API
bool
serd_namespaces_expand(SerdNamespaces     ns,
                       const SerdString*  qname,
                       SerdRange*         uri_prefix,
                       SerdRange*         uri_suffix);

/** @} */

#endif /* SERD_SERD_H */
