/*
  Copyright 2012-2020 David Robillard <d@drobilla.net>

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

#include "model.h"
#include "node.h"
#include "world.h"

#include "exess/exess.h"
#include "rerex/rerex.h"
#include "serd/serd.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NS_owl "http://www.w3.org/2002/07/owl#"
#define NS_rdf "http://www.w3.org/1999/02/22-rdf-syntax-ns#"
#define NS_rdfs "http://www.w3.org/2000/01/rdf-schema#"
#define NS_xsd "http://www.w3.org/2001/XMLSchema#"

#define SERD_FOREACH(name, range)                                      \
  for (const SerdStatement*(name) = NULL;                              \
       !serd_range_empty(range) && ((name) = serd_range_front(range)); \
       serd_range_next(range))

#define SERD_FOREACH_NODE(field, name, range)                                  \
  for (const SerdNode*(name) = NULL;                                           \
       !serd_range_empty(range) &&                                             \
       ((name) = serd_statement_node(                                          \
          (const SerdStatement* SERD_NONNULL)serd_range_front(range), field)); \
       serd_range_next(range))

#define NODE_FMT "%s%s%s"

#define NODE_ARGS(node) \
  open_quote(node), ((node) ? serd_node_string(node) : ""), close_quote(node)

#define N_CHECKS 31

static const char* const check_names[N_CHECKS] = {
  "allValuesFrom",             //
  "anyUri",                    //
  "cardinalityEqual",          //
  "cardinalityMax",            //
  "cardinalityMin",            //
  "classCycle",                //
  "classLabel",                //
  "datatypeProperty",          //
  "datatypeType",              //
  "deprecatedClass",           //
  "deprecatedProperty",        //
  "functionalProperty",        //
  "instanceLiteral",           //
  "instanceType",              //
  "inverseFunctionalProperty", //
  "literalInstance",           //
  "literalMaxExclusive",       //
  "literalMaxInclusive",       //
  "literalMinExclusive",       //
  "literalMinInclusive",       //
  "literalPattern",            //
  "literalRestriction",        //
  "literalValue",              //
  "objectProperty",            //
  "plainLiteralDatatype",      //
  "predicateType",             //
  "propertyCycle",             //
  "propertyDomain",            //
  "propertyLabel",             //
  "propertyRange",             //
  "someValuesFrom",            //
};

typedef unsigned long Count;

typedef struct {
  const SerdNode* owl_Class;
  const SerdNode* owl_DatatypeProperty;
  const SerdNode* owl_deprecated;
  const SerdNode* owl_DeprecatedClass;
  const SerdNode* owl_DeprecatedProperty;
  const SerdNode* owl_FunctionalProperty;
  const SerdNode* owl_InverseFunctionalProperty;
  const SerdNode* owl_ObjectProperty;
  const SerdNode* owl_Restriction;
  const SerdNode* owl_Thing;
  const SerdNode* owl_allValuesFrom;
  const SerdNode* owl_cardinality;
  const SerdNode* owl_equivalentClass;
  const SerdNode* owl_maxCardinality;
  const SerdNode* owl_minCardinality;
  const SerdNode* owl_onDatatype;
  const SerdNode* owl_onProperty;
  const SerdNode* owl_someValuesFrom;
  const SerdNode* owl_unionOf;
  const SerdNode* owl_withRestrictions;
  const SerdNode* rdf_PlainLiteral;
  const SerdNode* rdf_Property;
  const SerdNode* rdf_XMLLiteral;
  const SerdNode* rdf_first;
  const SerdNode* rdf_rest;
  const SerdNode* rdf_type;
  const SerdNode* rdfs_Class;
  const SerdNode* rdfs_Datatype;
  const SerdNode* rdfs_Literal;
  const SerdNode* rdfs_Resource;
  const SerdNode* rdfs_domain;
  const SerdNode* rdfs_label;
  const SerdNode* rdfs_range;
  const SerdNode* rdfs_subClassOf;
  const SerdNode* rdfs_subPropertyOf;
  const SerdNode* xsd_anyURI;
  const SerdNode* xsd_maxExclusive;
  const SerdNode* xsd_maxInclusive;
  const SerdNode* xsd_minExclusive;
  const SerdNode* xsd_minInclusive;
  const SerdNode* xsd_pattern;
  const SerdNode* sentinel;
} URIs;

struct SerdValidatorImpl {
  const SerdWorld* world;
  const SerdModel* model;
  const SerdNode*  graph;
  const SerdNode*  true_node;
  URIs             uris;
  uint32_t         checks;
  unsigned         n_errors;
  unsigned         n_checks;
  bool             suppressed;
};

typedef struct {
  const char* name;
} Check;

static SerdStatus
check_class_restriction(SerdValidator*       ctx,
                        const SerdNode*      root_klass,
                        const SerdNode*      restriction,
                        const SerdStatement* statement,
                        const SerdNode*      instance);

static const SerdNode*
string_node(const SerdValidator* const ctx, const SerdNode* const node)
{
  const SerdNode* const label =
    serd_model_get(ctx->model, node, ctx->uris.rdfs_label, NULL, NULL);

  return label ? label : node;
}

static const char*
open_quote(const SerdNode* const node)
{
  return !node                                    ? ""
         : (serd_node_type(node) == SERD_LITERAL) ? "\""
         : (serd_node_type(node) == SERD_URI)     ? "<"
         : (serd_node_type(node) == SERD_BLANK)   ? "_:"
                                                  : "";
}

static const char*
close_quote(const SerdNode* const node)
{
  return !node                                    ? ""
         : (serd_node_type(node) == SERD_LITERAL) ? "\""
         : (serd_node_type(node) == SERD_URI)     ? ">"
                                                  : "";
}

SERD_LOG_FUNC(5, 0)
static void
vreportf(SerdValidator* const       ctx,
         const SerdValidatorCheck   check,
         const SerdLogLevel         level,
         const SerdStatement* const statement,
         const char* const          fmt,
         va_list                    args)
{
  const char* file              = NULL;
  char        line[24]          = {0};
  char        col[24]           = {0};
  char        status_string[12] = {0};

  snprintf(status_string, sizeof(status_string), "%d", SERD_ERR_INVALID);

  const SerdCursor* const cursor =
    statement ? serd_statement_cursor(statement) : NULL;

  if (cursor) {
    file = serd_node_string(serd_cursor_name(cursor));

    snprintf(line, sizeof(line), "%u", serd_cursor_line(cursor));
    snprintf(col, sizeof(col), "%u", serd_cursor_column(cursor));
  }

  const SerdLogField fields[] = {{"SERD_STATUS", status_string},
                                 {"SERD_CHECK", check_names[check]},
                                 {"SERD_FILE", file},
                                 {"SERD_LINE", line},
                                 {"SERD_COL", col}};

  serd_world_vlogf(ctx->model->world, level, cursor ? 5 : 2, fields, fmt, args);
}

SERD_LOG_FUNC(5, 6)
static SerdStatus
report_check(SerdValidator* const       ctx,
             const SerdStatement* const statement,
             const SerdValidatorCheck   check,
             const bool                 condition,
             const char* const          fmt,
             ...)
{
  if (!ctx->suppressed && (ctx->checks & (1u << check))) {
    ctx->n_checks += 1;
    ctx->n_errors += condition ? 0 : 1;

    if (!condition) {
      va_list args;
      va_start(args, fmt);
      vreportf(ctx, check, SERD_LOG_LEVEL_ERROR, statement, fmt, args);
      va_end(args);
    }
  }

  return condition ? SERD_SUCCESS : SERD_ERR_INVALID;
}

SERD_LOG_FUNC(4, 5)
static void
log_note(SerdValidator* const       ctx,
         const SerdStatement* const statement,
         const SerdValidatorCheck   check,
         const char* const          fmt,
         ...)
{
  if (!ctx->suppressed) {
    va_list args;
    va_start(args, fmt);
    vreportf(ctx, check, SERD_LOG_LEVEL_NOTICE, statement, fmt, args);
    va_end(args);
  }
}

/*
  Return true iff `child` is a descendant of `parent` by `pred` arcs.

  That is, returns true if there is a path from `child` to `parent` by
  following `pred` arcs starting from child.
*/
static bool
is_descendant(SerdValidator* const  ctx,
              const SerdNode* const child,
              const SerdNode* const parent,
              const SerdNode* const pred)
{
  if (serd_node_equals(child, parent) ||
      serd_model_ask(
        ctx->model, child, ctx->uris.owl_equivalentClass, parent, NULL)) {
    return true;
  }

  SerdRange* const i = serd_model_range(ctx->model, child, pred, NULL, NULL);
  SERD_FOREACH_NODE (SERD_OBJECT, o, i) {
    if (!serd_node_equals(child, o) && is_descendant(ctx, o, parent, pred)) {
      serd_range_free(i);
      return true;
    }
  }

  serd_range_free(i);
  return false;
}

