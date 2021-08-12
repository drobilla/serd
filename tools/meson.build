tool_c_args = c_warnings + prog_args

tools = [
  'filter',
  'pipe',
  'sort',
]

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
