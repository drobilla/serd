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

#include "serd_config.h"

#include "model.h"
#include "serd/serd.h"
#include "world.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_PCRE
#include <pcre.h>
#endif

#define NS_owl  "http://www.w3.org/2002/07/owl#"
#define NS_rdf  "http://www.w3.org/1999/02/22-rdf-syntax-ns#"
#define NS_rdfs "http://www.w3.org/2000/01/rdf-schema#"
#define NS_xsd  "http://www.w3.org/2001/XMLSchema#"

#define VERRORF(ctx, statement, fmt, ...)                                      \
	report(ctx, statement, SERD_LOG_LEVEL_ERR, fmt, __VA_ARGS__);

#define VERROR(ctx, statement, fmt)                                            \
	report(ctx, statement, SERD_LOG_LEVEL_ERR, fmt);

#define VWARNF(ctx, statement, fmt, ...)                                       \
	report(ctx, statement, SERD_LOG_LEVEL_WARNING, fmt, __VA_ARGS__);

#define VNOTEF(ctx, statement, fmt, ...)                                       \
	report(ctx, statement, SERD_LOG_LEVEL_NOTICE, fmt, __VA_ARGS__);

#define VNOTE(ctx, statement, fmt)                                             \
	report(ctx, statement, SERD_LOG_LEVEL_NOTICE, fmt);

#define SERD_FOREACH(name, range)                                              \
	for (const SerdStatement* (name) = NULL;                                   \
	     !serd_range_empty(range) && ((name) = serd_range_front(range));       \
	     serd_range_next(range))

typedef struct
{
	SerdNode* owl_Class;
	SerdNode* owl_DatatypeProperty;
	SerdNode* owl_FunctionalProperty;
	SerdNode* owl_InverseFunctionalProperty;
	SerdNode* owl_ObjectProperty;
	SerdNode* owl_Restriction;
	SerdNode* owl_Thing;
	SerdNode* owl_allValuesFrom;
	SerdNode* owl_cardinality;
	SerdNode* owl_equivalentClass;
	SerdNode* owl_maxCardinality;
	SerdNode* owl_minCardinality;
	SerdNode* owl_onDatatype;
	SerdNode* owl_onProperty;
	SerdNode* owl_someValuesFrom;
	SerdNode* owl_withRestrictions;
	SerdNode* rdf_PlainLiteral;
	SerdNode* rdf_Property;
	SerdNode* rdf_first;
	SerdNode* rdf_rest;
	SerdNode* rdf_type;
	SerdNode* rdfs_Class;
	SerdNode* rdfs_Datatype;
	SerdNode* rdfs_Literal;
	SerdNode* rdfs_Resource;
	SerdNode* rdfs_domain;
	SerdNode* rdfs_label;
	SerdNode* rdfs_range;
	SerdNode* rdfs_subClassOf;
	SerdNode* xsd_anyURI;
	SerdNode* xsd_float;
	SerdNode* xsd_decimal;
	SerdNode* xsd_double;
	SerdNode* xsd_maxExclusive;
	SerdNode* xsd_maxInclusive;
	SerdNode* xsd_minExclusive;
	SerdNode* xsd_minInclusive;
	SerdNode* xsd_pattern;
	SerdNode* xsd_string;
	SerdNode* sentinel;
} URIs;

typedef struct
{
	URIs             uris;
	const SerdModel* model;
	unsigned         n_errors;
	unsigned         n_restrictions;
	bool             quiet;
} ValidationContext;

static int
check_class_restriction(ValidationContext*   ctx,
                        const SerdNode*      restriction,
                        const SerdStatement* statement,
                        const SerdNode*      instance);

static int
report(ValidationContext*   ctx,
       const SerdStatement* statement,
       const SerdLogLevel   level,
       const char*          fmt,
       ...)
{
	if (ctx->quiet) {
		return 0;
	}

	va_list args;
	va_start(args, fmt);
	serd_world_vlogf_internal(ctx->model->world, SERD_ERR_INVALID,
	                          level,
	                          serd_statement_get_cursor(statement),
	                          fmt,
	                          args);
	va_end(args);

	++ctx->n_errors;
	return 1;
}

