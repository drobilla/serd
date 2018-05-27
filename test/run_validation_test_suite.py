#!/usr/bin/env python3

"""Run the serd RDF validation test suite."""

import argparse
import os
import re
import shlex
import subprocess
import sys
import tempfile
import urllib.parse

import serd_test_util

NS_CHECKS = "http://drobilla.net/ns/serd/checks#"
NS_MF = "http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#"
NS_SERD = "http://drobilla.net/ns/serd#"


def log_error(message):
    """Log an error message to stderr"""

    sys.stderr.write("error: ")
    sys.stderr.write(message)


def _uri_path(uri):
    path = urllib.parse.urlparse(uri).path
    drive = os.path.splitdrive(path[1:])[0]
    return os.path.realpath(path) if not drive else path[1:]


def _load_rdf(filename, base_uri, command_prefix):
    """Load an RDF file as dictionaries via serdi (only supports URIs)."""

    rdf_type = "http://www.w3.org/1999/02/22-rdf-syntax-ns#type"
    model = {}
    instances = {}

    cmd = command_prefix + ["-I", base_uri, filename]
    proc = subprocess.run(cmd, capture_output=True, check=True)
    for line in proc.stdout.splitlines():
        matches = re.match(
            r"<([^ ]*)> <([^ ]*)> <([^ ]*)> \.", line.decode("utf-8")
        )
        if matches:
            s, p, o = (matches.group(1), matches.group(2), matches.group(3))
            if s not in model:
                model[s] = {p: [o]}
            elif p not in model[s]:
                model[s][p] = [o]
            else:
                model[s][p].append(o)

            if p == rdf_type:
                if o not in instances:
                    instances[o] = set([s])
                else:
                    instances[o].update([s])

    return model, instances


def _run_positive_test(command, out_filename):
    command_string = " ".join([shlex.quote(c) for c in command])

    with open(out_filename, "w") as stdout:
        proc = subprocess.run(command, check=False, stdout=stdout)
        if proc.returncode != 0:
            log_error("Unexpected command failure\n")
            sys.stderr.write(command_string + "\n")
            return 1

        return proc.returncode

    return 1


def _run_negative_test(command, check_name, out_filename):
    command_string = " ".join([shlex.quote(c) for c in command])

    with open(out_filename, "w") as stdout:
        with tempfile.TemporaryFile() as stderr:
            proc = subprocess.run(
                command, check=False, stdout=stdout, stderr=stderr
            )

            # Check that serdi returned with status SERD_ERR_INVALID
            if proc.returncode != 16:
                log_error("Unexpected status {}\n".format(proc.returncode))
                sys.stderr.write(command_string + "\n")
                return 1

            # Check that an error message was printed
            stderr.seek(0, 2)  # Seek to end
            if stderr.tell() == 0:  # Empty
                log_error("No error message printed\n")
                sys.stderr.write(command_string + "\n")
                return 1

            # Check that the expected check printed an error message
            stderr.seek(0)  # Seek to start
            err_output = stderr.read().decode("utf-8")
            if check_name and "[{}]".format(check_name) not in err_output:
                log_error("Test didn't trigger {}\n".format(check_name))
                sys.stderr.write(command_string + "\n")
                sys.stderr.write(err_output + "\n")
                return 1

            return 0

    return 1


def validation_test_suite(
    manifest_path,
    schemas,
    base_uri,
    report_filename,
    command_prefix,
):
    """Run all tests in a test suite manifest."""

    test_dir = os.path.dirname(manifest_path)
    model, instances = serd_test_util.load_rdf(
        manifest_path, base_uri, command_prefix
    )

    top_dir = os.path.commonpath([os.getcwd(), os.path.abspath(test_dir)])
    out_test_dir = os.path.relpath(test_dir, top_dir)

    os.makedirs(out_test_dir, exist_ok=True)

    asserter = ""
    if os.getenv("USER") == "drobilla":
        asserter = "http://drobilla.net/drobilla#me"

    class Results:
        """Aggregated count of all tests and results."""
        def __init__(self):
            self.n_tests = 0
            self.n_failures = 0

    def run_tests(tests, expected_return, results):
        for test in sorted(tests):
            test_uri = model[test][NS_MF + "action"][0]
            test_uri_path = _uri_path(test_uri)
            test_name = os.path.basename(test_uri_path)
            test_path = os.path.join(test_dir, test_name)
            out_filename = os.path.join(out_test_dir, test_name + ".out")

            results.n_tests += 1

            if expected_return == 0:  # Positive test
                options = ["-V", "all", "-I", test_uri]
                command = command_prefix + options + schemas + [test_path]

                status = _run_positive_test(command, out_filename)
                passed = status == 0
                results.n_failures += status

            else:  # Negative test
                if NS_SERD + "triggersCheck" not in model[test]:
                    log_error("{} has no serd:triggersCheck".format(test_name))

                check_names = []
                if NS_SERD + "triggersCheck" in model[test]:
                    for check in model[test][NS_SERD + "triggersCheck"]:
                        check_names += [check[len(NS_CHECKS) :]]

                options = ["-I", test_uri]
                for check_name in check_names:
                    options += ["-V", check_name]

                command = command_prefix + options + schemas + [test_path]
                status = _run_negative_test(command, check_name, out_filename)
                passed = status == 0
                results.n_failures += status

                if passed:
                    options = ["-I", test_uri, "-V", "all"]
                    for check_name in check_names:
                        options += ["-X", check_name]

                    command = command_prefix + options + schemas + [test_path]
                    status = _run_positive_test(command, out_filename)
                    passed = status == 0
                    results.n_failures += status

            # Write test report entry
            if report_filename:
                with open(report_filename, "a") as report:
                    report.write(
                        serd_test_util.earl_assertion(test, passed, asserter)
                    )

    # Run all test types in the test suite
    results = Results()
    ns_serd = "http://drobilla.net/ns/serd#"
    for test_class, instances in instances.items():
        if test_class.startswith(ns_serd):
            expected = 1 if "Negative" in test_class else 0
            run_tests(instances, expected, results)

    # Print result summary
    if results.n_failures > 0:
        log_error(
            "{}/{} tests failed\n".format(results.n_failures, results.n_tests)
        )
    else:
        sys.stdout.write("All {} tests passed\n".format(results.n_tests))

    return results.n_failures


def main():
    """Run the command line tool."""

    parser = argparse.ArgumentParser(
        usage="%(prog)s [OPTION]... MANIFEST BASE_URI SCHEMA...",
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )

    parser.add_argument("--report", help="path to write result report to")
    parser.add_argument("--serdi", default="serdi", help="path to serdi")
    parser.add_argument("--wrapper", default="", help="executable wrapper")
    parser.add_argument("manifest", help="test suite manifest.ttl file")
    parser.add_argument("base_uri", help="base URI for tests")
    parser.add_argument("schema", nargs="+", help="schema file")

    args = parser.parse_args(sys.argv[1:])
    command_prefix = shlex.split(args.wrapper) + [args.serdi]

    return validation_test_suite(
        args.manifest,
        args.schema,
        args.base_uri,
        args.report,
        command_prefix,
    )


if __name__ == "__main__":
    try:
        sys.exit(main())
    except subprocess.CalledProcessError as error:
        if error.stderr is not None:
            sys.stderr.write(error.stderr.decode("utf-8"))

        log_error(str(error) + "\n")
        sys.exit(error.returncode)
