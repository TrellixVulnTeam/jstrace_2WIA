// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/tts_android.h"

#include <string>

#include "base/android/context_utils.h"
#include "base/android/jni_string.h"
#include "base/memory/singleton.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/speech/tts_controller.h"
#include "chrome/common/features.h"
#include "jni/TtsPlatformImpl_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;

// static
TtsPlatformImpl* TtsPlatformImpl::GetInstance() {
  return TtsPlatformImplAndroid::GetInstance();
}

TtsPlatformImplAndroid::TtsPlatformImplAndroid()
    : utterance_id_(0) {
  JNIEnv* env = AttachCurrentThread();
  java_ref_.Reset(
      Java_TtsPlatformImpl_create(env,
                                  reinterpret_cast<intptr_t>(this),
                                  base::android::GetApplicationContext()));
}

TtsPlatformImplAndroid::~TtsPlatformImplAndroid() {
  JNIEnv* env = AttachCurrentThread();
  Java_TtsPlatformImpl_destroy(env, java_ref_.obj());
}

bool TtsPlatformImplAndroid::PlatformImplAvailable() {
  return true;
}

bool TtsPlatformImplAndroid::Speak(
    int utterance_id,
    const std::string& utterance,
    const std::string& lang,
    const VoiceData& voice,
    const UtteranceContinuousParameters& params) {
  JNIEnv* env = AttachCurrentThread();
  jboolean success = Java_TtsPlatformImpl_speak(
      env, java_ref_.obj(), utterance_id,
      base::android::ConvertUTF8ToJavaString(env, utterance).obj(),
      base::android::ConvertUTF8ToJavaString(env, lang).obj(), params.rate,
      params.pitch, params.volume);
  if (!success)
    return false;

  utterance_ = utterance;
  utterance_id_ = utterance_id;
  return true;
}

bool TtsPlatformImplAndroid::StopSpeaking() {
  JNIEnv* env = AttachCurrentThread();
  Java_TtsPlatformImpl_stop(env, java_ref_.obj());
  utterance_id_ = 0;
  utterance_.clear();
  return true;
}

void TtsPlatformImplAndroid::Pause() {
  StopSpeaking();
}

void TtsPlatformImplAndroid::Resume() {
}

bool TtsPlatformImplAndroid::IsSpeaking() {
  return (utterance_id_ != 0);
}

void TtsPlatformImplAndroid::GetVoices(
    std::vector<VoiceData>* out_voices) {
  JNIEnv* env = AttachCurrentThread();
  if (!Java_TtsPlatformImpl_isInitialized(env, java_ref_.obj()))
    return;

  int count = Java_TtsPlatformImpl_getVoiceCount(env, java_ref_.obj());
  for (int i = 0; i < count; ++i) {
    out_voices->push_back(VoiceData());
    VoiceData& data = out_voices->back();
    data.native = true;
    data.name = base::android::ConvertJavaStringToUTF8(
        Java_TtsPlatformImpl_getVoiceName(env, java_ref_.obj(), i));
    data.lang = base::android::ConvertJavaStringToUTF8(
        Java_TtsPlatformImpl_getVoiceLanguage(env, java_ref_.obj(), i));
    data.gender = TTS_GENDER_NONE;
    data.events.insert(TTS_EVENT_START);
    data.events.insert(TTS_EVENT_END);
    data.events.insert(TTS_EVENT_ERROR);
  }
}

void TtsPlatformImplAndroid::VoicesChanged(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj) {
  TtsController::GetInstance()->VoicesChanged();
}

void TtsPlatformImplAndroid::OnEndEvent(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj,
                                        jint utterance_id) {
  SendFinalTtsEvent(utterance_id, TTS_EVENT_END,
                    static_cast<int>(utterance_.size()));
}

void TtsPlatformImplAndroid::OnErrorEvent(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj,
                                          jint utterance_id) {
  SendFinalTtsEvent(utterance_id, TTS_EVENT_ERROR, 0);
}

void TtsPlatformImplAndroid::OnStartEvent(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj,
                                          jint utterance_id) {
  if (utterance_id != utterance_id_)
    return;

  TtsController::GetInstance()->OnTtsEvent(
      utterance_id_, TTS_EVENT_START, 0, std::string());
}

void TtsPlatformImplAndroid::SendFinalTtsEvent(
    int utterance_id, TtsEventType event_type, int char_index) {
  if (utterance_id != utterance_id_)
    return;

  TtsController::GetInstance()->OnTtsEvent(
      utterance_id_, event_type, char_index, std::string());
  utterance_id_ = 0;
  utterance_.clear();
}

// static
TtsPlatformImplAndroid* TtsPlatformImplAndroid::GetInstance() {
  return base::Singleton<
      TtsPlatformImplAndroid,
      base::LeakySingletonTraits<TtsPlatformImplAndroid>>::get();
}

// static
bool TtsPlatformImplAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
