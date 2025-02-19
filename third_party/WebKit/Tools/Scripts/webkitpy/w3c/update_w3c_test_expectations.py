# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A script to modify TestExpectations lines based layout test failures in try jobs.

This script outputs a list of test expectation lines to add to a 'TestExpectations' file
by retrieving the try job results for the current CL.
"""

import logging

from webkitpy.common.net import rietveld
from webkitpy.common.net.buildbot import BuildBot
from webkitpy.common.webkit_finder import WebKitFinder
from webkitpy.w3c.test_parser import TestParser

_log = logging.getLogger(__name__)


def main(host, port):
    expectations_file = port.path_to_generic_test_expectations_file()
    expectations_line_adder = W3CExpectationsLineAdder(host)
    issue_number = expectations_line_adder.get_issue_number()
    try_bots = expectations_line_adder.get_try_bots()
    try_jobs = rietveld.latest_try_jobs(issue_number, try_bots, host.web)
    test_expectations = {}
    if not try_jobs:
        print 'No Try Job information was collected.'
        return 1
    for job in try_jobs:
        platform_results = expectations_line_adder.get_failing_results_dict(BuildBot(), job.builder_name, job.build_number)
        test_expectations = expectations_line_adder.merge_dicts(test_expectations, platform_results)
    for test_name, platform_result in test_expectations.iteritems():
        test_expectations[test_name] = expectations_line_adder.merge_same_valued_keys(platform_result)
    test_expectations = expectations_line_adder.get_expected_txt_files(test_expectations)
    test_expectation_lines = expectations_line_adder.create_line_list(test_expectations)
    expectations_line_adder.write_to_test_expectations(host, expectations_file, test_expectation_lines)


class W3CExpectationsLineAdder(object):

    def __init__(self, host):
        self._host = host
        self.filesystem = host.filesystem

    def get_issue_number(self):
        return self._host.scm().get_issue_number()

    def get_try_bots(self):
        return self._host.builders.all_try_builder_names()

    def _generate_results_dict(self, platform, result_list):
        test_dict = {}
        if '-' in platform:
            platform = platform[platform.find('-') + 1:].capitalize()
        for result in result_list:
            test_dict[result.test_name()] = {
                platform: {
                    'expected': result.expected_results(),
                    'actual': result.actual_results(),
                    'bug': 'crbug.com/626703'
                }}
        return test_dict

    def get_failing_results_dict(self, buildbot, builder_name, build_number):
        """Returns a nested dict of failing test results.

        Retrieves a full list of layout test results from a builder result URL. Collects
        the builder name, platform and a list of tests that did not run as expected.

        Args:
            builder: A Builder object.
            build: A Build object.

        Returns:
            A dictionary with the structure: {
                'key': {
                    'expected': 'TIMEOUT',
                    'actual': 'CRASH',
                    'bug': 'crbug.com/11111'
                }
            }
        """
        results_url = buildbot.results_url(builder_name, build_number)
        layout_test_results = buildbot.fetch_layout_test_results(results_url)
        platform = self._host.builders.port_name_for_builder_name(builder_name)
        result_list = layout_test_results.didnt_run_as_expected_results()
        failing_results_dict = self._generate_results_dict(platform, result_list)
        return failing_results_dict

    def merge_dicts(self, target, source, path=None):
        """Recursively merge nested dictionaries, returning the target dictionary

        Merges the keys and values from the source dict into the target dict.

        Args:
            target: A dictionary where the keys and values from source are updated
                in.
            source: A dictionary that provides the new keys and values to be added
                into target.

        Returns:
            An updated target dictionary.
        """
        path = path or []
        for key in source:
            if key in target:
                if (isinstance(target[key], dict)) and isinstance(source[key], dict):
                    self.merge_dicts(target[key], source[key], path + [str(key)])
                elif target[key] == source[key]:
                    pass
                else:
                    raise ValueError('The key: %s already exist in the target dictionary.' % '.'.join(path))
            else:
                target[key] = source[key]
        return target

    def merge_same_valued_keys(self, dictionary):
        """Merges keys in dictionary with same value.

        Traverses through a dict and compares the values of keys to one another.
        If the values match, the keys are combined to a tuple and the previous keys
        are removed from the dict.

        Args:
            dictionary: A dictionary with a dictionary as the value.

        Returns:
            A new dictionary with updated keys to reflect matching values of keys.
            Example: {
                'one': {'foo': 'bar'},
                'two': {'foo': 'bar'},
                'three': {'foo': 'bar'}
            }
            is converted to a new dictionary with that contains
            {('one', 'two', 'three'): {'foo': 'bar'}}
        """
        merged_dict = {}
        matching_value_keys = set()
        keys = sorted(dictionary.keys())
        while keys:
            current_key = keys[0]
            found_match = False
            if current_key == keys[-1]:
                merged_dict[current_key] = dictionary[current_key]
                keys.remove(current_key)
                break
            for next_item in keys[1:]:
                if dictionary[current_key] == dictionary[next_item]:
                    found_match = True
                    matching_value_keys.update([current_key, next_item])
                if next_item == keys[-1]:
                    if found_match:
                        merged_dict[tuple(matching_value_keys)] = dictionary[current_key]
                        keys = [k for k in keys if k not in matching_value_keys]
                    else:
                        merged_dict[current_key] = dictionary[current_key]
                        keys.remove(current_key)
            matching_value_keys = set()
        return merged_dict

    def get_expectations(self, results):
        """Returns a set of test expectations for a given test dict.

        Returns a set of one or more test expectations based on the expected
        and actual results of a given test name.

        Args:
            results: A dictionary that maps one test to its results. Example: {
                'test_name': {
                    'expected': 'PASS',
                    'actual': 'FAIL',
                    'bug': 'crbug.com/11111'
                }
            }

        Returns:
            A set of one or more test expectations with the first letter capitalized. Example:
            set(['Failure', 'Timeout'])
        """
        expectations = set()
        failure_types = ['TEXT', 'FAIL', 'IMAGE+TEXT', 'IMAGE', 'AUDIO', 'MISSING', 'LEAK']
        test_expectation_types = ['SLOW', 'TIMEOUT', 'CRASH', 'PASS', 'REBASELINE', 'NEEDSREBASELINE', 'NEEDSMANUALREBASELINE']
        for expected in results['expected'].split():
            for actual in results['actual'].split():
                if expected in test_expectation_types and actual in failure_types:
                    expectations.add('Failure')
                if expected in failure_types and actual in test_expectation_types:
                    expectations.add(actual.capitalize())
                if expected in test_expectation_types and actual in test_expectation_types:
                    expectations.add(actual.capitalize())
        return expectations

    def create_line_list(self, merged_results):
        """Creates list of test expectations lines.

        Traverses through a merged_results and parses the value to create a test
        expectations line per key.

        Args:
            merged_results: A merged_results with the format {
                'test_name': {
                    'platform': {
                        'expected: 'PASS',
                        'actual': 'FAIL',
                        'bug': 'crbug.com/11111'
                    }
                }
            }
                It is possible for the dicitonary to have many test_name
                keys.

        Returns:
            A list of test expectations lines with the format
            ['BUG_URL [PLATFORM(S)] TEST_MAME [EXPECTATION(S)]']
        """
        line_list = []
        for test_name, platform_results in merged_results.iteritems():
            for platform in platform_results:
                if test_name.startswith('imported'):
                    platform_list = []
                    bug = []
                    expectations = []
                    if isinstance(platform, tuple):
                        platform_list = list(platform)
                    else:
                        platform_list.append(platform)
                    bug.append(platform_results[platform]['bug'])
                    expectations = self.get_expectations(platform_results[platform])
                    line = '%s [ %s ] %s [ %s ]' % (bug[0], ' '.join(platform_list), test_name, ' '.join(expectations))
                    line_list.append(str(line))
        return line_list

    def write_to_test_expectations(self, host, path, line_list):
        """Writes to TestExpectations.

        Writes to the test expectations lines in line_list
        to LayoutTest/TestExpectations. Checks the file for the string
        '# Tests added from W3C auto import bot' and writes expectation
        lines directly under it. If not found, it writes to the end of
        the file. If the test name is already in LayoutTests/TestExpectations,
        the line will be skipped.

        Args:
            host: A Host object.
            path: The path to the file LayoutTests/TestExpectations.
            line_list: A list of w3c test expectations lines.

        Returns:
            Writes to a file on the filesystem called LayoutTests/TestExpectations.
        """
        comment_line = '# Tests added from W3C auto import bot'
        file_contents = host.filesystem.read_text_file(path)
        w3c_comment_line_index = file_contents.find(comment_line)
        all_lines = ''
        for line in line_list:
            end_bracket_index = line.split().index(']')
            test_name = line.split()[end_bracket_index + 1]
            if test_name in file_contents:
                continue
            all_lines += str(line) + '\n'
        all_lines = all_lines[:-1]
        if w3c_comment_line_index == -1:
            file_contents += '\n%s\n' % comment_line
            file_contents += all_lines
        else:
            end_of_comment_line = (file_contents[w3c_comment_line_index:].find('\n')) + w3c_comment_line_index
            new_data = file_contents[: end_of_comment_line + 1] + all_lines + file_contents[end_of_comment_line:]
            file_contents = new_data
        host.filesystem.write_text_file(path, file_contents)

    def get_expected_txt_files(self, tests_results):
        """Gets -expected.txt files.

        Invokes webkit-patch rebaseline-from-try-jobs in order
        to download new -expected.txt files for testharness.js
        tests that did not Crash or Timeout. Then the platform-
        specific test is removed from the overall failure test dictionary.

        Args:
            tests_results: A dictionary that maps test name to platforms to
            test results.

        Returns:
            An updated tests_results dictionary without the platform-specific test-
            harness.js tests that required new baselines to be downloaded from
            webkit-patch rebaseline-from-try-jobs.
        """
        finder = WebKitFinder(self._host.filesystem)
        tests = self._host.executive.run_command(['git', 'diff', 'master', '--name-only']).splitlines()
        tests_to_rebaseline, tests_results = self.get_tests_to_rebaseline(finder, tests, tests_results)
        if tests_to_rebaseline:
            webkit_patch = self._host.filesystem.join(finder.chromium_base(), finder.webkit_base(),
                                                      finder.path_to_script('webkit-patch'))
            self._host.executive.run_command(['python', webkit_patch,
                                              'rebaseline-from-try-jobs', '-v'] + tests_to_rebaseline)
        return tests_results

    def get_tests_to_rebaseline(self, webkit_finder, tests, tests_results):
        """Gets test to rebaseline.

        Creates a list of tests to rebaseline depending on the tests' platform specific
        results. This function also removes the platform key form the tests_results
        dictionary if 'actual' results not in ['CRASH', 'TIMEOUT'].

        Args:
            webkit_finder: A WebKitFinder object.
            tests: A list of new imported tests.
            tests_results: A dictionary of failing tests results.

        Returns:
            A set of tests to be rebaselined. These tests are both
            js tests and test that failed during the try jobs. Also
            returns an updated tests_results dictionary.
        """
        tests_to_rebaseline = set()
        layout_tests_rel_path = self._host.filesystem.relpath(webkit_finder.layout_tests_dir(), webkit_finder.chromium_base())
        for test in tests:
            test_path = self._host.filesystem.relpath(test, layout_tests_rel_path)
            if self.is_js_test(webkit_finder, test) and tests_results.get(test_path):
                for platform in tests_results[test_path].keys():
                    if tests_results[test_path][platform]['actual'] not in ['CRASH', 'TIMEOUT']:
                        del tests_results[test_path][platform]
                        tests_to_rebaseline.add(test_path)
        return list(tests_to_rebaseline), tests_results

    def is_js_test(self, webkit_finder, test_path):
        absolute_path = self._host.filesystem.join(webkit_finder.chromium_base(), test_path)
        test_parser = TestParser(absolute_path, self._host)
        return test_parser.is_jstest()
