// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/system/reboot/reboot_util.h"

#include "base/logging.h"
#include "chromecast/public/reboot_shlib.h"

// This is a partial implementation of the reboot_util.h interface.
// The remaining methods are defined in other reboot_util_*.cc depending
// on which platform/product they are for.

namespace chromecast {

// static
void RebootUtil::Initialize(const std::vector<std::string>& argv) {
  RebootShlib::Initialize(argv);
}

// static
void RebootUtil::Finalize() {
  RebootShlib::Finalize();
}

// static
bool RebootUtil::IsRebootSupported() {
  return RebootShlib::IsSupported();
}

// static
bool RebootUtil::IsValidRebootSource(RebootShlib::RebootSource reboot_source) {
  switch (reboot_source) {
    case RebootShlib::RebootSource::UNKNOWN:
    case RebootShlib::RebootSource::FORCED:
    case RebootShlib::RebootSource::API:
    case RebootShlib::RebootSource::NIGHTLY:
    case RebootShlib::RebootSource::OTA:
    case RebootShlib::RebootSource::WATCHDOG:
    case RebootShlib::RebootSource::PROCESS_MANAGER:
    case RebootShlib::RebootSource::CRASH_UPLOADER:
    case RebootShlib::RebootSource::FDR:
      return true;
    default:
      return false;
  }
}

// static
bool RebootUtil::IsRebootSourceSupported(
    RebootShlib::RebootSource reboot_source) {
  return RebootShlib::IsSupported() &&
         RebootShlib::IsRebootSourceSupported(reboot_source);
}

// static
bool RebootUtil::RebootNow(RebootShlib::RebootSource reboot_source) {
  DCHECK(IsRebootSourceSupported(reboot_source));
  SetLastRebootSource(reboot_source);
  return RebootShlib::RebootNow(reboot_source);
}

// static
bool RebootUtil::IsFdrForNextRebootSupported() {
  return RebootShlib::IsSupported() &&
         RebootShlib::IsFdrForNextRebootSupported();
}

// static
void RebootUtil::SetFdrForNextReboot() {
  DCHECK(IsFdrForNextRebootSupported());
  RebootShlib::SetFdrForNextReboot();
}

// static
bool RebootUtil::IsOtaForNextRebootSupported() {
  return RebootShlib::IsSupported() &&
         RebootShlib::IsOtaForNextRebootSupported();
}

// static
void RebootUtil::SetOtaForNextReboot() {
  DCHECK(IsOtaForNextRebootSupported());
  RebootShlib::SetOtaForNextReboot();
}

}  // namespace chromecast