static bool
check(ValidationContext* ctx, const bool value)
{
	++ctx->n_restrictions;
	return value;
}

/** Return true iff `child` is a descendant of `parent` by `pred` arcs.
 *
 * That is, returns true if there is a path from `child` to `parent` by
 * following `pred` arcs starting from child.
 */
static bool
is_descendant(ValidationContext* ctx,
              const SerdNode*    child,
              const SerdNode*    parent,
              const SerdNode*    pred)
{
	if (serd_node_equals(child, parent) ||
	    serd_model_ask(
	        ctx->model, child, ctx->uris.owl_equivalentClass, parent, NULL)) {
		return true;
	}

	SerdRange* i = serd_model_range(ctx->model, child, pred, NULL, NULL);
	SERD_FOREACH (s, i) {
		const SerdNode* o = serd_statement_get_object(s);
		if (!serd_node_equals(child, o) &&
		    is_descendant(ctx, o, parent, pred)) {
			serd_range_free(i);
			return true;
		}
	}
	serd_range_free(i);

	return false;
}

/** Return true iff `child` is a subclass of `parent`. */
static bool
is_subclass(ValidationContext* ctx,
            const SerdNode*    child,
            const SerdNode*    parent)
{
	return is_descendant(ctx, child, parent, ctx->uris.rdfs_subClassOf);
}

/** Return true iff `child` is a sub-datatype of `parent`. */
static bool
is_subdatatype(ValidationContext* ctx,
               const SerdNode*    child,
               const SerdNode*    parent)
{
	return is_descendant(ctx, child, parent, ctx->uris.owl_onDatatype);
}

static bool
regexp_match(ValidationContext*   ctx,
             const SerdStatement* pat_statement,
             const char*          pat,
             const char*          str)
{
#ifdef HAVE_PCRE
	// Append a $ to the pattern so we only match if the entire string matches
	const size_t len  = strlen(pat);
	char* const  regx = (char*)malloc(len + 2);
	memcpy(regx, pat, len);
	regx[len]     = '$';
	regx[len + 1] = '\0';

	const char* err;
	int         erroffset;
	pcre*       re = pcre_compile(regx, PCRE_ANCHORED, &err, &erroffset, NULL);
	free(regx);
	if (!re) {
		VERRORF(ctx,
		        pat_statement,
		        "Error in pattern \"%s\" at offset %d (%s)\n",
		        pat,
		        erroffset,
		        err);
		return false;
	}

	const bool ret =
		pcre_exec(re, NULL, str, (int)strlen(str), 0, 0, NULL, 0) >= 0;

	pcre_free(re);
	return ret;
#else
	(void)ctx;
	(void)pat_statement;
	(void)pat;
	(void)str;
#endif // HAVE_PCRE
	return true;
}

static int
bound_cmp(ValidationContext* ctx,
          const SerdNode*    literal,
          const SerdNode*    type,
          const SerdNode*    bound)
{
	const char* str       = serd_node_get_string(literal);
	const char* bound_str = serd_node_get_string(bound);
	const bool is_numeric = (is_subdatatype(ctx, type, ctx->uris.xsd_decimal) ||
	                         is_subdatatype(ctx, type, ctx->uris.xsd_double));

	if (is_numeric) {
		const double fbound   = serd_strtod(bound_str, NULL);
		const double fliteral = serd_strtod(str, NULL);
		return ((fliteral < fbound) ? -1 : (fliteral > fbound) ? 1 : 0);
	} else {
		return strcmp(str, bound_str);
	}
}

