// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_H_

#include <list>
#include <string>

struct BrowserInfo;
class ChromeDesktopImpl;
class Status;
class WebView;

class Chrome {
 public:
  virtual ~Chrome() {}

  virtual Status GetAsDesktop(ChromeDesktopImpl** desktop) = 0;

  virtual const BrowserInfo* GetBrowserInfo() const = 0;

  virtual bool HasCrashedWebView() = 0;

  // Return ids of opened WebViews. The list is not guaranteed to be in the same
  // order as those WebViews are opened, if two or more new windows are opened
  // between two calls of this method.
  virtual Status GetWebViewIds(std::list<std::string>* web_view_ids) = 0;

  // Return the WebView for the given id.
  virtual Status GetWebViewById(const std::string& id, WebView** web_view) = 0;

  // Closes the specified WebView.
  virtual Status CloseWebView(const std::string& id) = 0;

  // Activates the specified WebView.
  virtual Status ActivateWebView(const std::string& id) = 0;

  // Get the operation system where Chrome is running.
  virtual std::string GetOperatingSystemName() = 0;

  // Return whether the mobileEmulation capability has been enabled.
  virtual bool IsMobileEmulationEnabled() const = 0;

  // Return whether the target device has a touchscreen, and whether touch
  // actions can be performed on it.
  virtual bool HasTouchScreen() const = 0;

  virtual std::string page_load_strategy() const = 0;

  virtual void set_page_load_strategy(std::string strategy) = 0;

  // Quits Chrome.
  virtual Status Quit() = 0;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_H_
