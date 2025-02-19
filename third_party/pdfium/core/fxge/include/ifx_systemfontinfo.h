// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_INCLUDE_IFX_SYSTEMFONTINFO_H_
#define CORE_FXGE_INCLUDE_IFX_SYSTEMFONTINFO_H_

#include "core/fxge/include/cfx_fontmapper.h"
#include "core/fxge/include/fx_font.h"

const uint32_t kTableNAME = FXDWORD_GET_MSBFIRST("name");
const uint32_t kTableTTCF = FXDWORD_GET_MSBFIRST("ttcf");

class IFX_SystemFontInfo {
 public:
  static std::unique_ptr<IFX_SystemFontInfo> CreateDefault(
      const char** pUserPaths);

  virtual ~IFX_SystemFontInfo() {}

  virtual FX_BOOL EnumFontList(CFX_FontMapper* pMapper) = 0;
  virtual void* MapFont(int weight,
                        FX_BOOL bItalic,
                        int charset,
                        int pitch_family,
                        const FX_CHAR* face,
                        int& iExact) = 0;

#ifdef PDF_ENABLE_XFA
  virtual void* MapFontByUnicode(uint32_t dwUnicode,
                                 int weight,
                                 FX_BOOL bItalic,
                                 int pitch_family);
#endif  // PDF_ENABLE_XFA

  virtual void* GetFont(const FX_CHAR* face) = 0;
  virtual uint32_t GetFontData(void* hFont,
                               uint32_t table,
                               uint8_t* buffer,
                               uint32_t size) = 0;
  virtual FX_BOOL GetFaceName(void* hFont, CFX_ByteString& name) = 0;
  virtual FX_BOOL GetFontCharset(void* hFont, int& charset) = 0;
  virtual int GetFaceIndex(void* hFont);
  virtual void DeleteFont(void* hFont) = 0;
  virtual void* RetainFont(void* hFont);
};

#endif  // CORE_FXGE_INCLUDE_IFX_SYSTEMFONTINFO_H_
