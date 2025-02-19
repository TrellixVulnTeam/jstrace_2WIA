// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/cast_sys_info_dummy.h"

namespace chromecast {

CastSysInfoDummy::CastSysInfoDummy()
    : build_type_(BUILD_ENG),
      serial_number_("dummy.serial.number"),
      product_name_("dummy product"),
      device_model_("dummy model"),
      factory_country_("US"),
      factory_locale_("en-US") {
}

CastSysInfoDummy::~CastSysInfoDummy() {
}

CastSysInfo::BuildType CastSysInfoDummy::GetBuildType() {
  return build_type_;
}

std::string CastSysInfoDummy::GetSystemReleaseChannel() {
  return system_release_channel_;
}

std::string CastSysInfoDummy::GetSerialNumber() {
  return serial_number_;
}

std::string CastSysInfoDummy::GetProductName() {
  return product_name_;
}

std::string CastSysInfoDummy::GetDeviceModel() {
  return device_model_;
}

std::string CastSysInfoDummy::GetBoardName() {
  return board_name_;
}

std::string CastSysInfoDummy::GetBoardRevision() {
  return board_revision_;
}

std::string CastSysInfoDummy::GetManufacturer() {
  return manufacturer_;
}

std::string CastSysInfoDummy::GetSystemBuildNumber() {
  return system_build_number_;
}

std::string CastSysInfoDummy::GetFactoryCountry() {
  return factory_country_;
}

std::string CastSysInfoDummy::GetFactoryLocale(std::string* second_locale) {
  return factory_locale_;
}

std::string CastSysInfoDummy::GetWifiInterface() {
  return wifi_interface_;
}

std::string CastSysInfoDummy::GetApInterface() {
  return ap_interface_;
}

std::string CastSysInfoDummy::GetGlVendor() {
  return gl_vendor_;
}

std::string CastSysInfoDummy::GetGlRenderer() {
  return gl_renderer_;
}

std::string CastSysInfoDummy::GetGlVersion() {
  return gl_version_;
}

void CastSysInfoDummy::SetBuildTypeForTesting(
    CastSysInfo::BuildType build_type) {
  build_type_ = build_type;
}

void CastSysInfoDummy::SetSystemReleaseChannelForTesting(
    const std::string& system_release_channel) {
  system_release_channel_ = system_release_channel;
}

void CastSysInfoDummy::SetSerialNumberForTesting(
    const std::string& serial_number) {
  serial_number_ = serial_number;
}

void CastSysInfoDummy::SetProductNameForTesting(
    const std::string& product_name) {
  product_name_ = product_name;
}

void CastSysInfoDummy::SetDeviceModelForTesting(
    const std::string& device_model) {
  device_model_ = device_model;
}

void CastSysInfoDummy::SetBoardNameForTesting(const std::string& board_name) {
  board_name_ = board_name;
}

void CastSysInfoDummy::SetBoardRevisionForTesting(
    const std::string& board_revision) {
  board_revision_ = board_revision;
}

void CastSysInfoDummy::SetManufacturerForTesting(
    const std::string& manufacturer) {
  manufacturer_ = manufacturer;
}

void CastSysInfoDummy::SetSystemBuildNumberForTesting(
    const std::string& system_build_number) {
  system_build_number_ = system_build_number;
}

void CastSysInfoDummy::SetFactoryCountryForTesting(
    const std::string& factory_country) {
  factory_country_ = factory_country;
}

void CastSysInfoDummy::SetFactoryLocaleForTesting(
    const std::string& factory_locale) {
  factory_locale_ = factory_locale;
}

void CastSysInfoDummy::SetWifiInterfaceForTesting(
    const std::string& wifi_interface) {
  wifi_interface_ = wifi_interface_;
}

void CastSysInfoDummy::SetApInterfaceForTesting(
    const std::string& ap_interface) {
  ap_interface_ = ap_interface;
}

void CastSysInfoDummy::SetGlVendorForTesting(const std::string& gl_vendor) {
  gl_vendor_ = gl_vendor;
}

void CastSysInfoDummy::SetGlRendererForTesting(const std::string& gl_renderer) {
  gl_renderer_ = gl_renderer;
}

void CastSysInfoDummy::SetGlVersionForTesting(const std::string& gl_version) {
  gl_version_ = gl_version;
}

}  // namespace chromecast
