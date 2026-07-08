# Copyright 2026 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

FROM docker.io/drobilla/debian-musl:20260709 AS build

WORKDIR /root/serd
COPY --parents include/ src/ meson* .

RUN CC=musl-gcc meson setup \
    -Db_lto=true \
    -Dbuildtype=release \
    -Dc_args="-march=x86-64 -mtune=generic" \
    -Ddefault_library=static \
    -Ddocs=disabled \
    -Dman=disabled \
    -Dprefer_static=true \
    -Dstatic=true \
    -Dtests=disabled \
    build && \
    ninja -C build

FROM scratch
LABEL org.opencontainers.image.authors="David Robillard <d@drobilla.net>"
LABEL org.opencontainers.image.description="A command-line tool for processing RDF documents"
LABEL org.opencontainers.image.licenses="ISC"
LABEL org.opencontainers.image.title="serdi"
LABEL org.opencontainers.image.url="https://drobilla.net/software/serd"
COPY --from=build /root/serd/build/serdi /
ENV PATH=/
ENTRYPOINT ["serdi"]