// Return true iff `klass` is a subclass of `super`
static bool
is_subclass(SerdValidator* const  ctx,
            const SerdNode* const klass,
            const SerdNode* const super)
{
  return serd_node_equals(klass, super) ||
         is_descendant(ctx, klass, super, ctx->uris.rdfs_subClassOf);
}

// Return true iff `datatype` is a subdatatype of `super`
static bool
is_subdatatype(SerdValidator* const  ctx,
               const SerdNode* const datatype,
               const SerdNode* const super)
{
  return serd_node_equals(datatype, super) ||
         is_descendant(ctx, datatype, super, ctx->uris.owl_onDatatype);
}

static void
update_status(SerdStatus* const old, const SerdStatus next)
{
  *old = next > *old ? next : *old;
}

static SerdStatus
merge_status(const SerdStatus a, const SerdStatus b)
{
  return a > b ? a : b;
}

static int
bound_cmp(SerdValidator* const       ctx,
          const SerdStatement* const literal_statement,
          const SerdNode* const      literal,
          const SerdNode* const      type,
          const SerdStatement* const bound_statement,
          const SerdNode* const      bound)
{
  const ExessDatatype value_type =
    exess_datatype_from_uri(serd_node_string(type));

  if (value_type != EXESS_NOTHING) {
    const ExessVariant bound_value = serd_node_get_value_as(bound, value_type);

    if (bound_value.datatype == EXESS_NOTHING) {
      return !!serd_world_logf_internal(
        ctx->model->world,
        SERD_ERR_INVALID,
        SERD_LOG_LEVEL_ERROR,
        serd_statement_cursor(bound_statement),
        "Failed to parse bound literal \"%s\" (%s)",
        serd_node_string(bound),
        exess_strerror(bound_value.value.as_status));
    }

    const ExessVariant literal_value =
      serd_node_get_value_as(literal, value_type);

    if (literal_value.datatype == EXESS_NOTHING) {
      return !!serd_world_logf_internal(
        ctx->model->world,
        SERD_ERR_INVALID,
        SERD_LOG_LEVEL_ERROR,
        serd_statement_cursor(literal_statement),
        "Failed to parse literal \"%s\" (%s)",
        serd_node_string(literal),
        exess_strerror(literal_value.value.as_status));
    }

    return exess_compare(literal_value, bound_value);
  }

  return strcmp(serd_node_string(literal), serd_node_string(bound));
}

static bool
less(const int cmp)
{
  return cmp < 0;
}

static bool
less_equal(const int cmp)
{
  return cmp <= 0;
}

static bool
greater(const int cmp)
{
  return cmp > 0;
}

static bool
greater_equal(const int cmp)
{
  return cmp >= 0;
}

static SerdStatus
check_bound(SerdValidator* const       ctx,
            const SerdValidatorCheck   check,
            const SerdStatement* const statement,
            const SerdNode* const      literal,
            const SerdNode* const      type,
            const SerdNode* const      restriction,
            const SerdNode* const      bound_property,
            bool (*pred)(int),
            const char* const message)
{
  SerdIter* const b =
    serd_model_find(ctx->model, restriction, bound_property, 0, 0);
  if (!b) {
    return SERD_SUCCESS;
  }

  const SerdNode* const bound =
    serd_statement_object((const SerdStatement* SERD_NONNULL)serd_iter_get(b));

  const int cmp =
    bound_cmp(ctx, statement, literal, type, serd_iter_get(b), bound);

  serd_iter_free(b);

  return report_check(ctx,
                      statement,
                      check,
                      pred(cmp),
                      "Value \"%s\" %s \"%s\"",
                      serd_node_string(literal),
                      message,
                      serd_node_string(bound));
}

static RerexPattern*
parse_regex(const SerdWorld* const     world,
            const SerdStatement* const pattern_statement,
            const char* const          regex)
{
  const SerdCursor* const cursor =
    pattern_statement ? serd_statement_cursor(pattern_statement) : NULL;

  RerexPattern*     re  = NULL;
  size_t            end = 0;
  const RerexStatus st  = rerex_compile(regex, &end, &re);
  if (st) {
    serd_world_logf_internal(world,
                             SERD_ERR_INVALID,
                             SERD_LOG_LEVEL_ERROR,
                             cursor,
                             "Error in pattern \"%s\" at offset %" PRIuPTR
                             " (%s)",
                             regex,
                             end,
                             rerex_strerror(st));
    return NULL;
  }

  return re;
}

static bool
regex_match(SerdValidator* const       ctx,
            const SerdStatement* const pattern_statement,
            const char* const          regex,
            const char* const          str)
{
  RerexPattern* const re = parse_regex(ctx->world, pattern_statement, regex);
  if (!re) {
    return false;
  }

  RerexMatcher* const matcher = rerex_new_matcher(re);
  const bool          ret     = rerex_match(matcher, str);

  rerex_free_matcher(matcher);
  rerex_free_pattern(re);

  return ret;
}

static SerdStatus
check_literal_restriction(SerdValidator* const       ctx,
                          const SerdStatement* const statement,
                          const SerdNode* const      literal,
                          const SerdNode* const      type,
                          const SerdNode* const      restriction)
{
  SerdStatus st = SERD_SUCCESS;

  // Check xsd:pattern
  const SerdStatement* const pat_statement = serd_model_get_statement(
    ctx->model, restriction, ctx->uris.xsd_pattern, 0, 0);
  if (pat_statement) {
    const char* const     str      = serd_node_string(literal);
    const SerdNode* const pat_node = serd_statement_object(pat_statement);
    const char* const     pat      = serd_node_string(pat_node);

    st = merge_status(st,
                      report_check(ctx,
                                   statement,
                                   SERD_CHECK_LITERAL_PATTERN,
                                   regex_match(ctx, pat_statement, pat, str),
                                   "Value \"%s\" doesn't match pattern \"%s\"",
                                   serd_node_string(literal),
                                   pat));
  }

  // Check inclusive/exclusive min and max

  typedef bool (*BoundCmpPredicate)(int);

  typedef struct {
    SerdValidatorCheck check_id;
    const SerdNode*    restriction_property;
    BoundCmpPredicate  pred;
    const char* const  message;
  } BoundCheck;

  const BoundCheck bound_checks[] = {
    {SERD_CHECK_LITERAL_MIN_INCLUSIVE,
     ctx->uris.xsd_minInclusive,
     greater_equal,
     "<"},
    {SERD_CHECK_LITERAL_MAX_INCLUSIVE,
     ctx->uris.xsd_maxInclusive,
     less_equal,
     ">"},
    {SERD_CHECK_LITERAL_MIN_EXCLUSIVE,
     ctx->uris.xsd_minExclusive,
     greater,
     "<="},
    {SERD_CHECK_LITERAL_MAX_EXCLUSIVE, ctx->uris.xsd_maxExclusive, less, ">="},
  };

  for (size_t i = 0; i < sizeof(bound_checks) / sizeof(BoundCheck); ++i) {
    st = merge_status(st,
                      check_bound(ctx,
                                  bound_checks[i].check_id,
                                  statement,
                                  literal,
                                  type,
                                  restriction,
                                  bound_checks[i].restriction_property,
                                  bound_checks[i].pred,
                                  bound_checks[i].message));
  }

  return st;
}

