tool_c_args = c_warnings + platform_args + prog_args
tool_link_args = []

if get_option('static') and cc.get_id() != 'msvc'
  tool_link_args += ['-static']
endif

serd_filter = executable('serd-filter',
                         ['serd-filter.c', 'console.c'],
                         c_args: tool_c_args,
                         link_args: tool_link_args,
                         install: true,
                         dependencies: serd_dep)

serd_pipe = executable('serd-pipe',
                       ['serd-pipe.c', 'console.c'],
                       c_args: tool_c_args,
                       link_args: tool_link_args,
                       install: true,
                       dependencies: serd_dep)

serd_sort = executable('serd-sort',
                       ['serd-sort.c', 'console.c'],
                       c_args: tool_c_args,
                       link_args: tool_link_args,
                       install: true,
                       dependencies: serd_dep)

serd_validate = executable('serd-validate',
                           ['serd-validate.c', 'console.c'],
                           c_args: tool_c_args,
                           link_args: tool_link_args,
                           install: true,
                           dependencies: serd_dep)
