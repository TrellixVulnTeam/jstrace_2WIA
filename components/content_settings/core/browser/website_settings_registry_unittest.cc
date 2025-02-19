// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/website_settings_registry.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content_settings {

class WebsiteSettingsRegistryTest : public testing::Test {
 protected:
  WebsiteSettingsRegistry* registry() { return &registry_; }

 private:
  WebsiteSettingsRegistry registry_;
};

TEST_F(WebsiteSettingsRegistryTest, Get) {
  // CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE should be registered.
  const WebsiteSettingsInfo* info =
      registry()->Get(CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE);
  ASSERT_TRUE(info);
  EXPECT_EQ(CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE, info->type());
  EXPECT_EQ("auto-select-certificate", info->name());
}

TEST_F(WebsiteSettingsRegistryTest, GetByName) {
  // Random string shouldn't be registered.
  EXPECT_FALSE(registry()->GetByName("abc"));

  // "auto-select-certificate" should be registered.
  const WebsiteSettingsInfo* info =
      registry()->GetByName("auto-select-certificate");
  ASSERT_TRUE(info);
  EXPECT_EQ(CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE, info->type());
  EXPECT_EQ("auto-select-certificate", info->name());
  EXPECT_EQ(registry()->Get(CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE),
            info);

  // Register a new setting.
  registry()->Register(static_cast<ContentSettingsType>(10), "test", nullptr,
                       WebsiteSettingsInfo::UNSYNCABLE,
                       WebsiteSettingsInfo::LOSSY,
                       WebsiteSettingsInfo::TOP_LEVEL_ORIGIN_ONLY_SCOPE,
                       WebsiteSettingsRegistry::ALL_PLATFORMS,
                       WebsiteSettingsInfo::INHERIT_IN_INCOGNITO);
  info = registry()->GetByName("test");
  ASSERT_TRUE(info);
  EXPECT_EQ(10, info->type());
  EXPECT_EQ("test", info->name());
  EXPECT_EQ(registry()->Get(static_cast<ContentSettingsType>(10)), info);
}

TEST_F(WebsiteSettingsRegistryTest, GetPlatformDependent) {
#if defined(OS_IOS)
  // App banner shouldn't be registered on iOS.
  EXPECT_FALSE(registry()->Get(CONTENT_SETTINGS_TYPE_APP_BANNER));
#else
  // App banner should be registered on other platforms.
  EXPECT_TRUE(registry()->Get(CONTENT_SETTINGS_TYPE_APP_BANNER));
#endif

  // Auto select certificate is registered on all platforms.
  EXPECT_TRUE(registry()->Get(CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE));
}

TEST_F(WebsiteSettingsRegistryTest, Properties) {
  // "auto-select-certificate" should be registered.
  const WebsiteSettingsInfo* info =
      registry()->Get(CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE);
  ASSERT_TRUE(info);
  EXPECT_EQ("profile.content_settings.exceptions.auto_select_certificate",
            info->pref_name());
  EXPECT_EQ("profile.default_content_setting_values.auto_select_certificate",
            info->default_value_pref_name());
  ASSERT_FALSE(info->initial_default_value());
  EXPECT_EQ(PrefRegistry::NO_REGISTRATION_FLAGS,
            info->GetPrefRegistrationFlags());

  // Register a new setting.
  registry()->Register(static_cast<ContentSettingsType>(10), "test",
                       base::WrapUnique(new base::FundamentalValue(999)),
                       WebsiteSettingsInfo::SYNCABLE,
                       WebsiteSettingsInfo::LOSSY,
                       WebsiteSettingsInfo::TOP_LEVEL_ORIGIN_ONLY_SCOPE,
                       WebsiteSettingsRegistry::ALL_PLATFORMS,
                       WebsiteSettingsInfo::INHERIT_IN_INCOGNITO);
  info = registry()->Get(static_cast<ContentSettingsType>(10));
  ASSERT_TRUE(info);
  EXPECT_EQ("profile.content_settings.exceptions.test", info->pref_name());
  EXPECT_EQ("profile.default_content_setting_values.test",
            info->default_value_pref_name());
  int setting;
  ASSERT_TRUE(info->initial_default_value()->GetAsInteger(&setting));
  EXPECT_EQ(999, setting);
#if defined(OS_IOS)
  EXPECT_EQ(PrefRegistry::LOSSY_PREF, info->GetPrefRegistrationFlags());
#else
  EXPECT_EQ(PrefRegistry::LOSSY_PREF |
                user_prefs::PrefRegistrySyncable::SYNCABLE_PREF,
            info->GetPrefRegistrationFlags());
#endif
  EXPECT_EQ(WebsiteSettingsInfo::TOP_LEVEL_ORIGIN_ONLY_SCOPE,
            info->scoping_type());
  EXPECT_EQ(WebsiteSettingsInfo::INHERIT_IN_INCOGNITO,
            info->incognito_behavior());
}

TEST_F(WebsiteSettingsRegistryTest, Iteration) {
  registry()->Register(static_cast<ContentSettingsType>(10), "test",
                       base::WrapUnique(new base::FundamentalValue(999)),
                       WebsiteSettingsInfo::SYNCABLE,
                       WebsiteSettingsInfo::LOSSY,
                       WebsiteSettingsInfo::TOP_LEVEL_ORIGIN_ONLY_SCOPE,
                       WebsiteSettingsRegistry::ALL_PLATFORMS,
                       WebsiteSettingsInfo::INHERIT_IN_INCOGNITO);

  bool found = false;
  for (const WebsiteSettingsInfo* info : *registry()) {
    EXPECT_EQ(registry()->Get(info->type()), info);
    if (info->type() == 10) {
      EXPECT_FALSE(found);
      found = true;
    }
  }

  EXPECT_TRUE(found);
}

}  // namespace content_settings