static bool
check_literal_restriction(ValidationContext*   ctx,
                          const SerdStatement* statement,
                          const SerdNode*      literal,
                          const SerdNode*      type,
                          const SerdNode*      restriction)
{
	const char* str = serd_node_get_string(literal);

	// Check xsd:pattern
	const SerdStatement* pat_statement = serd_model_get_statement(
	    ctx->model, restriction, ctx->uris.xsd_pattern, 0, 0);
	if (pat_statement) {
		const SerdNode* pat_node = serd_statement_get_object(pat_statement);
		const char*     pat      = serd_node_get_string(pat_node);
		if (check(ctx, !regexp_match(ctx, pat_statement, pat, str))) {
			VERRORF(ctx,
			        statement,
			        "Value \"%s\" does not match pattern \"%s\"\n",
			        serd_node_get_string(literal),
			        pat);
			return false;
		}
	}

	// Check xsd:minInclusive
	const SerdNode* lower = serd_model_get(
	    ctx->model, restriction, ctx->uris.xsd_minInclusive, 0, 0);
	if (lower) {
		if (check(ctx, bound_cmp(ctx, literal, type, lower) < 0)) {
			VERRORF(ctx,
			        statement,
			        "Value \"%s\" < minimum \"%s\"\n",
			        serd_node_get_string(literal),
			        serd_node_get_string(lower));
			return false;
		}
	}

	// Check xsd:maxInclusive
	const SerdNode* upper = serd_model_get(
	    ctx->model, restriction, ctx->uris.xsd_maxInclusive, 0, 0);
	if (upper) {
		if (check(ctx, bound_cmp(ctx, literal, type, upper) > 0)) {
			VERRORF(ctx,
			        statement,
			        "Value \"%s\" > than maximum \"%s\"\n",
			        serd_node_get_string(literal),
			        serd_node_get_string(upper));
			return false;
		}
	}

	// Check xsd:minExclusive
	const SerdNode* elower = serd_model_get(
	    ctx->model, restriction, ctx->uris.xsd_minExclusive, 0, 0);
	if (elower) {
		if (check(ctx, bound_cmp(ctx, literal, type, elower) <= 0)) {
			VERRORF(ctx,
			        statement,
			        "Value \"%s\" <= exclusive minimum \"%s\"\n",
			        serd_node_get_string(literal),
			        serd_node_get_string(elower));
			return false;
		}
	}

	// Check xsd:maxExclusive
	const SerdNode* eupper = serd_model_get(
	    ctx->model, restriction, ctx->uris.xsd_maxExclusive, 0, 0);
	if (eupper) {
		if (check(ctx, bound_cmp(ctx, literal, type, eupper) >= 0)) {
			VERRORF(ctx,
			        statement,
			        "Value \"%s\" >= exclusive maximum \"%s\"\n",
			        serd_node_get_string(literal),
			        serd_node_get_string(eupper));
			return false;
		}
		++ctx->n_restrictions;
	}

	return true; // Unknown restriction, be quietly tolerant
}

static bool
is_datatype(ValidationContext* ctx, const SerdNode* dtype)
{
	SerdRange* t =
	    serd_model_range(ctx->model, dtype, ctx->uris.rdf_type, NULL, NULL);
	SERD_FOREACH (s, t) {
		const SerdNode* type = serd_statement_get_object(s);
		if (is_subdatatype(ctx, type, ctx->uris.rdfs_Datatype)) {
			serd_range_free(t);
			return true; // Subdatatype of rdfs:Datatype
		}
	}
	serd_range_free(t);

	return false;
}

static bool
literal_is_valid(ValidationContext*   ctx,
                 const SerdStatement* statement,
                 const SerdNode*      literal,
                 const SerdNode*      type)
{
	if (!type) {
		return true;
	}

	// Check that datatype is defined
	const SerdNode* datatype = serd_node_get_datatype(literal);
	if (datatype && !is_datatype(ctx, datatype)) {
		VERRORF(ctx,
		        statement,
		        "Datatype <%s> is not defined\n",
		        serd_node_get_string(datatype));
		return false;
	}

	// Find restrictions list
	const SerdNode* head =
	    serd_model_get(ctx->model, type, ctx->uris.owl_withRestrictions, 0, 0);

	// Walk list, checking each restriction
	while (head) {
		SerdIter* f =
		    serd_model_find(ctx->model, head, ctx->uris.rdf_first, 0, 0);
		if (!f) {
			break;
		}

		const SerdNode* first = serd_statement_get_object(serd_iter_get(f));

		// Check this restriction
		if (!check_literal_restriction(ctx, statement, literal, type, first)) {
			VNOTEF(ctx,
			       serd_iter_get(f),
			       "Restriction on <%s>\n",
			       serd_node_get_string(type));
			serd_iter_free(f);
			return false;
		}

		// Seek to next list node
		head = serd_model_get(ctx->model, head, ctx->uris.rdf_rest, 0, 0);
		serd_iter_free(f);
	}

	// Recurse up datatype hierarchy
	const SerdNode* super =
	    serd_model_get(ctx->model, type, ctx->uris.owl_onDatatype, 0, 0);
	return super ? literal_is_valid(ctx, statement, literal, super) : true;
}

