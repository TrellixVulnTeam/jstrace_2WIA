// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_data_service_factory.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/sync_start_util.h"
#include "chrome/browser/ui/profile_error_dialog.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/search_engines/keyword_web_data_service.h"
#include "components/signin/core/browser/webdata/token_web_data.h"
#include "components/webdata_services/web_data_service_wrapper.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_WIN)
#include "components/password_manager/core/browser/webdata/password_web_data_service_win.h"
#endif

using content::BrowserThread;

namespace {

// Converts a WebDataServiceWrapper::ErrorType to ProfileErrorType.
ProfileErrorType ProfileErrorFromWebDataServiceWrapperError(
    WebDataServiceWrapper::ErrorType error_type) {
  switch (error_type) {
    case WebDataServiceWrapper::ERROR_LOADING_AUTOFILL:
      return PROFILE_ERROR_DB_AUTOFILL_WEB_DATA;

    case WebDataServiceWrapper::ERROR_LOADING_KEYWORD:
      return PROFILE_ERROR_DB_KEYWORD_WEB_DATA;

    case WebDataServiceWrapper::ERROR_LOADING_TOKEN:
      return PROFILE_ERROR_DB_TOKEN_WEB_DATA;

    case WebDataServiceWrapper::ERROR_LOADING_PASSWORD:
      return PROFILE_ERROR_DB_WEB_DATA;

    default:
      NOTREACHED()
          << "Unknown WebDataServiceWrapper::ErrorType: " << error_type;
      return PROFILE_ERROR_DB_WEB_DATA;
  }
}

// Callback to show error dialog on profile load error.
void ProfileErrorCallback(WebDataServiceWrapper::ErrorType error_type,
                          sql::InitStatus status,
                          const std::string& diagnostics) {
  ShowProfileErrorDialog(ProfileErrorFromWebDataServiceWrapperError(error_type),
                         (status == sql::INIT_FAILURE)
                             ? IDS_COULDNT_OPEN_PROFILE_ERROR
                             : IDS_PROFILE_TOO_NEW_ERROR,
                         diagnostics);
}

}  // namespace

WebDataServiceFactory::WebDataServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "WebDataService",
          BrowserContextDependencyManager::GetInstance()) {
  // WebDataServiceFactory has no dependecies.
}

WebDataServiceFactory::~WebDataServiceFactory() {
}

// static
WebDataServiceWrapper* WebDataServiceFactory::GetForProfile(
    Profile* profile,
    ServiceAccessType access_type) {
  // If |access_type| starts being used for anything other than this
  // DCHECK, we need to start taking it as a parameter to
  // the *WebDataService::FromBrowserContext() functions (see above).
  DCHECK(access_type != ServiceAccessType::IMPLICIT_ACCESS ||
         !profile->IsOffTheRecord());
  return static_cast<WebDataServiceWrapper*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
WebDataServiceWrapper* WebDataServiceFactory::GetForProfileIfExists(
    Profile* profile,
    ServiceAccessType access_type) {
  // If |access_type| starts being used for anything other than this
  // DCHECK, we need to start taking it as a parameter to
  // the *WebDataService::FromBrowserContext() functions (see above).
  DCHECK(access_type != ServiceAccessType::IMPLICIT_ACCESS ||
         !profile->IsOffTheRecord());
  return static_cast<WebDataServiceWrapper*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
scoped_refptr<autofill::AutofillWebDataService>
WebDataServiceFactory::GetAutofillWebDataForProfile(
    Profile* profile,
    ServiceAccessType access_type) {
  WebDataServiceWrapper* wrapper =
      WebDataServiceFactory::GetForProfile(profile, access_type);
  // |wrapper| can be null in Incognito mode.
  return wrapper ?
      wrapper->GetAutofillWebData() :
      scoped_refptr<autofill::AutofillWebDataService>(nullptr);
}

// static
scoped_refptr<KeywordWebDataService>
WebDataServiceFactory::GetKeywordWebDataForProfile(
    Profile* profile,
    ServiceAccessType access_type) {
  WebDataServiceWrapper* wrapper =
      WebDataServiceFactory::GetForProfile(profile, access_type);
  // |wrapper| can be null in Incognito mode.
  return wrapper ?
      wrapper->GetKeywordWebData() :
      scoped_refptr<KeywordWebDataService>(nullptr);
}

// static
scoped_refptr<TokenWebData> WebDataServiceFactory::GetTokenWebDataForProfile(
    Profile* profile,
    ServiceAccessType access_type) {
  WebDataServiceWrapper* wrapper =
      WebDataServiceFactory::GetForProfile(profile, access_type);
  // |wrapper| can be null in Incognito mode.
  return wrapper ?
      wrapper->GetTokenWebData() : scoped_refptr<TokenWebData>(nullptr);
}

#if defined(OS_WIN)
// static
scoped_refptr<PasswordWebDataService>
WebDataServiceFactory::GetPasswordWebDataForProfile(
    Profile* profile,
    ServiceAccessType access_type) {
  WebDataServiceWrapper* wrapper =
      WebDataServiceFactory::GetForProfile(profile, access_type);
  // |wrapper| can be null in Incognito mode.
  return wrapper ?
      wrapper->GetPasswordWebData() :
      scoped_refptr<PasswordWebDataService>(nullptr);
}
#endif

// static
WebDataServiceFactory* WebDataServiceFactory::GetInstance() {
  return base::Singleton<WebDataServiceFactory>::get();
}

content::BrowserContext* WebDataServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

KeyedService* WebDataServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  const base::FilePath& profile_path = context->GetPath();
  return new WebDataServiceWrapper(
      profile_path, g_browser_process->GetApplicationLocale(),
      BrowserThread::GetTaskRunnerForThread(BrowserThread::UI),
      BrowserThread::GetTaskRunnerForThread(BrowserThread::DB),
      sync_start_util::GetFlareForSyncableService(profile_path),
      &ProfileErrorCallback);
}

bool WebDataServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
