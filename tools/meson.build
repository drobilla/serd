tool_c_args = c_warnings + prog_args

serd_filter = executable('serd-filter',
                         ['serd-filter.c', 'console.c'],
                         c_args: tool_c_args,
                         install: true,
                         dependencies: serd_dep)

serd_pipe = executable('serd-pipe',
                       ['serd-pipe.c', 'console.c'],
                       c_args: tool_c_args,
                       install: true,
                       dependencies: serd_dep)

serd_sort = executable('serd-sort',
                       ['serd-sort.c', 'console.c'],
                       c_args: tool_c_args,
                       install: true,
                       dependencies: serd_dep)

serd_validate = executable('serd-validate',
                           ['serd-validate.c', 'console.c'],
                           c_args: tool_c_args,
                           install: true,
                           dependencies: serd_dep)