static bool
is_a(ValidationContext* ctx, const SerdNode* subject, const SerdNode* type)
{
	return serd_model_ask(ctx->model, subject, ctx->uris.rdf_type, type, 0);
}

static bool
has_explicit_type(ValidationContext* ctx,
                  const SerdNode*    node,
                  const SerdNode*    klass)
{
	if (is_a(ctx, node, klass)) {
		return true; // Directly stated to be an instance
	}

	SerdRange* t =
	    serd_model_range(ctx->model, node, ctx->uris.rdf_type, NULL, NULL);
	SERD_FOREACH (s, t) {
		if (is_subclass(ctx, serd_statement_get_object(s), klass)) {
			serd_range_free(t);
			return true; // Explicit instance of a subclass
		}
	}

	serd_range_free(t);
	return false;
}

static bool
is_instance_of(ValidationContext* ctx,
               const SerdNode*    node,
               const SerdNode*    klass)
{
	if (!serd_model_ask(ctx->model, node, NULL, NULL, NULL)) {
		/* Nothing about this node known in the model at all, assume it is some
		   external resource we can't validate. */
		return true;
	} else if (serd_node_get_type(node) == SERD_BLANK) {
		/* Be permissive for blank nodes and don't require explicit type
		   annotation, to avoid countless errors with things like lists. */
		return true;
	}

	return (has_explicit_type(ctx, node, klass) ||
	        serd_node_equals(klass, ctx->uris.rdfs_Resource) ||
	        serd_node_equals(klass, ctx->uris.owl_Thing));
}

static bool
check_instance_type(ValidationContext*   ctx,
                    const SerdStatement* statement,
                    const SerdNode*      node,
                    const SerdNode*      klass)
{
	if (is_subclass(ctx, klass, ctx->uris.rdfs_Literal) ||
	    is_a(ctx, klass, ctx->uris.rdfs_Datatype)) {
		VERROR(ctx, statement, "Class instance found where literal expected\n");
		return false;
	}

	if (is_a(ctx, klass, ctx->uris.owl_Restriction)) {
		if (check_class_restriction(ctx, klass, statement, node)) {
			return false;
		}
	}

	SerdRange* r = serd_model_range(
		ctx->model, klass, ctx->uris.rdfs_subClassOf, NULL, NULL);
	SERD_FOREACH (s, r) {
		const SerdNode* super = serd_statement_get_object(s);
		if (!serd_node_equals(super, klass) &&
		    !check_instance_type(ctx, statement, node, super)) {
			serd_range_free(r);
			return false;
		}
	}
	serd_range_free(r);

	if (!is_instance_of(ctx, node, klass)) {
		VERRORF(ctx,
		        statement,
		        "Node %s is not an instance of %s\n",
		        serd_node_get_string(node),
		        serd_node_get_string(klass));
		return false;
	}

	return true;
}

