// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "string_utils.h"
#include "try_write.h"

#include <serd/buffer.h>
#include <serd/file_uri.h>
#include <serd/output_stream.h>
#include <serd/status.h>
#include <serd/stream.h>
#include <serd/stream_result.h>
#include <serd/string.h>
#include <zix/allocator.h>
#include <zix/string_view.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static char*
parse_hostname(ZixAllocator* const allocator,
               const char* const   authority,
               char** const        hostname)
{
  char* const path = strchr(authority, '/');

  if (path && hostname) {
    const size_t len = (size_t)(path - authority);
    if (!(*hostname = (char*)zix_calloc(allocator, len + 1, 1))) {
      return NULL;
    }

    memcpy(*hostname, authority, len);
  }

  return path;
}

char*
serd_parse_file_uri(ZixAllocator* const allocator,
                    const char* const   uri,
                    char** const        hostname)
{
  assert(uri);

  SerdStatus st = SERD_SUCCESS;

  const char* path = uri;
  if (hostname) {
    *hostname = NULL;
  }

  if (!strncmp(uri, "file://", 7)) {
    const char* auth = uri + 7;
    if (*auth == '/') { // No hostname
      path = auth;
    } else if (!(path = parse_hostname(allocator, auth, hostname))) {
      return NULL;
    }
  }

  if (is_windows_path(path + 1)) {
    ++path;
  }

  SerdBuffer       buffer = {allocator, NULL, 0};
  SerdOutputStream out    = serd_open_output_buffer(&buffer);
  for (const char* s = path; !st && *s; ++s) {
    if (*s == '%') {
      if (is_hexdig(*(s + 1)) && is_hexdig(*(s + 2))) {
        const uint8_t hi = hex_digit_value((const uint8_t)s[1]);
        const uint8_t lo = hex_digit_value((const uint8_t)s[2]);
        const char    c  = (char)((hi << 4U) | lo);

        st = out.write(out.stream, 1, &c).status;
        s += 2;
      } else {
        st = SERD_BAD_SYNTAX;
      }
    } else {
      st = out.write(out.stream, 1, s).status;
    }
  }

  const SerdStatus cst = serd_close_output(&out);
  if (st || cst) {
    zix_free(buffer.allocator, buffer.buf);
    return NULL;
  }

  return buffer.buf;
}

static bool
is_unescaped_uri_path_char(const char c)
{
  return is_alpha(c) || is_digit(c) || strchr("!$&\'()*+,-./:;=@_~", c);
}

static bool
is_dir_sep(const char c)
{
#ifdef _WIN32
  return c == '\\' || c == '/';
#else
  return c == '/';
#endif
}

SerdStreamResult
serd_write_file_uri(const ZixStringView path,
                    const ZixStringView hostname,
                    const SerdWriteFunc sink,
                    void* const         stream)
{
  assert(sink);

  static const char hex_chars[] = "0123456789ABCDEF";

  const bool is_windows = is_windows_path(path.data);

  SerdStreamResult wr = {SERD_SUCCESS, 0U};

  if (is_dir_sep(path.data[0]) || is_windows) {
    TRY_WRITE(wr, sink(stream, strlen("file://"), "file://"));
    if (hostname.length) {
      TRY_WRITE(wr, sink(stream, hostname.length, hostname.data));
    }

    if (is_windows) {
      TRY_WRITE(wr, sink(stream, 1, "/"));
    }
  }

  for (size_t i = 0; i < path.length; ++i) {
    if (is_unescaped_uri_path_char(path.data[i])) {
      TRY_WRITE(wr, sink(stream, 1, path.data + i));
#ifdef _WIN32
    } else if (path.data[i] == '\\') {
      TRY_WRITE(wr, sink(stream, 1, "/"));
#endif
    } else {
      const uint8_t c        = (uint8_t)path.data[i];
      const char    escape[] = {'%', hex_chars[c >> 4U], hex_chars[c & 0x0FU]};
      TRY_WRITE(wr, sink(stream, 3, escape));
    }
  }

  return wr;
}

static SerdStreamResult
string_sink(void* const stream, const size_t len, const void* const buf)
{
  char** const ptr = (char**)stream;
  memcpy(*ptr, buf, len);
  *ptr += len;
  const SerdStreamResult r = {SERD_SUCCESS, len};
  return r;
}

static SerdStreamResult
no_sink(void* const stream, const size_t len, const void* const buf)
{
  (void)stream;
  (void)buf;
  const SerdStreamResult r = {SERD_SUCCESS, len};
  return r;
}

SerdString
serd_file_uri_to_string(ZixAllocator* const allocator,
                        const ZixStringView path,
                        const ZixStringView hostname)
{
  SerdString       string = {0U, NULL};
  SerdStreamResult r      = serd_write_file_uri(path, hostname, no_sink, NULL);
  const size_t     len    = r.count;
  assert(!r.status);

  if ((string.data = (char*)zix_calloc(allocator, len + 1U, 1U))) {
    char* ptr = string.data;
    r         = serd_write_file_uri(path, hostname, string_sink, &ptr);

    assert(r.count == len);
    string.length    = len;
    string.data[len] = '\0';
  }

  return string;
}