static bool
literal_is_valid(SerdValidator* const       ctx,
                 const SerdStatement* const statement,
                 const SerdNode* const      literal,
                 const SerdNode* const      type)
{
  if (!type) {
    return true;
  }

  // Check that datatype is defined
  const SerdNode* const node_datatype = serd_node_datatype(literal);
  if (node_datatype && report_check(ctx,
                                    statement,
                                    SERD_CHECK_DATATYPE_TYPE,
                                    serd_model_ask(ctx->model,
                                                   node_datatype,
                                                   ctx->uris.rdf_type,
                                                   ctx->uris.rdfs_Datatype,
                                                   NULL),
                                    "Undefined datatype <%s>",
                                    serd_node_string(node_datatype))) {
    return false;
  }

  const SerdNode* const type_string = string_node(ctx, type);

  const ExessDatatype value_type =
    node_datatype ? exess_datatype_from_uri(serd_node_string(node_datatype))
                  : EXESS_NOTHING;

  if (value_type != EXESS_NOTHING) {
    /* Check if the literal parses correctly by measuring the canonical string.
       This is better than trying to read a variant here, because it
       automatically supports some unbounded datatypes like xsd:decimal and
       xsd:base64Binary without needing to allocate space for the value. */

    const ExessResult r =
      exess_write_canonical(serd_node_string(literal), value_type, 0, NULL);

    if (report_check(ctx,
                     statement,
                     SERD_CHECK_LITERAL_VALUE,
                     r.status == EXESS_SUCCESS,
                     "Invalid xsd:%s literal \"%s\" (%s)",
                     serd_node_string(node_datatype) + sizeof(EXESS_XSD_URI) -
                       1,
                     serd_node_string(literal),
                     exess_strerror(r.status))) {
      return false;
    }
  }

  // Find restrictions list
  const SerdNode* head =
    serd_model_get(ctx->model, type, ctx->uris.owl_withRestrictions, 0, 0);

  // Walk list, checking each restriction
  while (head) {
    SerdIter* const i_first =
      serd_model_find(ctx->model, head, ctx->uris.rdf_first, 0, 0);

    if (!i_first) {
      break;
    }

    const SerdStatement* const s_first =
      (const SerdStatement* SERD_NONNULL)serd_iter_get(i_first);

    const SerdNode* const first = serd_statement_object(s_first);

    // Check this restriction
    if (check_literal_restriction(ctx, statement, literal, type, first)) {
      log_note(ctx,
               s_first,
               SERD_CHECK_LITERAL_RESTRICTION,
               "Restriction on datatype " NODE_FMT,
               NODE_ARGS(type_string));
      serd_iter_free(i_first);
      return false;
    }

    // Seek to next list node
    head = serd_model_get(ctx->model, head, ctx->uris.rdf_rest, 0, 0);
    serd_iter_free(i_first);
  }

  // Recurse up datatype hierarchy
  const SerdNode* const super =
    serd_model_get(ctx->model, type, ctx->uris.owl_onDatatype, 0, 0);

  // FIXME: check for cycles
  return super ? literal_is_valid(ctx, statement, literal, super) : true;
}

static bool
is_a(SerdValidator* const  ctx,
     const SerdNode* const node,
     const SerdNode* const type)
{
  if (serd_model_ask(ctx->model, node, ctx->uris.rdf_type, type, 0)) {
    return true; // Instance explicitly has this type
  }

  SerdRange* const node_types =
    serd_model_range(ctx->model, node, ctx->uris.rdf_type, NULL, NULL);

  SERD_FOREACH_NODE (SERD_OBJECT, node_type, node_types) {
    if (is_subclass(ctx, node_type, type)) {
      serd_range_free(node_types);
      return true; // Instance explicitly has a subtype of this type
    }
  }

  serd_range_free(node_types);
  return false;
}

static SerdStatus
check_instance_type(SerdValidator* const       ctx,
                    const SerdValidatorCheck   check,
                    const SerdNode* const      root_klass,
                    const SerdStatement* const statement,
                    const SerdNode* const      instance,
                    const SerdNode* const      klass)
{
  SerdStatus st = SERD_SUCCESS;

  if (serd_node_type(klass) == SERD_BLANK) {
    const SerdNode* const list =
      serd_model_get(ctx->model, klass, ctx->uris.owl_unionOf, NULL, NULL);

    for (const SerdNode* l = list; l;
         l = serd_model_get(ctx->model, l, ctx->uris.rdf_rest, NULL, NULL)) {
      const SerdNode* const element =
        serd_model_get(ctx->model, l, ctx->uris.rdf_first, NULL, NULL);
      if (element) {
        ctx->suppressed = true;

        st = check_instance_type(
          ctx, check, root_klass, statement, instance, element);

        ctx->suppressed = false;
        if (!st) {
          return SERD_SUCCESS;
        }
      }
    }

    if (list) {
      return report_check(ctx,
                          statement,
                          check,
                          false,
                          "Instance " NODE_FMT " is not any type in union",
                          NODE_ARGS(instance));
    }
  }

  // Any URI node is an xsd:anyURI
  if (serd_node_equals(klass, ctx->uris.xsd_anyURI) ||
      is_subdatatype(ctx, klass, ctx->uris.xsd_anyURI)) {
    return report_check(ctx,
                        statement,
                        SERD_CHECK_ANY_URI,
                        serd_node_type(instance) == SERD_URI,
                        "Node " NODE_FMT " isn't a URI",
                        NODE_ARGS(instance));
  }

  // An instance can not be a rdfs:Literal or a rdfs:Datatype
  if (report_check(ctx,
                   statement,
                   SERD_CHECK_INSTANCE_LITERAL,
                   !is_subclass(ctx, klass, ctx->uris.rdfs_Literal) &&
                     !is_a(ctx, klass, ctx->uris.rdfs_Datatype),
                   "Instance " NODE_FMT " isn't a literal",
                   NODE_ARGS(instance))) {
    return SERD_ERR_INVALID;
  }

  // Every instance is a rdfs:Resource and owl:Thing
  if (serd_node_equals(klass, ctx->uris.rdfs_Resource) ||
      serd_node_equals(klass, ctx->uris.owl_Thing)) {
    return SERD_SUCCESS;
  }

  // If the class is a restriction, check it
  if (serd_model_ask(ctx->model,
                     klass,
                     ctx->uris.rdf_type,
                     ctx->uris.owl_Restriction,
                     NULL)) {
    if ((st = check_class_restriction(
           ctx, root_klass, klass, statement, instance))) {
      return st;
    }
  }

#if 0
  // A class can not be a datatype (OWL DL, but general good practice)
  if ( //! serd_model_ask(node_equals(instance, ctx->uris.rdf.PlainLiteral) &&
    (ctx->checks & ((1u << SERD_CHECK_INSTANCE_LITERAL))) && //
    !serd_node_equals(instance, ctx->uris.rdf_XMLLiteral) &&
    !serd_node_equals(instance, ctx->uris.rdfs_Literal) &&
    !serd_node_equals(instance, ctx->uris.rdf_PlainLiteral) &&
    (serd_node_equals(klass, ctx->uris.rdfs_Class) ||
     serd_node_equals(klass, ctx->uris.owl_Class))) {
    if (report_check(ctx,
                     statement,
                     SERD_CHECK_INSTANCE_LITERAL, // FIXME
                     !is_a(ctx, instance, ctx->uris.rdfs_Datatype),
                     "Instance " NODE_FMT " is both a class and a datatype",
                     NODE_ARGS(instance))) {
      return SERD_ERR_INVALID;
    }
  }
#endif

  SerdRange* const superclasses =
    serd_model_range(ctx->model, klass, ctx->uris.rdfs_subClassOf, NULL, NULL);
  SERD_FOREACH_NODE (SERD_OBJECT, superclass, superclasses) {
    const SerdNode* const klass_string      = string_node(ctx, klass);
    const SerdNode* const superclass_string = string_node(ctx, superclass);
    if (!serd_node_equals(klass, superclass) &&
        !serd_node_equals(superclass, ctx->uris.rdfs_Class) &&
        !serd_node_equals(superclass, ctx->uris.owl_Class) &&
        check_instance_type(
          ctx, check, klass, statement, instance, superclass)) {
      if (serd_node_type(superclass) == SERD_URI ||
          serd_node_type(superclass) == SERD_CURIE) {
        log_note(ctx,
                 serd_range_front(superclasses),
                 check,
                 "A " NODE_FMT " is a " NODE_FMT,
                 NODE_ARGS(klass_string),
                 NODE_ARGS(superclass_string));
      }

      serd_range_free(superclasses);
      return SERD_ERR_INVALID;
    }
  }
  serd_range_free(superclasses);

  if (serd_model_ask(ctx->model, instance, ctx->uris.rdf_type, klass, NULL)) {
    return SERD_SUCCESS;
  }

  const SerdNode* const instance_string = string_node(ctx, instance);
  const SerdNode* const klass_string    = string_node(ctx, klass);

  if (!serd_model_ask(ctx->model, instance, NULL, NULL, NULL)) {
    /* Nothing about this node known in the model at all, assume it is some
       external resource we can't validate. */
    serd_world_logf_internal(ctx->model->world,
                             SERD_ERR_INVALID,
                             SERD_LOG_LEVEL_WARNING,
                             serd_statement_cursor(statement),
                             "Nothing known about " NODE_FMT
                             ", assuming it is a " NODE_FMT,
                             NODE_ARGS(instance_string),
                             NODE_ARGS(klass_string));

    // FIXME: test
    return SERD_FAILURE;
  }

  if (serd_node_type(instance) == SERD_BLANK) {
    /* Be permissive for blank nodes and don't require explicit type
       annotation, to avoid countless errors with things like lists. */
    // return SERD_FAILURE;
    return SERD_SUCCESS;
  }

  return report_check(ctx,
                      statement,
                      SERD_CHECK_INSTANCE_TYPE,
                      is_a(ctx, instance, klass),
                      "Instance " NODE_FMT " isn't a " NODE_FMT,
                      NODE_ARGS(instance_string),
                      NODE_ARGS(klass_string));
}

