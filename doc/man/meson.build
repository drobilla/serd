# Copyright 2020-2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

if not get_option('tools').disabled()
  # Install tool man pages
  if not get_option('man').disabled()
    install_man('serd-filter.1')
    install_man('serd-pipe.1')
    install_man('serd-sort.1')
  endif

  # Build/install HTML man pages if mandoc is present
  mandoc = find_program('mandoc', required: get_option('man_html'))
  if mandoc.found()
    mandoc_css = files('mandoc.css')

    stylelint = find_program('stylelint', required: get_option('tests'))
    if stylelint.found()
      test('stylelint', stylelint, args: [mandoc_css], suite: 'data')
    endif

    configure_file(
      copy: true,
      input: mandoc_css,
      output: 'mandoc.css',
      install_dir: docdir / versioned_name / 'man',
    )

    mandoc_html_command = [
      mandoc,
      '-Thtml',
      '-Wwarning',
      '-Ostyle=mandoc.css,man=%N.html',
      '@INPUT@',
    ]

    page_names = [
      'serd-filter',
      'serd-pipe',
      'serd-sort',
    ]

    foreach name : page_names
      custom_target(
        name + '.html',
        capture: true,
        command: mandoc_html_command,
        input: files(name + '.1'),
        install: true,
        install_dir: docdir / versioned_name / 'man',
        output: name + '.html',
      )
    endforeach
  endif
endif
