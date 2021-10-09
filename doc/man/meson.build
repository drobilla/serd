if get_option('tools')
  # Install tool man pages
  install_man('serd-filter.1')
  install_man('serd-pipe.1')
  install_man('serd-sort.1')

  # Build/install HTML man pages if mandoc is present
  if mandoc.found()
    configure_file(copy: true,
                   input: files('mandoc.css'),
                   output: 'mandoc.css',
                   # install: true,
                   install_dir: docdir / versioned_name / 'man')

    mandoc_html_command = [mandoc,
                           '-Thtml',
                           '-Wwarning',
                           '-Ostyle=mandoc.css,man=%N.html',
                           '@INPUT@']

    page_names = [
      'serd-filter',
      'serd-pipe',
      'serd-sort',
    ]

    foreach name : page_names
      custom_target(name + '.1',
                    capture: true,
                    command: mandoc_html_command,
                    input: files(name + '.1'),
                    install: true,
                    install_dir: docdir / versioned_name / 'man',
                    output: name + '.html')
    endforeach
  endif
endif
