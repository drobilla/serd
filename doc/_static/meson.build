resource_filenames = [
  'diagram.css',
  'model_pipeline.svg',
  'serd.svg',
  'writer_pipeline.svg',
]

foreach filename: resource_filenames
  configure_file(copy: true, input: '../../resources/' + filename, output: filename)
endforeach
