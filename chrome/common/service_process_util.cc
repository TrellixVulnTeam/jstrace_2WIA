// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/service_process_util.h"

#include <stdint.h>

#include <algorithm>
#include <memory>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "base/sha1.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "components/cloud_devices/common/cloud_devices_switches.h"
#include "components/version_info/version_info.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "google_apis/gaia/gaia_switches.h"
#include "ui/base/ui_base_switches.h"

#if !defined(OS_MACOSX)

namespace {

// This should be more than enough to hold a version string assuming each part
// of the version string is an int64_t.
const uint32_t kMaxVersionStringLength = 256;

// The structure that gets written to shared memory.
struct ServiceProcessSharedData {
  char service_process_version[kMaxVersionStringLength];
  base::ProcessId service_process_pid;
};

// Gets the name of the shared memory used by the service process to write its
// version. The name is not versioned.
std::string GetServiceProcessSharedMemName() {
  return GetServiceProcessScopedName("_service_shmem");
}

enum ServiceProcessRunningState {
  SERVICE_NOT_RUNNING,
  SERVICE_OLDER_VERSION_RUNNING,
  SERVICE_SAME_VERSION_RUNNING,
  SERVICE_NEWER_VERSION_RUNNING,
};

ServiceProcessRunningState GetServiceProcessRunningState(
    std::string* service_version_out, base::ProcessId* pid_out) {
  std::string version;
  if (!GetServiceProcessData(&version, pid_out))
    return SERVICE_NOT_RUNNING;

#if defined(OS_POSIX)
  // We only need to check for service running on POSIX because Windows cleans
  // up shared memory files when an app crashes, so there isn't a chance of
  // us reading bogus data from shared memory for an app that has died.
  if (!CheckServiceProcessReady()) {
    return SERVICE_NOT_RUNNING;
  }
#endif  // defined(OS_POSIX)

  // At this time we have a version string. Set the out param if it exists.
  if (service_version_out)
    *service_version_out = version;

  Version service_version(version);
  // If the version string is invalid, treat it like an older version.
  if (!service_version.IsValid())
    return SERVICE_OLDER_VERSION_RUNNING;

  // Get the version of the currently *running* instance of Chrome.
  Version running_version(version_info::GetVersionNumber());
  if (!running_version.IsValid()) {
    NOTREACHED() << "Failed to parse version info";
    // Our own version is invalid. This is an error case. Pretend that we
    // are out of date.
    return SERVICE_NEWER_VERSION_RUNNING;
  }

  if (running_version.CompareTo(service_version) > 0) {
    return SERVICE_OLDER_VERSION_RUNNING;
  } else if (service_version.CompareTo(running_version) > 0) {
    return SERVICE_NEWER_VERSION_RUNNING;
  }
  return SERVICE_SAME_VERSION_RUNNING;
}

}  // namespace


// Return a name that is scoped to this instance of the service process. We
// use the user-data-dir and the version as a scoping prefix.
std::string GetServiceProcessScopedVersionedName(
    const std::string& append_str) {
  std::string versioned_str;
  versioned_str.append(version_info::GetVersionNumber());
  versioned_str.append(append_str);
  return GetServiceProcessScopedName(versioned_str);
}

// Reads the named shared memory to get the shared data. Returns false if no
// matching shared memory was found.
bool GetServiceProcessData(std::string* version, base::ProcessId* pid) {
  std::unique_ptr<base::SharedMemory> shared_mem_service_data;
  shared_mem_service_data.reset(new base::SharedMemory());
  ServiceProcessSharedData* service_data = NULL;
  if (shared_mem_service_data.get() &&
      shared_mem_service_data->Open(GetServiceProcessSharedMemName(), true) &&
      shared_mem_service_data->Map(sizeof(ServiceProcessSharedData))) {
    service_data = reinterpret_cast<ServiceProcessSharedData*>(
        shared_mem_service_data->memory());
    // Make sure the version in shared memory is null-terminated. If it is not,
    // treat it as invalid.
    if (version && memchr(service_data->service_process_version, '\0',
                          sizeof(service_data->service_process_version)))
      *version = service_data->service_process_version;
    if (pid)
      *pid = service_data->service_process_pid;
    return true;
  }
  return false;
}

#endif  // !OS_MACOSX

// Return a name that is scoped to this instance of the service process. We
// use the hash of the user-data-dir as a scoping prefix. We can't use
// the user-data-dir itself as we have limits on the size of the lock names.
std::string GetServiceProcessScopedName(const std::string& append_str) {
  base::FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
#if defined(OS_WIN)
  std::string user_data_dir_path = base::WideToUTF8(user_data_dir.value());
#elif defined(OS_POSIX)
  std::string user_data_dir_path = user_data_dir.value();
#endif  // defined(OS_WIN)
  std::string hash = base::SHA1HashString(user_data_dir_path);
  std::string hex_hash = base::HexEncode(hash.c_str(), hash.length());
  return hex_hash + "." + append_str;
}

