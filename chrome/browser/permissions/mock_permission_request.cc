// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/mock_permission_request.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/theme_resources.h"

MockPermissionRequest::MockPermissionRequest()
    : MockPermissionRequest("test",
                            "button",
                            "button",
                            GURL("http://www.google.com"),
                            PermissionRequestType::UNKNOWN,
                            PermissionRequestGestureType::UNKNOWN) {}

MockPermissionRequest::MockPermissionRequest(
    const std::string& text)
    : MockPermissionRequest(text,
                            "button",
                            "button",
                            GURL("http://www.google.com"),
                            PermissionRequestType::UNKNOWN,
                            PermissionRequestGestureType::UNKNOWN) {}

MockPermissionRequest::MockPermissionRequest(
    const std::string& text,
    PermissionRequestType request_type,
    PermissionRequestGestureType gesture_type)
    : MockPermissionRequest(text,
                            "button",
                            "button",
                             GURL("http://www.google.com"),
                             request_type,
                             gesture_type) {}

MockPermissionRequest::MockPermissionRequest(
    const std::string& text,
    const GURL& url)
    : MockPermissionRequest(text,
                            "button",
                            "button",
                            url,
                            PermissionRequestType::UNKNOWN,
                            PermissionRequestGestureType::UNKNOWN) {}

MockPermissionRequest::MockPermissionRequest(
    const std::string& text,
    const std::string& accept_label,
    const std::string& deny_label)
    : MockPermissionRequest(text,
                            accept_label,
                            deny_label,
                            GURL("http://www.google.com"),
                            PermissionRequestType::UNKNOWN,
                            PermissionRequestGestureType::UNKNOWN) {}

MockPermissionRequest::~MockPermissionRequest() {}

int MockPermissionRequest::GetIconId() const {
  // Use a valid icon ID to support UI tests.
  return IDR_INFOBAR_MEDIA_STREAM_CAMERA;
}

base::string16 MockPermissionRequest::GetMessageTextFragment() const {
  return text_;
}

GURL MockPermissionRequest::GetOrigin() const {
  return origin_;
}

void MockPermissionRequest::PermissionGranted() {
  granted_ = true;
}

void MockPermissionRequest::PermissionDenied() {
  granted_ = false;
}

void MockPermissionRequest::Cancelled() {
  granted_ = false;
  cancelled_ = true;
}

void MockPermissionRequest::RequestFinished() {
  finished_ = true;
}

PermissionRequestType MockPermissionRequest::GetPermissionRequestType()
    const {
  return request_type_;
}

PermissionRequestGestureType MockPermissionRequest::GetGestureType()
    const {
  return gesture_type_;
}

bool MockPermissionRequest::granted() {
  return granted_;
}

bool MockPermissionRequest::cancelled() {
  return cancelled_;
}

bool MockPermissionRequest::finished() {
  return finished_;
}

MockPermissionRequest::MockPermissionRequest(
    const std::string& text,
    const std::string& accept_label,
    const std::string& deny_label,
    const GURL& origin,
    PermissionRequestType request_type,
    PermissionRequestGestureType gesture_type)
    : granted_(false),
      cancelled_(false),
      finished_(false),
      request_type_(request_type),
      gesture_type_(gesture_type) {
  text_ = base::UTF8ToUTF16(text);
  accept_label_ = base::UTF8ToUTF16(accept_label);
  deny_label_ = base::UTF8ToUTF16(deny_label);
  origin_ = origin.GetOrigin();
}
