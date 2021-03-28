# Copyright 2020-2023 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

doc_static_build_dir = meson.current_build_dir() / '..' / '_static'

resources = files(
  'epubstyle.css',
  'model_pipeline.svg',
  'serd.svg',
  'writer_pipeline.svg',
)

foreach resource : resources
  configure_file(copy: true, input: resource, output: '@PLAINNAME@')
endforeach
