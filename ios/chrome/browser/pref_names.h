// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PREF_NAMES_H_
#define IOS_CHROME_BROWSER_PREF_NAMES_H_

namespace prefs {

extern const char kAcceptLanguages[];
extern const char kApplicationLocale[];
extern const char kBrowserStateInfoCache[];
extern const char kBrowserStateLastUsed[];
extern const char kBrowserStatesLastActive[];
extern const char kBrowserStatesNumCreated[];
extern const char kBrowsingDataMigrationHasBeenPossible[];
extern const char kClearBrowsingDataHistoryNoticeShownTimes[];
extern const char kContextualSearchEnabled[];
extern const char kDataSaverEnabled[];
extern const char kDefaultCharset[];
extern const char kEnableDoNotTrack[];
extern const char kHttpServerProperties[];
extern const char kIosBookmarkFolderDefault[];
extern const char kIosBookmarkPromoAlreadySeen[];
extern const char kIosHandoffToOtherDevices[];
extern const char kIosPhysicalWebEnabled[];
extern const char kLastSessionExitedCleanly[];
extern const char kLastSessionUsedWKWebViewControlGroup[];
extern const char kMetricsReportingWifiOnly[];
extern const char kNtpShownPage[];
extern const char kSavingBrowserHistoryDisabled[];
extern const char kSearchSuggestEnabled[];

// TODO(crbug.com/538573): Consider migrating from these two bools to an integer
// since only three cases are supported.
extern const char kNetworkPredictionEnabled[];
extern const char kNetworkPredictionWifiOnly[];

extern const char kNtpShownBookmarksFolder[];
extern const char kShowMemoryDebuggingTools[];

extern const char kVoiceSearchLocale[];
extern const char kVoiceSearchTTS[];

extern const char kSigninLastAccounts[];
extern const char kSigninLastAccountsMigrated[];
extern const char kSigninSharedAuthenticationUserId[];
extern const char kSigninShouldPromptForSigninAgain[];

extern const char kOmniboxGeolocationAuthorizationState[];
extern const char kOmniboxGeolocationLastAuthorizationAlertVersion[];

extern const char kRateThisAppDialogLastShownTime[];

}  // namespace prefs

#endif  // IOS_CHROME_BROWSER_PREF_NAMES_H_
