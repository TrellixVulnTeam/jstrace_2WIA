// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/javascript/JS_Object.h"

#include "fpdfsdk/include/fsdk_mgr.h"
#include "fpdfsdk/javascript/JS_Define.h"
#include "fpdfsdk/javascript/cjs_context.h"

CJS_EmbedObj::CJS_EmbedObj(CJS_Object* pJSObject) : m_pJSObject(pJSObject) {}

CJS_EmbedObj::~CJS_EmbedObj() {
  m_pJSObject = nullptr;
}

void FreeObject(const v8::WeakCallbackInfo<CJS_Object>& data) {
  CJS_Object* pJSObj = data.GetParameter();
  pJSObj->ExitInstance();
  delete pJSObj;
  FXJS_FreePrivate(data.GetInternalField(0));
}

void DisposeObject(const v8::WeakCallbackInfo<CJS_Object>& data) {
  CJS_Object* pJSObj = data.GetParameter();
  pJSObj->Dispose();
  data.SetSecondPassCallback(FreeObject);
}

CJS_Object::CJS_Object(v8::Local<v8::Object> pObject) {
  m_pIsolate = pObject->GetIsolate();
  m_pV8Object.Reset(m_pIsolate, pObject);
}

CJS_Object::~CJS_Object() {}

void CJS_Object::MakeWeak() {
  m_pV8Object.SetWeak(this, DisposeObject,
                      v8::WeakCallbackType::kInternalFields);
}

void CJS_Object::Dispose() {
  m_pV8Object.Reset();
}

void CJS_Object::InitInstance(IJS_Runtime* pIRuntime) {}

void CJS_Object::ExitInstance() {}