static bool
check_type(ValidationContext*   ctx,
           const SerdStatement* statement,
           const SerdNode*      node,
           const SerdNode*      type)
{
	if (serd_node_equals(type, ctx->uris.rdfs_Resource) ||
	    serd_node_equals(type, ctx->uris.owl_Thing)) {
		return true; // Trivially true for everything (more or less)
	}

	if (serd_node_get_type(node) == SERD_LITERAL) {
		if (serd_node_equals(type, ctx->uris.rdfs_Literal)) {
			return true; // Trivially true for a literal
		} else if (serd_node_equals(type, ctx->uris.rdf_PlainLiteral)) {
			if (serd_node_get_datatype(node)) {
				VERRORF(ctx,
				        statement,
				        "Literal \"%s\" should be plain, but has datatype "
				        "<%s>\n",
				        serd_node_get_string(node),
				        serd_node_get_string(serd_node_get_datatype(node)));
				return false;
			}
		} else if (!is_a(ctx, type, ctx->uris.rdfs_Datatype)) {
			VERRORF(ctx,
			        statement,
			        "Literal \"%s\" where instance of <%s> expected\n",
			        serd_node_get_string(node),
			        serd_node_get_string(type));
			return false;
		} else {
			return literal_is_valid(ctx, statement, node, type);
		}
	} else if (serd_node_get_type(node) == SERD_URI) {
		if (!is_subdatatype(ctx, type, ctx->uris.xsd_anyURI)) {
			// Only check if type is not anyURI, since node is a URI
			return check_instance_type(ctx, statement, node, type);
		}
	} else {
		return check_instance_type(ctx, statement, node, type);
	}

	return true;
}

static uint64_t
count_non_blanks(SerdRange* i, SerdField field)
{
	uint64_t n = 0;
	SERD_FOREACH (s, i) {
		const SerdNode* node = serd_statement_get_node(s, field);
		if (serd_node_get_type(node) != SERD_BLANK) {
			++n;
		}
	}
	return n;
}

static int
check_statement(ValidationContext* ctx, const SerdStatement* statement)
{
	int             st   = 0;
	const URIs*     uris = &ctx->uris;
	const SerdNode* subj = serd_statement_get_subject(statement);
	const SerdNode* pred = serd_statement_get_predicate(statement);
	const SerdNode* obj  = serd_statement_get_object(statement);

	if (serd_node_equals(pred, uris->rdf_type)) {
		// Type statement, check that object is a valid instance of type
		check_type(ctx, statement, subj, obj);
	}

	if (!serd_model_ask(ctx->model, pred, uris->rdfs_label, 0, 0)) {
		// Warn if property has no label
		st = VWARNF(ctx,
		            statement,
		            "Property <%s> has no label\n",
		            serd_node_get_string(pred));
	}

	if (serd_node_get_type(obj) == SERD_LITERAL &&
	    !literal_is_valid(ctx, statement, obj, serd_node_get_datatype(obj))) {
		st = SERD_ERR_INVALID;
	}

	// Check restrictions based on property type
	if (is_a(ctx, pred, uris->owl_DatatypeProperty)) {
		if (serd_node_get_type(obj) != SERD_LITERAL) {
			st = VERROR(ctx, statement,
			            "Datatype property with non-literal value\n");
		}
	} else if (is_a(ctx, pred, uris->owl_ObjectProperty)) {
		if (serd_node_get_type(obj) == SERD_LITERAL) {
			st = VERROR(ctx, statement, "Object property with literal value\n");
		}
	} else if (is_a(ctx, pred, uris->owl_FunctionalProperty)) {
		SerdRange*     o = serd_model_range(ctx->model, subj, pred, NULL, NULL);
		const uint64_t n = count_non_blanks(o, SERD_OBJECT);
		if (n > 1) {
			st = VERRORF(ctx, statement,
			             "Functional property with %u objects\n", n);
		}
		serd_range_free(o);
	} else if (is_a(ctx, pred, uris->owl_InverseFunctionalProperty)) {
		SerdRange*     s = serd_model_range(ctx->model, NULL, pred, obj, NULL);
		const uint64_t n = count_non_blanks(s, SERD_SUBJECT);
		if (n > 1) {
			st = VERRORF(ctx, statement,
			             "Inverse functional property with %u subjects\n", n);
		}
		serd_range_free(s);
	} else {
		SerdRange* t = serd_model_range(ctx->model, pred, uris->rdf_type, 0, 0);

		bool is_property = false;
		SERD_FOREACH (s, t) {
			const SerdNode* type = serd_statement_get_object(s);
			if (is_subclass(ctx, type, uris->rdf_Property)) {
				is_property = true;
				break;
			}
		}

		if (!is_property) {
			st = VERROR(ctx, statement, "Use of undefined property\n");
		}

		serd_range_free(t);
	}

	// Check range
	SerdRange* r = serd_model_range(ctx->model, pred, uris->rdfs_range, 0, 0);
	SERD_FOREACH (s, r) {
		const SerdNode* range = serd_statement_get_object(s);
		if (!has_explicit_type(ctx, obj, range) &&
		    !check_type(ctx, statement, obj, range)) {
			VNOTEF(ctx, serd_range_front(r),
			       "In range of <%s>\n", serd_node_get_string(pred));
		}
	}
	serd_range_free(r);

	// Check domain
	SerdRange* d = serd_model_range(ctx->model, pred, uris->rdfs_domain, 0, 0);
	SERD_FOREACH (s, d) {
		const SerdNode* domain = serd_statement_get_object(s);
		if (!has_explicit_type(ctx, subj, domain) &&
		    !check_type(ctx, statement, subj, domain)) {
			VNOTEF(ctx, serd_range_front(d),
			       "In domain of <%s>\n", serd_node_get_string(pred));
		}
	}
	serd_range_free(d);

	return st;
}

