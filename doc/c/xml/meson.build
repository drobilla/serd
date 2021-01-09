doxygen = find_program('doxygen')

c_doxygen_input = []
foreach h : c_headers
  c_doxygen_input += ['..' / h]
endforeach

config = configuration_data()
config.set('SERD_SRCDIR', serd_src_root)
config.set('DOX_OUTPUT', meson.current_build_dir() / '..')

c_doxyfile = configure_file(configuration: config,
                            input: files('../Doxyfile.in'),
                            output: 'Doxyfile')

c_index_xml = custom_target('serd-c-index.xml',
                            command: [doxygen, '@INPUT0@'],
                            input: [c_doxyfile] + c_header_files,
                            output: 'index.xml')