static SerdStatus
check_type(SerdValidator* const       ctx,
           const SerdValidatorCheck   check,
           const SerdStatement* const statement,
           const SerdNode* const      node,
           const SerdNode* const      type)
{
  const SerdNode* const type_string = string_node(ctx, type);

  // Everything is an rdfs:Resource
  if (serd_node_equals(type, ctx->uris.rdfs_Resource)) {
    return SERD_SUCCESS;
  }

  switch (serd_node_type(node)) {
  case SERD_LITERAL:
    // Every literal is an rdfs:Literal
    if (serd_node_equals(type, ctx->uris.rdfs_Literal)) {
      return SERD_SUCCESS;
    }

    // A plain literal can not have a datatype
    if (serd_node_equals(type, ctx->uris.rdf_PlainLiteral)) {
      if (report_check(ctx,
                       statement,
                       SERD_CHECK_PLAIN_LITERAL_DATATYPE,
                       !serd_node_datatype(node),
                       "Typed literal \"%s\" isn't a plain literal",
                       serd_node_string(node))) {
        return SERD_ERR_INVALID;
      }
    } else if (report_check(ctx,
                            statement,
                            SERD_CHECK_LITERAL_INSTANCE,
                            is_a(ctx, type, ctx->uris.rdfs_Datatype),
                            "Literal \"%s\" isn't an instance of " NODE_FMT,
                            serd_node_string(node),
                            NODE_ARGS(type_string))) {
      return SERD_ERR_INVALID;
    }

    return literal_is_valid(ctx, statement, node, type) ? SERD_SUCCESS
                                                        : SERD_ERR_INVALID;

  case SERD_URI:
    if (serd_node_equals(type, ctx->uris.xsd_anyURI)) {
      return SERD_SUCCESS;
    }
    break;

  case SERD_CURIE:
  case SERD_BLANK:
  case SERD_VARIABLE:
    break;
  }

  return check_instance_type(ctx, check, type, statement, node, type);
}

static Count
count_non_blanks(SerdRange* const i, const SerdField field)
{
  Count n = 0u;
  SERD_FOREACH (s, i) {
    const SerdNode* node = serd_statement_node(s, field);
    if (node && serd_node_type(node) != SERD_BLANK) {
      ++n;
    }
  }
  return n;
}

static SerdStatus
check_cardinality_restriction(SerdValidator* const       ctx,
                              const SerdNode* const      root_klass,
                              const SerdNode* const      restriction,
                              const SerdStatement* const statement,
                              const SerdNode* const      instance)
{
  const SerdNode* const prop = serd_model_get(
    ctx->model, restriction, ctx->uris.owl_onProperty, NULL, NULL);

  const SerdStatement* const equal_statement = serd_model_get_statement(
    ctx->model, restriction, ctx->uris.owl_cardinality, NULL, NULL);

  const SerdStatement* const min_statement = serd_model_get_statement(
    ctx->model, restriction, ctx->uris.owl_minCardinality, NULL, NULL);

  const SerdStatement* const max_statement = serd_model_get_statement(
    ctx->model, restriction, ctx->uris.owl_maxCardinality, NULL, NULL);

  if (!equal_statement && !min_statement && !max_statement) {
    return SERD_SUCCESS;
  }

  const SerdNode* const prop_string  = string_node(ctx, prop);
  const SerdNode* const klass_string = string_node(ctx, root_klass);

  SerdStatus  st = SERD_SUCCESS;
  const Count n_values =
    (Count)serd_model_count(ctx->model, instance, prop, NULL, NULL);

  // Check owl:cardinality
  if (equal_statement) {
    const SerdNode* card     = serd_statement_object(equal_statement);
    const Count     expected = strtoul(serd_node_string(card), NULL, 10);
    if ((st =
           report_check(ctx,
                        statement,
                        SERD_CHECK_CARDINALITY_EQUAL,
                        n_values == expected,
                        "Instance " NODE_FMT " has %lu " NODE_FMT " properties",
                        NODE_ARGS(instance),
                        n_values,
                        NODE_ARGS(prop_string)))) {
      log_note(ctx,
               equal_statement,
               SERD_CHECK_CARDINALITY_EQUAL,
               "A " NODE_FMT " must have exactly %lu",
               NODE_ARGS(klass_string),
               expected);
      return st;
    }
  }

  // Check owl:minCardinality
  if (min_statement) {
    const SerdNode* card  = serd_statement_object(min_statement);
    const Count     n_min = strtoul(serd_node_string(card), NULL, 10);
    if ((st =
           report_check(ctx,
                        statement,
                        SERD_CHECK_CARDINALITY_MIN,
                        n_values >= n_min,
                        "Instance " NODE_FMT " has %lu " NODE_FMT " properties",
                        NODE_ARGS(instance),
                        n_values,
                        NODE_ARGS(prop_string)))) {
      log_note(ctx,
               min_statement,
               SERD_CHECK_CARDINALITY_MIN,
               "A " NODE_FMT " must have at least %lu",
               NODE_ARGS(klass_string),
               n_min);
      return st;
    }
  }

  // Check owl:maxCardinality
  if (max_statement) {
    const SerdNode* const card  = serd_statement_object(max_statement);
    const Count           n_max = strtoul(serd_node_string(card), NULL, 10);
    if ((st =
           report_check(ctx,
                        statement,
                        SERD_CHECK_CARDINALITY_MAX,
                        n_values <= n_max,
                        "Instance " NODE_FMT " has %lu " NODE_FMT " properties",
                        NODE_ARGS(instance),
                        n_values,
                        NODE_ARGS(prop_string)))) {
      log_note(ctx,
               max_statement,
               SERD_CHECK_CARDINALITY_MAX,
               "A " NODE_FMT " must have at most %lu",
               NODE_ARGS(klass_string),
               n_max);
      return st;
    }
  }

  return st;
}