static int
cardinality_error(ValidationContext*   ctx,
                  const SerdStatement* statement,
                  const SerdStatement* restriction_statement,
                  const SerdNode*      property,
                  const size_t         actual_values,
                  const char*          comparison,
                  const size_t         expected_values)
{
	const int st = VERRORF(ctx,
	                       statement,
	                       "Property <%s> has %zu %s %zu values\n",
	                       serd_node_get_string(property),
	                       actual_values,
	                       comparison,
	                       expected_values);
	VNOTE(ctx, restriction_statement, "Restriction here\n");
	return st;
}

static int
check_property_restriction(ValidationContext*   ctx,
                           const SerdNode*      restriction,
                           const SerdNode*      prop,
                           const SerdStatement* statement,
                           const SerdNode*      instance)
{
	typedef size_t Count;

	int          st = 0;
	const size_t values =
		serd_model_count(ctx->model, instance, prop, NULL, NULL);

	// Check exact cardinality
	const SerdStatement* c = serd_model_get_statement(
	    ctx->model, restriction, ctx->uris.owl_cardinality, NULL, NULL);
	if (c) {
		const SerdNode* card  = serd_statement_get_object(c);
		const Count     count = strtoul(serd_node_get_string(card), NULL, 10);
		if (check(ctx, values != count)) {
			st = cardinality_error(
				ctx, statement, c, prop, values, "!=", count);
		}
	}

	// Check minimum cardinality
	const SerdStatement* l = serd_model_get_statement(
	    ctx->model, restriction, ctx->uris.owl_minCardinality, NULL, NULL);
	if (l) {
		const SerdNode* card  = serd_statement_get_object(l);
		const Count     count = strtoul(serd_node_get_string(card), NULL, 10);
		if (check(ctx, values < count)) {
			st = cardinality_error(ctx, statement, l, prop, values, "<", count);
		}
	}

	// Check maximum cardinality
	const SerdStatement* u = serd_model_get_statement(
	    ctx->model, restriction, ctx->uris.owl_maxCardinality, NULL, NULL);
	if (u) {
		const SerdNode* card  = serd_statement_get_object(u);
		const Count     count = strtoul(serd_node_get_string(card), NULL, 10);
		if (check(ctx, values > count)) {
			st = cardinality_error(ctx, statement, u, prop, values, ">", count);
		}
	}

	// Check someValuesFrom
	const SerdStatement* s = serd_model_get_statement(
	    ctx->model, restriction, ctx->uris.owl_someValuesFrom, 0, 0);
	if (s) {
		const SerdNode* some = serd_statement_get_object(s);

		ctx->quiet = true;
		SerdRange* v = serd_model_range(ctx->model, instance, prop, NULL, NULL);
		bool       found = false;
		SERD_FOREACH (i, v) {
			const SerdNode* value = serd_statement_get_object(i);
			if (check_type(ctx, statement, value, some)) {
				found = true;
				break;
			}
		}
		ctx->quiet = false;

		if (check(ctx, !found)) {
			st = VERRORF(ctx,
			             statement,
			             "%s has no <%s> values of type <%s>\n",
			             serd_node_get_string(instance),
			             serd_node_get_string(prop),
			             serd_node_get_string(some));
			VNOTE(ctx, s, "Restriction here\n");
		}
		serd_range_free(v);
	}

	// Check allValuesFrom
	const SerdStatement* a = serd_model_get_statement(
	    ctx->model, restriction, ctx->uris.owl_allValuesFrom, 0, 0);
	if (a) {
		++ctx->n_restrictions;
		const SerdNode* all = serd_statement_get_object(a);

		SerdRange* v = serd_model_range(ctx->model, instance, prop, NULL, NULL);
		SERD_FOREACH (i, v) {
			const SerdNode* value = serd_statement_get_object(i);
			if (!check_type(ctx, statement, value, all)) {
				st = VERRORF(ctx,
				             i,
				             "<%s> value not of type <%s>\n",
				             serd_node_get_string(prop),
				             serd_node_get_string(all));
				VNOTE(ctx, a, "Restriction here\n");
				break;
			}
		}
		serd_range_free(v);
	}

	return st;
}

