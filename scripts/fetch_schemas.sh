#!/bin/sh

# dc
curl https://www.dublincore.org/specifications/dublin-core/dcmi-terms/dublin_core_elements.ttl \
    | tools/serd-filter -I turtle -O turtle -v '?s <http://www.w3.org/2004/02/skos/core#note> ?o .' \
    | tools/serd-filter -I turtle -O turtle -v '?s <http://purl.org/dc/terms/description> ?o .' \
    | tools/serd-filter -I turtle -O turtle -v '?s <http://purl.org/dc/terms/issued> ?o .' \
    | tools/serd-sort -I turtle -O turtle > ../schemas/dc.ttl

# dcterms
curl http://www.dublincore.org/specifications/dublin-core/dcmi-terms/dublin_core_terms.ttl \
    | cat <(echo -e '@prefix dc: <http://purl.org/dc/elements/1.1/> .\n@prefix dcam: <http://purl.org/dc/dcam/> .\n@prefix dcterms: <http://purl.org/dc/terms/> .\n@prefix xsd: <http://www.w3.org/2001/XMLSchema#> .\n') - \
    | tools/serd-sort -I turtle -O turtle \
    > ../schemas/dcterms.ttl

# dcam
curl https://www.dublincore.org/specifications/dublin-core/dcmi-terms/dublin_core_abstract_model.ttl \
    | tools/serd-sort -I turtle -O turtle > ../schemas/dcam.ttl

# rdf
curl http://www.w3.org/1999/02/22-rdf-syntax-ns# \
    | tools/serd-sort -I turtle -O turtle > ../schemas/rdf.ttl

# rdfs
curl http://www.w3.org/2000/01/rdf-schema# \
    | tools/serd-sort -I turtle -O turtle > ../schemas/rdfs.ttl

# owl
curl http://www.w3.org/2002/07/owl# \
    | tools/serd-filter -I turtle -O turtle -v '?s <http://www.w3.org/2002/07/owl#imports> ?o .' \
    | tools/serd-filter -I turtle -O turtle -v '?s <http://www.w3.org/2003/g/data-view#namespaceTransformation> ?o .' \
    | tools/serd-sort -I turtle -O turtle \
    | sed -e "s/\r//g" > ../schemas/owl.ttl

# doap
curl https://raw.githubusercontent.com/ewilderj/doap/master/schema/doap.rdf \
    | rapper -o turtle - 'http://example.org/' \
    | tools/serd-filter -I turtle -O turtle -v '?s <http://www.w3.org/2002/07/owl#imports> ?o .' \
    | tools/serd-sort -I turtle -O turtle > ../schemas/doap.ttl

# foaf
curl http://xmlns.com/foaf/spec/index.rdf \
    | rapper -o turtle - 'http://example.org/' \
    | tools/serd-sort -I turtle -O turtle > ../schemas/foaf.ttl

