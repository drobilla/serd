doxygen = find_program('doxygen')

cpp_doxygen_input = []
foreach h : cpp_headers
  cpp_doxygen_input += ['..' / h]
endforeach

config = configuration_data()
config.set('SERD_SRCDIR', serd_src_root)
config.set('DOX_OUTPUT', meson.current_build_dir() / '..')

cpp_doxyfile = configure_file(configuration: config,
                              input: files('../Doxyfile.in'),
                              output: 'Doxyfile')

cpp_index_xml = custom_target('serd-c-index.xml',
                              command: [doxygen, '@INPUT0@'],
                              input: [cpp_doxyfile] + cpp_header_files,
                              output: 'index.xml')