std::unique_ptr<base::CommandLine> CreateServiceProcessCommandLine() {
  base::FilePath exe_path;
  PathService::Get(content::CHILD_PROCESS_EXE, &exe_path);
  DCHECK(!exe_path.empty()) << "Unable to get service process binary name.";
  std::unique_ptr<base::CommandLine> command_line(
      new base::CommandLine(exe_path));
  command_line->AppendSwitchASCII(switches::kProcessType,
                                  switches::kServiceProcess);

#if defined(OS_WIN)
  command_line->AppendArg(switches::kPrefetchArgumentOther);
#endif  // defined(OS_WIN)

  static const char* const kSwitchesToCopy[] = {
    switches::kCloudPrintSetupProxy,
    switches::kCloudPrintURL,
    switches::kCloudPrintXmppEndpoint,
#if defined(OS_WIN)
    switches::kEnableCloudPrintXps,
#endif
    switches::kEnableLogging,
    switches::kIgnoreUrlFetcherCertRequests,
    switches::kLang,
    switches::kLoggingLevel,
    switches::kLsoUrl,
    switches::kNoServiceAutorun,
    switches::kUserDataDir,
    switches::kV,
    switches::kVModule,
    switches::kWaitForDebugger,
  };

  command_line->CopySwitchesFrom(*base::CommandLine::ForCurrentProcess(),
                                 kSwitchesToCopy,
                                 arraysize(kSwitchesToCopy));
  return command_line;
}

ServiceProcessState::ServiceProcessState() : state_(NULL) {
  autorun_command_line_ = CreateServiceProcessCommandLine();
  CreateState();
}

ServiceProcessState::~ServiceProcessState() {
#if !defined(OS_MACOSX)
  if (shared_mem_service_data_.get()) {
    shared_mem_service_data_->Delete(GetServiceProcessSharedMemName());
  }
#endif  // !OS_MACOSX
  TearDownState();
}

void ServiceProcessState::SignalStopped() {
  TearDownState();
  shared_mem_service_data_.reset();
}

#if !defined(OS_MACOSX)
bool ServiceProcessState::Initialize() {
  if (!TakeSingletonLock()) {
    return false;
  }
  // Now that we have the singleton, take care of killing an older version, if
  // it exists.
  if (!HandleOtherVersion())
    return false;

  // Write the version we are using to shared memory. This can be used by a
  // newer service to signal us to exit.
  return CreateSharedData();
}

bool ServiceProcessState::HandleOtherVersion() {
  std::string running_version;
  base::ProcessId process_id = 0;
  ServiceProcessRunningState state =
      GetServiceProcessRunningState(&running_version, &process_id);
  switch (state) {
    case SERVICE_SAME_VERSION_RUNNING:
    case SERVICE_NEWER_VERSION_RUNNING:
      return false;
    case SERVICE_OLDER_VERSION_RUNNING:
      // If an older version is running, kill it.
      ForceServiceProcessShutdown(running_version, process_id);
      break;
    case SERVICE_NOT_RUNNING:
      break;
  }
  return true;
}

bool ServiceProcessState::CreateSharedData() {
  if (version_info::GetVersionNumber().length() >= kMaxVersionStringLength) {
    NOTREACHED() << "Version string length is << "
                 << version_info::GetVersionNumber().length()
                 << " which is longer than" << kMaxVersionStringLength;
    return false;
  }

  std::unique_ptr<base::SharedMemory> shared_mem_service_data(
      new base::SharedMemory());
  if (!shared_mem_service_data.get())
    return false;

  uint32_t alloc_size = sizeof(ServiceProcessSharedData);
  // TODO(viettrungluu): Named shared memory is deprecated (crbug.com/345734).
  if (!shared_mem_service_data->CreateNamedDeprecated
          (GetServiceProcessSharedMemName(), true, alloc_size))
    return false;

  if (!shared_mem_service_data->Map(alloc_size))
    return false;

  memset(shared_mem_service_data->memory(), 0, alloc_size);
  ServiceProcessSharedData* shared_data =
      reinterpret_cast<ServiceProcessSharedData*>(
          shared_mem_service_data->memory());
  memcpy(shared_data->service_process_version,
         version_info::GetVersionNumber().c_str(),
         version_info::GetVersionNumber().length());
  shared_data->service_process_pid = base::GetCurrentProcId();
  shared_mem_service_data_.reset(shared_mem_service_data.release());
  return true;
}

IPC::ChannelHandle ServiceProcessState::GetServiceProcessChannel() {
  return ::GetServiceProcessChannel();
}

#endif  // !OS_MACOSX
