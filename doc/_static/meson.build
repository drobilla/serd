# Copyright 2020-2023 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

doc_static_build_dir = meson.current_build_dir()

static_files = files(
  'model_pipeline.svg',
  'serd.svg',
  'writer_pipeline.svg',
)

foreach static_file : static_files
  configure_file(
    copy: true,
    input: static_file,
    output: '@PLAINNAME@',
  )
endforeach
