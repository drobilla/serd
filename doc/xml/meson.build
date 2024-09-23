# Copyright 2020-2025 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

config = configuration_data()
config.set('SERD_SRCDIR', serd_src_root)
config.set('DOX_OUTPUT', meson.current_build_dir() / '..')

doxyfile = configure_file(
  configuration: config,
  input: files('../Doxyfile.in'),
  output: 'Doxyfile',
)

doxygen_xml = custom_target(
  'index.xml',
  command: [doxygen, '@INPUT0@'],
  input: [doxyfile] + c_headers,
  output: [
    'index.xml',

    'group__serd.xml',
    'group__serd__attributes.xml',
    'group__serd__buffer.xml',
    'group__serd__data.xml',
    'group__serd__env.xml',
    'group__serd__error.xml',
    'group__serd__errors.xml',
    'group__serd__field.xml',
    'group__serd__file_uri.xml',
    'group__serd__library.xml',
    'group__serd__memory.xml',
    'group__serd__node.xml',
    'group__serd__node__flags.xml',
    'group__serd__node__type.xml',
    'group__serd__reader.xml',
    'group__serd__reading__writing.xml',
    'group__serd__sink.xml',
    'group__serd__statement.xml',
    'group__serd__status.xml',
    'group__serd__stream.xml',
    'group__serd__streaming.xml',
    'group__serd__string.xml',
    'group__serd__syntax.xml',
    'group__serd__uri.xml',
    'group__serd__utilities.xml',
    'group__serd__version.xml',
    'group__serd__world.xml',
    'group__serd__writer.xml',

    'attributes_8h.xml',
    'buffer_8h.xml',
    'env_8h.xml',
    'error_8h.xml',
    'field_8h.xml',
    'file_uri_8h.xml',
    'node_8h.xml',
    'node__flags_8h.xml',
    'node__type_8h.xml',
    'reader_8h.xml',
    'serd_8h.xml',
    'sink_8h.xml',
    'statement__flags_8h.xml',
    'status_8h.xml',
    'stream_8h.xml',
    'string_8h.xml',
    'syntax_8h.xml',
    'uri_8h.xml',
    'version_8h.xml',
    'world_8h.xml',
    'writer_8h.xml',

    'structSerdBuffer.xml',
    'structSerdError.xml',
    'structSerdNode.xml',
    'structSerdString.xml',
    'structSerdURIComponent.xml',
    'structSerdURIView.xml',
  ],
)