static int
check_class_restriction(ValidationContext*   ctx,
                        const SerdNode*      restriction,
                        const SerdStatement* statement,
                        const SerdNode*      instance)
{
	const SerdNode* prop = serd_model_get(
	    ctx->model, restriction, ctx->uris.owl_onProperty, NULL, NULL);

	return prop ? check_property_restriction(
	                      ctx, restriction, prop, statement, instance)
	            : 0;
}

static void
init_uris(URIs* uris)
{
#define URI(prefix, suffix)                                                    \
	uris->prefix##_##suffix = serd_new_uri(NS_##prefix #suffix)

	URI(owl, Class);
	URI(owl, DatatypeProperty);
	URI(owl, FunctionalProperty);
	URI(owl, InverseFunctionalProperty);
	URI(owl, ObjectProperty);
	URI(owl, Restriction);
	URI(owl, Thing);
	URI(owl, allValuesFrom);
	URI(owl, cardinality);
	URI(owl, equivalentClass);
	URI(owl, maxCardinality);
	URI(owl, minCardinality);
	URI(owl, onDatatype);
	URI(owl, onProperty);
	URI(owl, someValuesFrom);
	URI(owl, withRestrictions);
	URI(rdf, PlainLiteral);
	URI(rdf, Property);
	URI(rdf, first);
	URI(rdf, rest);
	URI(rdf, type);
	URI(rdfs, Class);
	URI(rdfs, Datatype);
	URI(rdfs, Literal);
	URI(rdfs, Resource);
	URI(rdfs, domain);
	URI(rdfs, label);
	URI(rdfs, range);
	URI(rdfs, subClassOf);
	URI(xsd, anyURI);
	URI(xsd, float);
	URI(xsd, decimal);
	URI(xsd, double);
	URI(xsd, maxExclusive);
	URI(xsd, maxInclusive);
	URI(xsd, minExclusive);
	URI(xsd, minInclusive);
	URI(xsd, pattern);
	URI(xsd, string);
}

SerdStatus
serd_validate(const SerdModel* model)
{
	ValidationContext ctx;
	memset(&ctx, 0, sizeof(ValidationContext));
	init_uris(&ctx.uris);

	ctx.model          = model;
	ctx.n_errors       = 0;
	ctx.n_restrictions = 0;

#ifndef HAVE_PCRE
	fprintf(stderr, "warning: Built without PCRE, datatypes not checked.\n");
#endif

	int        st = 0;
	SerdRange* i  = serd_model_all(ctx.model);
	SERD_FOREACH (statement, i) {
		st = check_statement(&ctx, statement) || st;
	}
	serd_range_free(i);

	printf("Found %d errors (checked %d restrictions)\n",
	       ctx.n_errors,
	       ctx.n_restrictions);

	for (SerdNode** n = (SerdNode**)&ctx.uris; *n; ++n) {
		serd_node_free(*n);
	}

	return !st && ctx.n_errors == 0 ? SERD_SUCCESS : SERD_ERR_INVALID;
}
