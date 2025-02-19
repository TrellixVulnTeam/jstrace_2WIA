// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/unindexed_ruleset.h"

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "components/subresource_filter/core/common/proto/rules.pb.h"
#include "components/subresource_filter/core/common/url_pattern.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace subresource_filter {

namespace {

bool IsEqual(const proto::UrlRule& first, const proto::UrlRule& second) {
  // Note: The domain list is omitted for simplicity.
  return first.semantics() == second.semantics() &&
         first.source_type() == second.source_type() &&
         first.element_types() == second.element_types() &&
         first.activation_types() == second.activation_types() &&
         first.url_pattern_type() == second.url_pattern_type() &&
         first.anchor_left() == second.anchor_left() &&
         first.anchor_right() == second.anchor_right() &&
         first.match_case() == second.match_case() &&
         first.url_pattern() == second.url_pattern();
}

proto::UrlRule CreateRule(const UrlPattern& url_pattern,
                          proto::SourceType source_type,
                          bool is_whitelist) {
  proto::UrlRule rule;
  rule.set_semantics(is_whitelist ? proto::RULE_SEMANTICS_WHITELIST
                                  : proto::RULE_SEMANTICS_BLACKLIST);

  rule.set_source_type(source_type);
  rule.set_element_types(proto::ELEMENT_TYPE_ALL);

  rule.set_url_pattern_type(url_pattern.type);
  rule.set_anchor_left(url_pattern.anchor_left);
  rule.set_anchor_right(url_pattern.anchor_right);
  rule.set_match_case(url_pattern.match_case);
  rule.set_url_pattern(url_pattern.url_pattern.as_string());
  return rule;
}

// The helper class used for building UnindexedRulesets.
class UnindexedRulesetTestBuilder {
 public:
  // Initializes the builder that writes the ruleset to StringOutputStream.
  UnindexedRulesetTestBuilder()
      : output_(
            new google::protobuf::io::StringOutputStream(&ruleset_contents_)),
        ruleset_writer_(output_.get()) {}

  // Initializes the builder that writes the ruleset to an array of |array_size|
  // through an ArrayOutputStream.
  UnindexedRulesetTestBuilder(int array_size)
      : ruleset_contents_(array_size, '\0'),
        output_(
            new google::protobuf::io::ArrayOutputStream(&ruleset_contents_[0],
                                                        array_size)),
        ruleset_writer_(output_.get()) {}

  int max_rules_per_chunk() const {
    return ruleset_writer_.max_rules_per_chunk();
  }

  bool AddUrlRule(const UrlPattern& url_pattern,
                  proto::SourceType source_type,
                  bool is_whitelist) {
    url_rules_.push_back(CreateRule(url_pattern, source_type, is_whitelist));
    return !ruleset_writer_.had_error() &&
           ruleset_writer_.AddUrlRule(url_rules_.back());
  }

  bool AddUrlRules(int number_of_rules) {
    for (int i = 0; i < number_of_rules; ++i) {
      std::string url_pattern = "example" + base::IntToString(i) + ".com";
      if (!AddUrlRule(UrlPattern(url_pattern), proto::SOURCE_TYPE_ANY, i & 1))
        return false;
    }
    return true;
  }

  bool Finish() {
    if (!ruleset_writer_.had_error() && ruleset_writer_.Finish()) {
      // Note: This line has effect only when |output_| is an ArrayOutputStream.
      ruleset_contents_.resize(output_->ByteCount());
      return true;
    }
    return false;
  }

  const std::vector<proto::UrlRule>& url_rules() const { return url_rules_; }
  const std::string& ruleset_contents() const { return ruleset_contents_; }

 private:
  std::vector<proto::UrlRule> url_rules_;
  std::string ruleset_contents_;
  std::unique_ptr<google::protobuf::io::ZeroCopyOutputStream> output_;
  UnindexedRulesetWriter ruleset_writer_;

