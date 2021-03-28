resource_filenames = [
  'epubstyle.css',
  'model_pipeline.svg',
  'serd.svg',
  'writer_pipeline.svg',
]

foreach filename: resource_filenames
  configure_file(copy: true, input: '../../resources/' + filename, output: filename)
endforeach