static SerdStatus
check_property_value_restriction(SerdValidator* const       ctx,
                                 const SerdNode* const      root_klass,
                                 const SerdNode* const      restriction,
                                 const SerdStatement* const statement,
                                 const SerdNode* const      instance)
{
  SerdStatus st = SERD_SUCCESS;

  const SerdNode* const prop = serd_model_get(
    ctx->model, restriction, ctx->uris.owl_onProperty, NULL, NULL);

  const SerdStatement* const all_statement = serd_model_get_statement(
    ctx->model, restriction, ctx->uris.owl_allValuesFrom, NULL, NULL);

  const SerdStatement* const some_statement = serd_model_get_statement(
    ctx->model, restriction, ctx->uris.owl_someValuesFrom, NULL, NULL);

  if (!all_statement && !some_statement) {
    return SERD_SUCCESS;
  }

  const SerdNode* const prop_string  = string_node(ctx, prop);
  const SerdNode* const klass_string = string_node(ctx, root_klass);

  SerdRange* const values =
    serd_model_range(ctx->model, instance, prop, NULL, NULL);

  if (all_statement) {
    const SerdNode* const type        = serd_statement_object(all_statement);
    const SerdNode* const type_string = string_node(ctx, type);
    SERD_FOREACH (v, values) {
      const SerdNode* const value  = serd_statement_object(v);
      const SerdStatus      all_st = report_check(
        ctx,
        v,
        SERD_CHECK_ALL_VALUES_FROM,
        !check_type(ctx, SERD_CHECK_ALL_VALUES_FROM, v, value, type),
        "Value isn't a " NODE_FMT,
        NODE_ARGS(type_string));

      if (all_st) {
        st = merge_status(st, all_st);
        log_note(ctx,
                 all_statement,
                 SERD_CHECK_ALL_VALUES_FROM,
                 "Required for any " NODE_FMT " of a " NODE_FMT,
                 NODE_ARGS(prop_string),
                 NODE_ARGS(klass_string));
      }
    }
  }

  if (some_statement) {
    const SerdNode* const type        = serd_statement_object(some_statement);
    const SerdNode* const type_string = string_node(ctx, type);

    // Search for some value with the required type
    bool found = false;
    {
      ctx->suppressed = true;
      SERD_FOREACH_NODE (SERD_OBJECT, value, values) {
        if (!check_type(
              ctx, SERD_CHECK_SOME_VALUES_FROM, statement, value, type)) {
          found = true;
          break;
        }
      }
      ctx->suppressed = false;
    }

    const SerdStatus some_st =
      report_check(ctx,
                   statement,
                   SERD_CHECK_SOME_VALUES_FROM,
                   found,
                   NODE_FMT " has no " NODE_FMT " that is a " NODE_FMT,
                   NODE_ARGS(instance),
                   NODE_ARGS(prop_string),
                   NODE_ARGS(type_string));

    if (some_st) {
      log_note(ctx,
               some_statement,
               SERD_CHECK_SOME_VALUES_FROM,
               "An instance of " NODE_FMT " must have at least 1",
               NODE_ARGS(klass_string));
    }

    st = merge_status(st, some_st);
  }

  serd_range_free(values);

  return st;
}

static SerdStatus
check_class_restriction(SerdValidator* const       ctx,
                        const SerdNode* const      root_klass,
                        const SerdNode* const      restriction,
                        const SerdStatement* const statement,
                        const SerdNode* const      instance)
{
  SerdStatus st = SERD_SUCCESS;

  st = merge_status(st,
                    check_cardinality_restriction(
                      ctx, root_klass, restriction, statement, instance));

  st = merge_status(st,
                    check_property_value_restriction(
                      ctx, root_klass, restriction, statement, instance));

  return st;
}

/* Top-Level Checks */

static SerdStatus
check_class_label(SerdValidator* const ctx)
{
  const SerdModel* const model = ctx->model;
  const URIs* const      uris  = &ctx->uris;
  SerdStatus             st    = SERD_SUCCESS;

  // For each rdfs:Class
  SerdRange* const klasses =
    serd_model_range(model, NULL, uris->rdf_type, uris->rdfs_Class, ctx->graph);
  SERD_FOREACH (k, klasses) {
    const SerdNode* const klass = serd_statement_subject(k);

    // Check that it has an rdfs:label in the same graph
    st = merge_status(
      st,
      report_check(
        ctx,
        k,
        SERD_CHECK_CLASS_LABEL,
        serd_model_ask(ctx->model, klass, uris->rdfs_label, 0, ctx->graph),
        "Class <%s> has no label",
        serd_node_string(klass)));
  }
  serd_range_free(klasses);

  return st;
}

static SerdStatus
check_datatype_property(SerdValidator* const ctx)
{
  const SerdModel* const model = ctx->model;
  const URIs*            uris  = &ctx->uris;
  SerdStatus             st    = SERD_SUCCESS;

  // For each owl:DatatypeProperty
  SerdRange* const properties = serd_model_range(
    model, NULL, uris->rdf_type, uris->owl_DatatypeProperty, NULL);
  SERD_FOREACH (p, properties) {
    const SerdNode* const prop        = serd_statement_subject(p);
    const SerdNode* const prop_string = string_node(ctx, prop);

    // For each statement of this property in the target graph
    SerdRange* const statements =
      serd_model_range(model, NULL, prop, NULL, ctx->graph);
    SERD_FOREACH (s, statements) {
      const SerdNode* const object = serd_statement_object(s);

      // Check that the object is a literal
      if ((st = report_check(ctx,
                             s,
                             SERD_CHECK_DATATYPE_PROPERTY,
                             serd_node_type(object) == SERD_LITERAL,
                             NODE_FMT " isn't a literal",
                             NODE_ARGS(serd_statement_object(s))))) {
        log_note(ctx,
                 p,
                 SERD_CHECK_DATATYPE_PROPERTY,
                 "A " NODE_FMT " must be a literal",
                 NODE_ARGS(prop_string));
      }
    }
    serd_range_free(statements);
  }
  serd_range_free(properties);

  return st;
}

