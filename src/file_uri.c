// Copyright 2011-2025 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#include "string_utils.h"

#include <serd/buffer.h>
#include <serd/file_uri.h>
#include <serd/stream.h>
#include <serd/string.h>
#include <zix/allocator.h>
#include <zix/string_view.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

char*
serd_parse_file_uri(ZixAllocator* const allocator,
                    const char* const   uri,
                    char** const        hostname)
{
  assert(uri);

  const char* path = uri;
  if (hostname) {
    *hostname = NULL;
  }

  if (!strncmp(uri, "file://", 7)) {
    const char* auth = uri + 7;
    if (*auth == '/') { // No hostname
      path = auth;
    } else { // Has hostname
      if (!(path = strchr(auth, '/'))) {
        return NULL;
      }

      if (hostname) {
        const size_t len = (size_t)(path - auth);
        if (!(*hostname = (char*)zix_calloc(allocator, len + 1, 1))) {
          return NULL;
        }

        memcpy(*hostname, auth, len);
      }
    }
  }

  if (is_windows_path(path + 1)) {
    ++path;
  }

  SerdBuffer buffer = {allocator, NULL, 0};
  for (const char* s = path; *s; ++s) {
    if (*s == '%') {
      if (is_hexdig(*(s + 1)) && is_hexdig(*(s + 2))) {
        const uint8_t hi = hex_digit_value((const uint8_t)s[1]);
        const uint8_t lo = hex_digit_value((const uint8_t)s[2]);
        const char    c  = (char)((hi << 4U) | lo);
        if (serd_buffer_sink(&c, 1, &buffer) < 1U) {
          zix_free(buffer.allocator, buffer.buf);
          return NULL; // Allocation failed
        }

        s += 2;
      } else {
        zix_free(buffer.allocator, buffer.buf);
        return NULL; // Invalid percent-encoding
      }
    } else if (serd_buffer_sink(s, 1, &buffer) < 1U) {
      zix_free(buffer.allocator, buffer.buf);
      return NULL; // Allocation failed
    }
  }

  char* const result = serd_buffer_sink_finish(&buffer);
  if (!result) {
    zix_free(buffer.allocator, buffer.buf);
  }

  return result;
}

static bool
is_uri_path_char(const char c)
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

size_t
serd_write_file_uri(const ZixStringView path,
                    const ZixStringView hostname,
                    const SerdWriteFunc sink,
                    void* const         stream)
{
  assert(sink);

  const bool is_windows = is_windows_path(path.data);
  size_t     len        = 0U;

  if (is_dir_sep(path.data[0]) || is_windows) {
    len += sink("file://", strlen("file://"), stream);
    if (hostname.length) {
      len += sink(hostname.data, hostname.length, stream);
    }

    if (is_windows) {
      len += sink("/", 1, stream);
    }
  }

  for (size_t i = 0; i < path.length; ++i) {
    if (is_uri_path_char(path.data[i])) {
      len += sink(path.data + i, 1, stream);
#ifdef _WIN32
    } else if (path.data[i] == '\\') {
      len += sink("/", 1, stream);
#endif
    } else {
      char escape_str[10] = {'%', 0, 0, 0, 0, 0, 0, 0, 0, 0};
      snprintf(
        escape_str + 1, sizeof(escape_str) - 1, "%X", (unsigned)path.data[i]);
      len += sink(escape_str, 3, stream);
    }
  }

  return len;
}

static size_t
string_sink(const void* const buf, const size_t len, void* const stream)
{
  char** ptr = (char**)stream;
  memcpy(*ptr, buf, len);
  *ptr += len;
  return len;
}

static size_t
no_sink(const void* const buf, const size_t len, void* const stream)
{
  (void)buf;
  (void)stream;
  return len;
}

SerdString
serd_file_uri_to_string(ZixAllocator* const allocator,
                        const ZixStringView path,
                        const ZixStringView hostname)
{
  SerdString   string = {0U, NULL};
  const size_t length = serd_write_file_uri(path, hostname, no_sink, NULL);

  if ((string.data = (char*)zix_calloc(allocator, length + 1U, 1U))) {
    char*        ptr = string.data;
    const size_t len = serd_write_file_uri(path, hostname, string_sink, &ptr);

    string.length    = len;
    string.data[len] = '\0';
  }

  return string;
}
