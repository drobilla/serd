# Copyright 2020-2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: CC0-1.0 OR ISC

resource_filenames = [
  'epubstyle.css',
  'model_pipeline.svg',
  'serd.svg',
  'writer_pipeline.svg',
]

foreach filename: resource_filenames
  configure_file(copy: true, input: '../../resources/' + filename, output: filename)
endforeach