static SerdStatus
check_deprecated(SerdValidator* const ctx)
{
  const SerdModel* const model = ctx->model;
  SerdStatus             st    = SERD_SUCCESS;

  // For each deprecated thing
  SerdRange* const things = serd_model_range(
    model, NULL, ctx->uris.owl_deprecated, ctx->true_node, NULL);
  SERD_FOREACH (t, things) {
    const SerdNode* const thing        = serd_statement_subject(t);
    const SerdNode* const thing_string = string_node(ctx, thing);

    if (is_a(ctx, thing, ctx->uris.rdf_Property)) {
      // For each statement of this property in the target graph
      SerdRange* const statements =
        serd_model_range(model, NULL, thing, NULL, ctx->graph);
      SERD_FOREACH (s, statements) {
        st = report_check(ctx,
                          s,
                          SERD_CHECK_DEPRECATED_PROPERTY,
                          false,
                          "Use of deprecated property");
        log_note(ctx,
                 t,
                 SERD_CHECK_DEPRECATED_PROPERTY,
                 "Property " NODE_FMT " is deprecated",
                 NODE_ARGS(thing_string));
      }
      serd_range_free(statements);

    } else if (is_a(ctx, thing, ctx->uris.rdfs_Class)) {
      // For each explicit instance of this class in the target graph
      SerdRange* const statements =
        serd_model_range(model, NULL, ctx->uris.rdf_type, thing, ctx->graph);
      SERD_FOREACH (s, statements) {
        st = report_check(ctx,
                          s,
                          SERD_CHECK_DEPRECATED_CLASS,
                          false,
                          "Instance of deprecated class");
        log_note(ctx,
                 t,
                 SERD_CHECK_DEPRECATED_CLASS,
                 "Class " NODE_FMT " is deprecated",
                 NODE_ARGS(thing_string));
      }
      serd_range_free(statements);
    }
  }
  serd_range_free(things);

  return st;
}

static SerdStatus
check_functional_property(SerdValidator* const ctx)
{
  const SerdModel* const model = ctx->model;
  const URIs*            uris  = &ctx->uris;
  SerdStatus             st    = SERD_SUCCESS;

  // For each owl:FunctionalProperty
  SerdRange* const properties = serd_model_range(
    model, NULL, uris->rdf_type, uris->owl_FunctionalProperty, NULL);
  SERD_FOREACH (p, properties) {
    const SerdNode* const prop        = serd_statement_subject(p);
    const SerdNode* const prop_string = string_node(ctx, prop);

    const SerdNode* last_subj = NULL;

    // For each instance with this property in the target graph
    SerdRange* const statements =
      serd_model_range(model, NULL, prop, NULL, ctx->graph);
    SERD_FOREACH (s, statements) {
      const SerdNode* const subj = serd_statement_subject(s);
      if (serd_node_equals(subj, last_subj)) {
        continue;
      }

      // Count the number of values on this instance
      SerdRange* const o =
        serd_model_range(ctx->model, subj, prop, NULL, ctx->graph);
      const Count n = count_non_blanks(o, SERD_OBJECT);

      serd_range_free(o);
      if (report_check(ctx,
                       s,
                       SERD_CHECK_FUNCTIONAL_PROPERTY,
                       n <= 1,
                       "Instance has %lu " NODE_FMT " properties",
                       n,
                       NODE_ARGS(prop_string))) {
        st = SERD_ERR_INVALID;
        log_note(ctx,
                 p,
                 SERD_CHECK_FUNCTIONAL_PROPERTY,
                 "An instance may have at most 1");
      }

      last_subj = subj;
    }
    serd_range_free(statements);
  }
  serd_range_free(properties);

  return st;
}

// FIXME: name
static SerdStatus
check_instance(SerdValidator* const ctx)
{
  const SerdModel* const model = ctx->model;
  const URIs*            uris  = &ctx->uris;
  SerdStatus             st    = SERD_SUCCESS;

  // For each rdf:type property in the target graph
  SerdRange* const types =
    serd_model_range(model, NULL, uris->rdf_type, NULL, ctx->graph);
  SERD_FOREACH (t, types) {
    const SerdNode* const instance    = serd_statement_subject(t);
    const SerdNode* const type        = serd_statement_object(t);
    const SerdNode* const type_string = string_node(ctx, type);

    if ((st = check_instance_type(
           ctx, SERD_CHECK_INSTANCE_TYPE, type, t, instance, type))) {
      log_note(ctx,
               t,
               SERD_CHECK_INSTANCE_TYPE,
               "Instance is a " NODE_FMT,
               NODE_ARGS(type_string));
      break;
    }
  }
  serd_range_free(types);

  return st;
}

static SerdStatus
check_inverse_functional_property(SerdValidator* const ctx)
{
  const SerdModel* const model = ctx->model;
  const URIs*            uris  = &ctx->uris;
  SerdStatus             st    = SERD_SUCCESS;

  // For each owl:InverseFunctionalProperty
  SerdRange* const properties = serd_model_range(
    model, NULL, uris->rdf_type, uris->owl_InverseFunctionalProperty, NULL);
  SERD_FOREACH (p, properties) {
    const SerdNode* const prop        = serd_statement_subject(p);
    const SerdNode* const prop_string = string_node(ctx, prop);

    const SerdNode* last_obj = NULL;

    // For each value of this property in the target graph
    SerdRange* const statements =
      serd_model_range(model, NULL, prop, NULL, ctx->graph);
    SERD_FOREACH (statement, statements) {
      const SerdNode* const obj        = serd_statement_object(statement);
      const SerdNode* const obj_string = string_node(ctx, obj);
      if (serd_node_equals(obj, last_obj)) {
        continue;
      }

      // Count the number of subjects with this value in the target graph
      SerdRange*  s = serd_model_range(ctx->model, NULL, prop, obj, ctx->graph);
      const Count n = count_non_blanks(s, SERD_SUBJECT);

      if (n > 1) {
        // Get the range again so we can print a note for every value
        serd_range_free(s);
        s = serd_model_range(ctx->model, NULL, prop, obj, ctx->graph);

        SERD_FOREACH (value_statement, s) {
          const SerdNode* const subj = serd_statement_subject(value_statement);
          const SerdNode* const subj_string = string_node(ctx, subj);

          report_check(ctx,
                       value_statement,
                       SERD_CHECK_INVERSE_FUNCTIONAL_PROPERTY,
                       false,
                       "Instance " NODE_FMT " shares the " NODE_FMT
                       " " NODE_FMT,
                       NODE_ARGS(subj_string),
                       NODE_ARGS(prop_string),
                       NODE_ARGS(obj_string));
        }

        log_note(ctx,
                 p,
                 SERD_CHECK_INVERSE_FUNCTIONAL_PROPERTY,
                 "At most 1 instance may have a given " NODE_FMT,
                 NODE_ARGS(prop_string));
      }

      serd_range_free(s);
      last_obj = obj;
    }
    serd_range_free(statements);
  }
  serd_range_free(properties);

  return st;
}

static SerdStatus
check_object_property(SerdValidator* const ctx)
{
  const SerdModel* const model = ctx->model;
  SerdStatus             st    = SERD_SUCCESS;

  // For each owl:ObjectProperty
  SerdRange* const properties = serd_model_range(
    model, NULL, ctx->uris.rdf_type, ctx->uris.owl_ObjectProperty, NULL);
  SERD_FOREACH_NODE (SERD_SUBJECT, prop, properties) {
    const SerdNode* const prop_string = string_node(ctx, prop);

    // For each statement of this property in the target graph
    SerdRange* const statements =
      serd_model_range(model, NULL, prop, NULL, ctx->graph);
    SERD_FOREACH (s, statements) {
      if (report_check(ctx,
                       s,
                       SERD_CHECK_OBJECT_PROPERTY,
                       serd_node_type(serd_statement_object(s)) != SERD_LITERAL,
                       "Object property has literal value")) {
        st = SERD_ERR_INVALID;
        log_note(ctx,
                 serd_range_front(properties),
                 SERD_CHECK_OBJECT_PROPERTY,
                 "A " NODE_FMT " must be an instance",
                 NODE_ARGS(prop_string));
      }
    }
    serd_range_free(statements);
  }
  serd_range_free(properties);

  return st;
}

