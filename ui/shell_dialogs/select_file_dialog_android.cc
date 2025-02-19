// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "select_file_dialog_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "jni/SelectFileDialog_jni.h"
#include "ui/android/window_android.h"
#include "ui/shell_dialogs/selected_file_info.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace ui {

// static
SelectFileDialogImpl* SelectFileDialogImpl::Create(Listener* listener,
                                                   SelectFilePolicy* policy) {
  return new SelectFileDialogImpl(listener, policy);
}

void SelectFileDialogImpl::OnFileSelected(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_object,
    const JavaParamRef<jstring>& filepath,
    const JavaParamRef<jstring>& display_name) {
  if (!listener_)
    return;

  std::string path = ConvertJavaStringToUTF8(env, filepath);
  std::string file_name = ConvertJavaStringToUTF8(env, display_name);
  base::FilePath file_path = base::FilePath(path);
  ui::SelectedFileInfo file_info;
  file_info.file_path = file_path;
  file_info.local_path = file_path;
  if (!file_name.empty())
    file_info.display_name = file_name;

  listener_->FileSelectedWithExtraInfo(file_info, 0, NULL);
}

void SelectFileDialogImpl::OnMultipleFilesSelected(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_object,
    const JavaParamRef<jobjectArray>& filepaths,
    const JavaParamRef<jobjectArray>& display_names) {
  if (!listener_)
    return;

  std::vector<ui::SelectedFileInfo> selected_files;

  jsize length = env->GetArrayLength(filepaths);
  DCHECK(length == env->GetArrayLength(display_names));
  for (int i = 0; i < length; ++i) {
    std::string path = ConvertJavaStringToUTF8(
        env, static_cast<jstring>(env->GetObjectArrayElement(filepaths, i)));
    std::string display_name = ConvertJavaStringToUTF8(
        env,
        static_cast<jstring>(env->GetObjectArrayElement(display_names, i)));

    base::FilePath file_path = base::FilePath(path);

    ui::SelectedFileInfo file_info;
    file_info.file_path = file_path;
    file_info.local_path = file_path;
    file_info.display_name = display_name;

    selected_files.push_back(file_info);
  }

  listener_->MultiFilesSelectedWithExtraInfo(selected_files, NULL);
}

void SelectFileDialogImpl::OnFileNotSelected(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_object) {
  if (listener_)
    listener_->FileSelectionCanceled(NULL);
}

bool SelectFileDialogImpl::IsRunning(gfx::NativeWindow) const {
  return listener_;
}

void SelectFileDialogImpl::ListenerDestroyed() {
  listener_ = NULL;
}

void SelectFileDialogImpl::SelectFileImpl(
    SelectFileDialog::Type type,
    const base::string16& title,
    const base::FilePath& default_path,
    const SelectFileDialog::FileTypeInfo* file_types,
    int file_type_index,
    const std::string& default_extension,
    gfx::NativeWindow owning_window,
    void* params) {
  JNIEnv* env = base::android::AttachCurrentThread();

  // The first element in the pair is a list of accepted types, the second
  // indicates whether the device's capture capabilities should be used.
  typedef std::pair<std::vector<base::string16>, bool> AcceptTypes;
  AcceptTypes accept_types = std::make_pair(std::vector<base::string16>(),
                                            false);

  if (params)
    accept_types = *(reinterpret_cast<AcceptTypes*>(params));

  ScopedJavaLocalRef<jobjectArray> accept_types_java =
      base::android::ToJavaArrayOfStrings(env, accept_types.first);

  bool accept_multiple_files = SelectFileDialog::SELECT_OPEN_MULTI_FILE == type;

  Java_SelectFileDialog_selectFile(env, java_object_.obj(),
                                   accept_types_java.obj(),
                                   accept_types.second,
                                   accept_multiple_files,
                                   owning_window->GetJavaObject().obj());
}

bool SelectFileDialogImpl::RegisterSelectFileDialog(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

SelectFileDialogImpl::~SelectFileDialogImpl() {
}

SelectFileDialogImpl::SelectFileDialogImpl(Listener* listener,
                                           SelectFilePolicy* policy)
    : SelectFileDialog(listener, policy) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_object_.Reset(
      Java_SelectFileDialog_create(env, reinterpret_cast<intptr_t>(this)));
}

bool SelectFileDialogImpl::HasMultipleFileTypeChoicesImpl() {
  NOTIMPLEMENTED();
  return false;
}

SelectFileDialog* CreateSelectFileDialog(SelectFileDialog::Listener* listener,
                                         SelectFilePolicy* policy) {
  return SelectFileDialogImpl::Create(listener, policy);
}

}  // namespace ui