  DISALLOW_COPY_AND_ASSIGN(UnindexedRulesetTestBuilder);
};

bool IsRulesetValid(const std::string& ruleset_contents,
                    const std::vector<proto::UrlRule>& expected_url_rules) {
  google::protobuf::io::ArrayInputStream array_input(ruleset_contents.data(),
                                                     ruleset_contents.size());
  UnindexedRulesetReader reader(&array_input);
  proto::FilteringRules chunk;
  std::vector<proto::UrlRule> read_rules;
  while (reader.ReadNextChunk(&chunk)) {
    read_rules.insert(read_rules.end(), chunk.url_rules().begin(),
                      chunk.url_rules().end());
  }

  if (expected_url_rules.size() != read_rules.size())
    return false;
  for (size_t i = 0, size = read_rules.size(); i != size; ++i) {
    if (!IsEqual(expected_url_rules[i], read_rules[i]))
      return false;
  }
  return true;
}

}  // namespace

TEST(UnindexedRulesetTest, EmptyRuleset) {
  UnindexedRulesetTestBuilder builder;
  EXPECT_TRUE(builder.Finish());
  EXPECT_TRUE(IsRulesetValid(builder.ruleset_contents(), builder.url_rules()));
}

TEST(UnindexedRulesetTest, OneUrlRule) {
  UnindexedRulesetTestBuilder builder;
  EXPECT_TRUE(builder.AddUrlRule(UrlPattern("example.com"),
                                 proto::SOURCE_TYPE_THIRD_PARTY, false));
  EXPECT_TRUE(builder.Finish());
  EXPECT_TRUE(IsRulesetValid(builder.ruleset_contents(), builder.url_rules()));
}

TEST(UnindexedRulesetTest, ManyUrlRules) {
  UnindexedRulesetTestBuilder builder;
  EXPECT_TRUE(builder.AddUrlRules(1234));
  EXPECT_TRUE(builder.Finish());
  EXPECT_TRUE(IsRulesetValid(builder.ruleset_contents(), builder.url_rules()));
}

TEST(UnindexedRulesetTest, ExactlyMaxRulesPerChunk) {
  UnindexedRulesetTestBuilder builder;
  EXPECT_TRUE(builder.AddUrlRules(builder.max_rules_per_chunk()));
  EXPECT_TRUE(builder.Finish());
  EXPECT_TRUE(IsRulesetValid(builder.ruleset_contents(), builder.url_rules()));
}

TEST(UnindexedRulesetTest, MaxRulesPerChunkPlusOne) {
  UnindexedRulesetTestBuilder builder;
  EXPECT_TRUE(builder.AddUrlRules(builder.max_rules_per_chunk() + 1));
  EXPECT_TRUE(builder.Finish());
  EXPECT_TRUE(IsRulesetValid(builder.ruleset_contents(), builder.url_rules()));
}

TEST(UnindexedRulesetTest, ErrorOnWrite) {
  UnindexedRulesetTestBuilder builder(1000);
  EXPECT_FALSE(builder.AddUrlRules(1234));
}

TEST(UnindexedRulesetTest, ReadCorruptedInput) {
  UnindexedRulesetTestBuilder builder;
  EXPECT_TRUE(builder.AddUrlRules(1000));
  EXPECT_TRUE(builder.Finish());

  {
    std::string ruleset_contents = builder.ruleset_contents();
    ASSERT_GE(ruleset_contents.size(), static_cast<size_t>(2000));
    ruleset_contents[100] ^= 239;
    ruleset_contents[1000] ^= 3;
    EXPECT_FALSE(IsRulesetValid(ruleset_contents, builder.url_rules()));
  }

  {
    std::string ruleset_contents = builder.ruleset_contents();
    ASSERT_GT(ruleset_contents.size(), static_cast<size_t>(100));
    ruleset_contents.resize(100);
    EXPECT_FALSE(IsRulesetValid(ruleset_contents, builder.url_rules()));
  }
}

}  // namespace subresource_filter
