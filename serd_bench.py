#!/usr/bin/env python

import optparse
import os
import subprocess
import sys

class WorkingDirectory:
    "Scoped context for changing working directory"
    def __init__(self, working_dir):
        self.original_dir = os.getcwd()
        self.working_dir  = working_dir

    def __enter__(self):
        os.chdir(self.working_dir)
        return self

    def __exit__(self, type, value, traceback):
        os.chdir(self.original_dir)

def filename(n):
    "Filename for a generated file with n statements"
    return 'gen%d.ttl' % n

def gen(sp2b_dir, n_min, n_max, step):
    "Generate files with n_min ... n_max statements if they are not present"
    with WorkingDirectory(sp2b_dir) as dir:
        for n in range(n_min, n_max + step, step):
            out_path = os.path.join(dir.original_dir, 'build', filename(n))
            if not os.path.exists(out_path):
                subprocess.call(['sp2b_gen', '-t', str(n), out_path])

def write_header(results, progs):
    "Write the header line for TSV output"
    results.write('n')
    for prog in progs:
        results.write('\t' + os.path.basename(prog.split()[0]))
    results.write('\n')

def parse_time(report):
    "Return user time and max RSS from a /usr/bin/time -v report"
    time = memory = None
    for line in report.split('\n'):
        if line.startswith('\tUser time'):
            time = float(line[line.find(':') + 1:])
        elif line.startswith('\tMaximum resident set'):
            memory = float(line[line.find(':') + 1:]) * 1024

    return (time, memory)

def run(progs, n_min, n_max, step):
    "Benchmark each program with n_min ... n_max statements"
    with WorkingDirectory('build'):
        results = {'time':       open('serdi-time.txt', 'w'),
                   'throughput': open('serdi-throughput.txt', 'w'),
                   'memory':     open('serdi-memory.txt', 'w')}

        # Write TSV header for all output files
        for name, f in results.iteritems():
            write_header(f, progs)

        for n in range(n_min, n_max + step, step):
            # Add first column (n) to rows
            rows = {}
            for name, _ in results.iteritems():
                rows[name] = [str(n)]

            # Run each program and fill rows with measurements
            for prog in progs:
                cmd = '/usr/bin/time -v ' + prog + ' ' + filename(n)
                with open(filename(n) + '.out', 'w') as out:
                    sys.stderr.write(cmd + '\n')
                    proc = subprocess.Popen(
                        cmd.split(), stdout=out, stderr=subprocess.PIPE)

                    time, memory = parse_time(proc.communicate()[1])
                    rows['time']       += ['%.07f' % time]
                    rows['throughput'] += ['%d' % (n / time)]
                    rows['memory']     += [str(memory)]

            # Write rows to output files
            for name, f in results.iteritems():
                f.write('\t'.join(rows[name]) + '\n')

        for name, _ in results.iteritems():
            sys.stderr.write('wrote build/serdi-%s.txt\n' % name)

if __name__ == "__main__":
    class OptParser(optparse.OptionParser):
        def format_epilog(self, formatter):
            return self.expand_prog_name(self.epilog)

    opt = OptParser(
        usage='%prog [OPTION]... SP2B_DIR',
        description='Benchmark RDF reading and writing commands\n',
        epilog='''
Example:       
	%prog --max 100000 \\
		--run 'rapper -i turtle -o turtle' \\
		--run 'riot --output=ttl' \\
		--run 'rdfpipe -i turtle -o turtle' /path/to/sp2b/src/
 ''')

    opt.add_option('--max', type='int', default=1000000,
                   help='maximum triple count')
    opt.add_option('--run', type='string', action='append', default=[],
                   help='additional command to run (input file is appended)')

    (options, args) = opt.parse_args()
    if len(args) != 1:
        opt.print_usage()
        sys.exit(1)

    progs = ['serdi -b -f -i turtle -o turtle'] + options.run
    min_n = options.max / 10
    max_n = options.max
    step  = min_n

    gen(str(args[0]), min_n, max_n, step)
    run(progs, min_n, max_n, step)
