// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_update_infobar_delegate_android.h"

#include <memory>

#include "base/android/jni_array.h"
#include "base/callback_helpers.h"
#include "chrome/browser/android/preferences/pref_service_bridge.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/web_contents.h"
#include "jni/PermissionUpdateInfoBarDelegate_jni.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::JavaParamRef;

// static
infobars::InfoBar* PermissionUpdateInfoBarDelegate::Create(
    content::WebContents* web_contents,
    const std::vector<ContentSettingsType>& content_settings_types,
    const PermissionUpdatedCallback& callback) {
  DCHECK(ShouldShowPermissionInfobar(web_contents, content_settings_types))
      << "Caller should check ShouldShowPermissionInfobar before creating the "
      << "infobar.";

  content::ContentViewCore* cvc =
      content::ContentViewCore::FromWebContents(web_contents);
  ui::WindowAndroid* window_android = cvc->GetWindowAndroid();

  std::vector<std::string> permissions;
  int message_id = IDS_INFOBAR_MISSING_MULTIPLE_PERMISSIONS_TEXT;

  for (ContentSettingsType content_settings_type : content_settings_types) {
    std::string android_permission =
        PrefServiceBridge::GetAndroidPermissionForContentSetting(
            content_settings_type);

    if (!android_permission.empty() &&
        !window_android->HasPermission(android_permission)) {
      permissions.push_back(android_permission);

      if (content_settings_type == CONTENT_SETTINGS_TYPE_GEOLOCATION) {
          message_id = IDS_INFOBAR_MISSING_LOCATION_PERMISSION_TEXT;
      } else if (content_settings_type ==
                 CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC) {
          message_id = IDS_INFOBAR_MISSING_MICROPHONE_PERMISSION_TEXT;
      } else if (content_settings_type ==
                 CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA) {
          message_id = IDS_INFOBAR_MISSING_CAMERA_PERMISSION_TEXT;
      } else {
          NOTREACHED();
          message_id = IDS_INFOBAR_MISSING_MULTIPLE_PERMISSIONS_TEXT;
      }
    }
  }
  if (permissions.size() > 1)
    message_id = IDS_INFOBAR_MISSING_MULTIPLE_PERMISSIONS_TEXT;

  return PermissionUpdateInfoBarDelegate::Create(
      web_contents, permissions, message_id, callback);
}

// static
infobars::InfoBar* PermissionUpdateInfoBarDelegate::Create(
    content::WebContents* web_contents,
    const std::vector<std::string>& android_permissions,
    int permission_msg_id,
    const PermissionUpdatedCallback& callback) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  if (!infobar_service) {
    callback.Run(false);
    return nullptr;
  }

  return infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
      std::unique_ptr<ConfirmInfoBarDelegate>(
          new PermissionUpdateInfoBarDelegate(web_contents, android_permissions,
                                              permission_msg_id, callback))));
}

// static
bool PermissionUpdateInfoBarDelegate::ShouldShowPermissionInfobar(
    content::WebContents* web_contents,
    const std::vector<ContentSettingsType>& content_settings_types) {
  if (!web_contents)
    return false;

  content::ContentViewCore* cvc =
      content::ContentViewCore::FromWebContents(web_contents);
  if (!cvc || !cvc->GetWindowAndroid())
    return false;
  ui::WindowAndroid* window_android = cvc->GetWindowAndroid();

  for (ContentSettingsType content_settings_type : content_settings_types) {
    std::string android_permission =
        PrefServiceBridge::GetAndroidPermissionForContentSetting(
            content_settings_type);

    if (!android_permission.empty() &&
        !window_android->HasPermission(android_permission)) {
      return true;
    }
  }

  return false;
}

// static
bool PermissionUpdateInfoBarDelegate::RegisterPermissionUpdateInfoBarDelegate(
    JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void PermissionUpdateInfoBarDelegate::OnPermissionResult(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean all_permissions_granted) {
  base::ResetAndReturn(&callback_).Run(all_permissions_granted);
  infobar()->RemoveSelf();
}

PermissionUpdateInfoBarDelegate::PermissionUpdateInfoBarDelegate(
    content::WebContents* web_contents,
    const std::vector<std::string>& android_permissions,
    int permission_msg_id,
    const PermissionUpdatedCallback& callback)
    : ConfirmInfoBarDelegate(),
      android_permissions_(android_permissions),
      permission_msg_id_(permission_msg_id),
      callback_(callback) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_delegate_.Reset(Java_PermissionUpdateInfoBarDelegate_create(
      env,
      reinterpret_cast<intptr_t>(this),
      web_contents->GetJavaWebContents().obj(),
      base::android::ToJavaArrayOfStrings(env, android_permissions_).obj()));
}

PermissionUpdateInfoBarDelegate::~PermissionUpdateInfoBarDelegate() {
  Java_PermissionUpdateInfoBarDelegate_onNativeDestroyed(
      base::android::AttachCurrentThread(), java_delegate_.obj());
}

infobars::InfoBarDelegate::InfoBarIdentifier
PermissionUpdateInfoBarDelegate::GetIdentifier() const {
  return PERMISSION_UPDATE_INFOBAR_DELEGATE;
}

int PermissionUpdateInfoBarDelegate::GetIconId() const {
  return IDR_INFOBAR_WARNING;
}

base::string16 PermissionUpdateInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(permission_msg_id_);
}

int PermissionUpdateInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

base::string16 PermissionUpdateInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK_EQ(button, BUTTON_OK);
  return l10n_util::GetStringUTF16(IDS_INFOBAR_UPDATE_PERMISSIONS_BUTTON_TEXT);
}

bool PermissionUpdateInfoBarDelegate::Accept() {
  Java_PermissionUpdateInfoBarDelegate_requestPermissions(
      base::android::AttachCurrentThread(), java_delegate_.obj());
  return false;
}

bool PermissionUpdateInfoBarDelegate::Cancel() {
  base::ResetAndReturn(&callback_).Run(false);
  return true;
}

void PermissionUpdateInfoBarDelegate::InfoBarDismissed() {
  base::ResetAndReturn(&callback_).Run(false);
}
