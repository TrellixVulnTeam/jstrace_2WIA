// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BANNERS_APP_BANNER_INFOBAR_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_ANDROID_BANNERS_APP_BANNER_INFOBAR_DELEGATE_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/android/banners/app_banner_data_fetcher_android.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "content/public/common/manifest.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace infobars {
class InfoBarManager;
}  // namespace infobars

class AppBannerInfoBar;

namespace banners {

// Manages installation of an app being promoted by a webpage.
class AppBannerInfoBarDelegateAndroid : public ConfirmInfoBarDelegate {
 public:
  // Delegate for promoting a web app.
  AppBannerInfoBarDelegateAndroid(
      int event_request_id,
      scoped_refptr<AppBannerDataFetcherAndroid> data_fetcher,
      const base::string16& app_title,
      const GURL& app_icon_url,
      SkBitmap* app_icon,
      const GURL& manifest_url,
      const content::Manifest& manifest);

  // Delegate for promoting an Android app.
  AppBannerInfoBarDelegateAndroid(
      int event_request_id,
      const base::string16& app_title,
      SkBitmap* app_icon,
      const base::android::ScopedJavaGlobalRef<jobject>& native_app_data,
      const std::string& native_app_package,
      const std::string& referrer);

  ~AppBannerInfoBarDelegateAndroid() override;

  // Called when the AppBannerInfoBar's button needs to be updated.
  void UpdateInstallState(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj);

  // Called when the installation Intent has been handled and focus has been
  // returned to Chrome.
  void OnInstallIntentReturned(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj,
                               jboolean jis_installing);

  // Called when the InstallerDelegate task has finished.
  void OnInstallFinished(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         jboolean success);

 private:
  void CreateJavaDelegate();
  void SendBannerAccepted(content::WebContents* web_contents,
                          const std::string& platform);

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  gfx::Image GetIcon() const override;
  void InfoBarDismissed() override;
  base::string16 GetMessageText() const override;
  int GetButtons() const override;
  bool Accept() override;
  bool LinkClicked(WindowOpenDisposition disposition) override;

  base::android::ScopedJavaGlobalRef<jobject> java_delegate_;

  // Used to fetch the splash screen icon for webapps.
  scoped_refptr<AppBannerDataFetcherAndroid> data_fetcher_;

  base::string16 app_title_;
  GURL app_icon_url_;
  std::unique_ptr<SkBitmap> app_icon_;

  int event_request_id_;
  GURL manifest_url_;
  content::Manifest manifest_;

  base::android::ScopedJavaGlobalRef<jobject> native_app_data_;
  std::string native_app_package_;
  std::string referrer_;
  bool has_user_interaction_;

  DISALLOW_COPY_AND_ASSIGN(AppBannerInfoBarDelegateAndroid);
};  // AppBannerInfoBarDelegateAndroid

// Register native methods.
bool RegisterAppBannerInfoBarDelegateAndroid(JNIEnv* env);

}  // namespace banners

#endif  // CHROME_BROWSER_ANDROID_BANNERS_APP_BANNER_INFOBAR_DELEGATE_ANDROID_H_
