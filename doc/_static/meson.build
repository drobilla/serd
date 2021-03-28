# Copyright 2020-2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

resource_filenames = [
  'epubstyle.css',
  'model_pipeline.svg',
  'serd.svg',
  'writer_pipeline.svg',
]

foreach filename: resource_filenames
  configure_file(copy: true, input: filename, output: filename)
endforeach