static SerdStatus
check_property_domain(SerdValidator* const ctx)
{
  const SerdModel* const model = ctx->model;
  SerdStatus             st    = SERD_SUCCESS;

  // For each property with an rdfs:domain
  SerdRange* const properties =
    serd_model_range(model, NULL, ctx->uris.rdfs_domain, NULL, NULL);
  SERD_FOREACH (p, properties) {
    const SerdNode* const prop          = serd_statement_subject(p);
    const SerdNode* const prop_string   = string_node(ctx, prop);
    const SerdNode* const domain        = serd_statement_object(p);
    const SerdNode* const domain_string = string_node(ctx, domain);

    // For each statement of this property in the target graph
    SerdRange* const statements =
      serd_model_range(model, NULL, prop, NULL, ctx->graph);
    SERD_FOREACH (statement, statements) {
      const SerdNode* const subj = serd_statement_subject(statement);

      // Check that the subject is in the domain
      if (check_instance_type(
            ctx, SERD_CHECK_PROPERTY_DOMAIN, domain, statement, subj, domain)) {
        log_note(ctx,
                 p,
                 SERD_CHECK_PROPERTY_DOMAIN,
                 "An instance with a " NODE_FMT " must be a " NODE_FMT,
                 NODE_ARGS(prop_string),
                 NODE_ARGS(domain_string));
      }
    }
    serd_range_free(statements);
  }
  serd_range_free(properties);

  return st;
}

static SerdStatus
check_property_label(SerdValidator* const ctx)
{
  const SerdModel* const model = ctx->model;
  SerdStatus             st    = SERD_SUCCESS;

  // For each ?property a rdf:Property in the target graph
  SerdRange* const properties = serd_model_range(
    model, NULL, ctx->uris.rdf_type, ctx->uris.rdf_Property, ctx->graph);
  SERD_FOREACH (p, properties) {
    const SerdNode* const property = serd_statement_subject(p);

    update_status(
      &st,
      report_check(ctx,
                   p,
                   SERD_CHECK_PROPERTY_LABEL,
                   serd_model_ask(
                     ctx->model, property, ctx->uris.rdfs_label, 0, ctx->graph),
                   "Property <%s> has no label",
                   serd_node_string(property)));
  }
  serd_range_free(properties);

  return st;
}

static SerdStatus
check_property_range(SerdValidator* const ctx)
{
  const SerdModel* const model = ctx->model;
  SerdStatus             st    = SERD_SUCCESS;

  // For each property with an rdfs:range
  SerdRange* const properties =
    serd_model_range(model, NULL, ctx->uris.rdfs_range, NULL, NULL);
  SERD_FOREACH (p, properties) {
    const SerdNode* const prop        = serd_statement_subject(p);
    const SerdNode* const klass       = serd_statement_object(p);
    const SerdNode* const prop_string = string_node(ctx, prop);

    // For each statement of this property in the target graph
    SerdRange* const statements =
      serd_model_range(model, NULL, prop, NULL, ctx->graph);
    SERD_FOREACH (statement, statements) {
      const SerdNode* const obj = serd_statement_object(statement);

      // Check that the object is in the range
      if (check_type(ctx, SERD_CHECK_PROPERTY_RANGE, statement, obj, klass)) {
        log_note(ctx,
                 p,
                 SERD_CHECK_PROPERTY_RANGE,
                 "Required for any " NODE_FMT " value",
                 NODE_ARGS(prop_string));
      }
    }
    serd_range_free(statements);
  }
  serd_range_free(properties);

  return st;
}

static SerdStatus
check_predicate_type(SerdValidator* const ctx)
{
  // For each predicate
  SerdStatus      st        = SERD_SUCCESS;
  const SerdNode* last_pred = NULL;
  // FIXME: graph
  SerdRange* const all = serd_model_ordered(ctx->model, SERD_ORDER_POS);
  SERD_FOREACH (s, all) {
    const SerdNode* const pred = serd_statement_predicate(s);
    if (serd_node_equals(pred, last_pred)) {
      continue;
    }

    const bool defined = serd_model_ask(ctx->model, pred, NULL, NULL, NULL);

    st = merge_status(st,
                      report_check(ctx,
                                   s,
                                   SERD_CHECK_PREDICATE_TYPE,
                                   defined,
                                   "Undefined property <%s>",
                                   serd_node_string(pred)));

    if (defined) {
      st = merge_status(
        st,
        report_check(
          ctx,
          s,
          SERD_CHECK_PREDICATE_TYPE,
          serd_model_ask(ctx->model, pred, ctx->uris.rdf_type, NULL, NULL) &&
            is_a(ctx, pred, ctx->uris.rdf_Property),
          "<%s> isn't a property",
          serd_node_string(pred)));
    }

    last_pred = pred;
  }
  serd_range_free(all);

  return st;
}

static SerdStatus
check_acyclic(SerdValidator* const     ctx,
              const SerdValidatorCheck check,
              const SerdNode* const    root,
              const SerdNode* const    node,
              const SerdNode* const    property,
              const char* const        fmt)
{
  SerdStatus st = SERD_SUCCESS;

  // FIXME: graph
  SerdRange* const links =
    serd_model_range(ctx->model, node, property, NULL, NULL);
  SERD_FOREACH (l, links) {
    const SerdNode* const object        = serd_statement_object(l);
    const SerdNode* const object_string = string_node(ctx, object);

    if ((st = report_check(ctx,
                           l,
                           check,
                           !serd_node_equals(object, root),
                           fmt,
                           NODE_ARGS(object_string)))) {
      break;
    }

    if ((st = check_acyclic(ctx, check, root, object, property, fmt))) {
      log_note(ctx, l, check, "Via " NODE_FMT, NODE_ARGS(object_string));
      break;
    }
  }
  serd_range_free(links);

  return st;
}

static SerdStatus
check_subclass_cycle(SerdValidator* const ctx)
{
  const SerdModel* const model = ctx->model;
  SerdStatus             st    = SERD_SUCCESS;

  // For each subclass
  SerdRange* const properties =
    serd_model_range(model, NULL, ctx->uris.rdfs_subClassOf, NULL, NULL);
  SERD_FOREACH_NODE (SERD_SUBJECT, root, properties) {
    st = merge_status(st,
                      check_acyclic(ctx,
                                    SERD_CHECK_CLASS_CYCLE,
                                    root,
                                    root,
                                    ctx->uris.rdfs_subClassOf,
                                    "Class " NODE_FMT
                                    " is a sub-class of itself"));
  }
  serd_range_free(properties);

  return st;
}

static SerdStatus
check_subproperty_cycle(SerdValidator* const ctx)
{
  const SerdModel* const model = ctx->model;
  SerdStatus             st    = SERD_SUCCESS;

  // For each subproperty relation
  SerdRange* const properties =
    serd_model_range(model, NULL, ctx->uris.rdfs_subPropertyOf, NULL, NULL);
  SERD_FOREACH_NODE (SERD_SUBJECT, root, properties) {
    st = merge_status(st,
                      check_acyclic(ctx,
                                    SERD_CHECK_PROPERTY_CYCLE,
                                    root,
                                    root,
                                    ctx->uris.rdfs_subPropertyOf,
                                    "Property " NODE_FMT
                                    " is a sub-property of itself"));
  }
  serd_range_free(properties);

  return st;
}

/* Statement Checks */

