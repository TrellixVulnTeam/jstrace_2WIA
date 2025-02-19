#!/usr/bin/env python
# Copyright 2016 The LUCI Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0
# that can be found in the LICENSE file.

import os
import sys
import unittest
import StringIO

ROOT_DIR = os.path.dirname(os.path.abspath(os.path.join(
    __file__.decode(sys.getfilesystemencoding()),
    os.pardir, os.pardir, os.pardir)))
sys.path.insert(0, ROOT_DIR)

from libs.logdog import streamname


class StreamNameTestCase(unittest.TestCase):

  def testInvalidStreamNamesRaiseValueError(self):
    for name in (
        '',
        'a' * (streamname._MAX_STREAM_NAME_LENGTH+1),
        ' s p a c e s ',
        '-hyphen',
        'stream/path/+/not/name',
    ):
      with self.assertRaises(ValueError):
        streamname.validate_stream_name(name)

  def testValidStreamNamesDoNotRaise(self):
    for name in (
        'a',
        'a' * (streamname._MAX_STREAM_NAME_LENGTH),
        'foo/bar',
        'f123/four/five-_.:',
    ):
      raised = False
      try:
        streamname.validate_stream_name(name)
      except ValueError:
        raised = True
      self.assertFalse(raised, "Stream name '%s' raised ValueError" % (name,))


if __name__ == '__main__':
  unittest.main()
