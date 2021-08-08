tool_c_args = c_warnings + platform_args + prog_args
tool_link_args = []

if get_option('static') and cc.get_id() != 'msvc'
  tool_link_args += ['-static']
endif

serdi = executable('serdi',
                   ['serdi.c', 'console.c'],
                   c_args: tool_c_args,
                   link_args: tool_link_args,
                   install: true,
                   dependencies: serd_dep)