static SerdStatus
statement_check_valid_literal(SerdValidator* const       ctx,
                              const SerdStatement* const statement)
{
  const SerdNode* const object = serd_statement_object(statement);
  if (serd_node_type(object) != SERD_LITERAL) {
    return SERD_SUCCESS;
  }

  if (!literal_is_valid(ctx, statement, object, serd_node_datatype(object))) {
    /* log_note(ctx, l, check, "Via " NODE_FMT, NODE_ARGS(object_string)); */

    return SERD_ERR_INVALID;
  }

  return SERD_SUCCESS;
}

/* Entry Points */

SerdValidator*
serd_validator_new(SerdWorld* const world)
{
  SerdValidator* const validator =
    (SerdValidator*)calloc(1, sizeof(SerdValidator));

  if (!validator) {
    return NULL;
  }

  SerdNodes* const nodes = world->nodes;

  validator->world     = world;
  validator->true_node = serd_nodes_manage(nodes, serd_new_boolean(true));

#define URI(prefix, suffix)                              \
  validator->uris.prefix##_##suffix = serd_nodes_manage( \
    nodes, serd_new_uri(SERD_STATIC_STRING(NS_##prefix #suffix)))

  URI(owl, Class);
  URI(owl, DatatypeProperty);
  URI(owl, DeprecatedClass);
  URI(owl, DeprecatedProperty);
  URI(owl, FunctionalProperty);
  URI(owl, InverseFunctionalProperty);
  URI(owl, ObjectProperty);
  URI(owl, Restriction);
  URI(owl, Thing);
  URI(owl, allValuesFrom);
  URI(owl, cardinality);
  URI(owl, deprecated);
  URI(owl, equivalentClass);
  URI(owl, maxCardinality);
  URI(owl, minCardinality);
  URI(owl, onDatatype);
  URI(owl, onProperty);
  URI(owl, someValuesFrom);
  URI(owl, unionOf);
  URI(owl, withRestrictions);
  URI(rdf, PlainLiteral);
  URI(rdf, Property);
  URI(rdf, XMLLiteral);
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
  URI(rdfs, subPropertyOf);
  URI(xsd, anyURI);
  URI(xsd, maxExclusive);
  URI(xsd, maxInclusive);
  URI(xsd, minExclusive);
  URI(xsd, minInclusive);
  URI(xsd, pattern);

#undef URI

  return validator;
}

void
serd_validator_free(SerdValidator* const validator)
{
  free(validator);
}

SerdStatus
serd_validator_enable_checks(SerdValidator* const validator,
                             const char* const    regex)
{
  if (!strcmp(regex, "all")) {
    return serd_validator_enable_checks(validator, ".*");
  }

  RerexPattern* const re = parse_regex(validator->world, NULL, regex);
  if (!re) {
    return SERD_ERR_BAD_ARG;
  }

  bool          matched = false;
  RerexMatcher* matcher = rerex_new_matcher(re);

  for (unsigned i = 0; i < N_CHECKS; ++i) {
    if (rerex_match(matcher, check_names[i])) {
      validator->checks |= (1u << i);
      matched = true;
    }
  }

  rerex_free_matcher(matcher);
  rerex_free_pattern(re);

  return matched ? SERD_SUCCESS : SERD_FAILURE;
}

SerdStatus
serd_validator_disable_checks(SerdValidator* const validator,
                              const char* const    regex)
{
  RerexPattern* const re = parse_regex(validator->world, NULL, regex);
  if (!re) {
    return SERD_ERR_BAD_ARG;
  }

  bool          matched = false;
  RerexMatcher* matcher = rerex_new_matcher(re);

  for (unsigned i = 0; i < N_CHECKS; ++i) {
    if (rerex_match(matcher, check_names[i])) {
      validator->checks &= ~(1u << i);
      matched = true;
    }
  }

  rerex_free_matcher(matcher);
  rerex_free_pattern(re);

  return matched ? SERD_SUCCESS : SERD_FAILURE;
}

SerdStatus
serd_validate_model(SerdValidator* const   validator,
                    const SerdModel* const model,
                    const SerdNode* const  graph)
{
  SerdValidator* const ctx = validator;
  SerdStatus           st  = SERD_SUCCESS;

  ctx->model = model;
  ctx->graph = graph;

  if (ctx->checks & (1u << SERD_CHECK_PREDICATE_TYPE)) {
    update_status(&st, check_predicate_type(ctx));
  }

  if (ctx->checks & (1u << SERD_CHECK_CLASS_CYCLE)) {
    update_status(&st, check_subclass_cycle(ctx));
  }

  if (ctx->checks & (1u << SERD_CHECK_CLASS_LABEL)) {
    update_status(&st, check_class_label(ctx));
  }

  if (ctx->checks & (1u << SERD_CHECK_DATATYPE_PROPERTY)) {
    update_status(&st, check_datatype_property(ctx));
  }

  if (ctx->checks & ((1u << SERD_CHECK_DEPRECATED_PROPERTY) |
                     (1u << SERD_CHECK_DEPRECATED_CLASS))) {
    update_status(&st, check_deprecated(ctx));
  }

  if (ctx->checks & (1u << SERD_CHECK_FUNCTIONAL_PROPERTY)) {
    update_status(&st, check_functional_property(ctx));
  }

  if (ctx->checks & (1u << SERD_CHECK_INSTANCE_TYPE)) {
    update_status(&st, check_instance(ctx));
  }

  if (ctx->checks & (1u << SERD_CHECK_INVERSE_FUNCTIONAL_PROPERTY)) {
    update_status(&st, check_inverse_functional_property(ctx));
  }

  if (ctx->checks & (1u << SERD_CHECK_OBJECT_PROPERTY)) {
    update_status(&st, check_object_property(ctx));
  }

  if (ctx->checks & (1u << SERD_CHECK_PROPERTY_CYCLE)) {
    update_status(&st, check_subproperty_cycle(ctx));
  }

  if (ctx->checks & (1u << SERD_CHECK_PROPERTY_DOMAIN)) {
    update_status(&st, check_property_domain(ctx));
  }

  if (ctx->checks & (1u << SERD_CHECK_PROPERTY_LABEL)) {
    update_status(&st, check_property_label(ctx));
  }

  if (ctx->checks & (1u << SERD_CHECK_PROPERTY_RANGE)) {
    update_status(&st, check_property_range(ctx));
  }

  if (ctx->checks & ((1u << SERD_CHECK_DATATYPE_TYPE) | //
                     (1u << SERD_CHECK_LITERAL_INSTANCE) |
                     (1u << SERD_CHECK_LITERAL_MAX_EXCLUSIVE) |
                     (1u << SERD_CHECK_LITERAL_MAX_INCLUSIVE) |
                     (1u << SERD_CHECK_LITERAL_MIN_EXCLUSIVE) |
                     (1u << SERD_CHECK_LITERAL_MIN_INCLUSIVE) |
                     (1u << SERD_CHECK_LITERAL_PATTERN) |
                     (1u << SERD_CHECK_LITERAL_RESTRICTION) |
                     (1u << SERD_CHECK_LITERAL_VALUE))) {
    SerdRange* const all = serd_model_ordered(ctx->model, SERD_ORDER_SPO);
    SERD_FOREACH (statement, all) {
      update_status(&st, statement_check_valid_literal(ctx, statement));
    }
    serd_range_free(all);
  }

  ctx->graph = NULL;

  return (ctx->n_errors > 0)
           ? serd_world_logf_internal(ctx->model->world,
                                      SERD_ERR_INVALID,
                                      SERD_LOG_LEVEL_ERROR,
                                      NULL,
                                      "Failed %u of %u validation checks",
                                      ctx->n_errors,
                                      ctx->n_checks)
           : serd_world_logf_internal(ctx->model->world,
                                      SERD_SUCCESS,
                                      SERD_LOG_LEVEL_INFO,
                                      NULL,
                                      "Passed all %u validation checks",
                                      ctx->n_checks);
}
