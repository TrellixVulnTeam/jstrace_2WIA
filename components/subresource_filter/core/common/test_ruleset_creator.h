// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_TEST_RULESET_CREATOR_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_TEST_RULESET_CREATOR_H_

#include <stdint.h>

#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"

namespace subresource_filter {
namespace testing {

// Encapsulates a testing subresource filtering ruleset serialized either in
// indexed or unindexed format. The ruleset |contents| can be accessed directly
// as a byte buffer, as well as through the file |path| pointing to a temporary
// file that is cleaned up when the TestRulesetCreator is destroyed.
struct TestRuleset {
  TestRuleset();
  ~TestRuleset();

  // Convenience function to open a read-only file handle to |ruleset|.
  static base::File Open(const TestRuleset& ruleset);

  std::vector<uint8_t> contents;
  base::FilePath path;
};

// Encapsulates the same ruleset in both indexed and unindexed formats.
struct TestRulesetPair {
  TestRulesetPair();
  ~TestRulesetPair();

  TestRuleset unindexed;
  TestRuleset indexed;
};

// Helper class to create subresource filtering rulesets for testing.
//
// All temporary files and paths are cleaned up when the instance goes out of
// scope, but file handles already open can still be used and read even after
// this has happened.
class TestRulesetCreator {
 public:
  TestRulesetCreator();
  ~TestRulesetCreator();

  // Creates a testing ruleset comprised of a single filtering rule that
  // disallows subresource loads from URL paths having the given |suffix|.
  // Enclose call in ASSERT_NO_FATAL_FAILURE to detect errors.
  void CreateRulesetToDisallowURLsWithPathSuffix(
      base::StringPiece suffix,
      TestRulesetPair* test_ruleset_pair);

  // Returns a unique |path| that is valid for the lifetime of this instance.
  // No file at |path| will be automatically created.
  void GetUniqueTemporaryPath(base::FilePath* path);

 private:
  // Writes the |ruleset_contents| to a temporary file, and initializes
  // |ruleset| to have the same |contents|, and the |path| to this file.
  void CreateTestRulesetFromContents(std::vector<uint8_t> ruleset_contents,
                                     TestRuleset* ruleset);

  base::ScopedTempDir scoped_temp_dir_;
  int next_unique_file_suffix = 1;

  DISALLOW_COPY_AND_ASSIGN(TestRulesetCreator);
};

}  // namespace testing
}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_TEST_RULESET_CREATOR_H_
