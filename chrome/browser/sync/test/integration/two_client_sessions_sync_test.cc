// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/guid.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sessions_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/sync/sessions/sync_session_snapshot.h"

using passwords_helper::SetDecryptionPassphrase;
using passwords_helper::SetEncryptionPassphrase;
using sessions_helper::CheckInitialState;
using sessions_helper::DeleteForeignSession;
using sessions_helper::GetLocalWindows;
using sessions_helper::GetSessionData;
using sessions_helper::OpenTabAndGetLocalWindows;
using sessions_helper::ScopedWindowMap;
using sessions_helper::SessionWindowMap;
using sessions_helper::SyncedSessionVector;
using sessions_helper::WindowsMatch;

class TwoClientSessionsSyncTest : public SyncTest {
 public:
  TwoClientSessionsSyncTest() : SyncTest(TWO_CLIENT) {}
  ~TwoClientSessionsSyncTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientSessionsSyncTest);
};

static const char* kURL1 = "http://127.0.0.1/bubba1";
static const char* kURL2 = "http://127.0.0.1/bubba2";

// TODO(zea): Test each individual session command we care about separately.
// (as well as multi-window). We're currently only checking basic single-window/
// single-tab functionality.

// Fails on Win, see http://crbug.com/232313
#if defined(OS_WIN)
#define MAYBE_SingleClientChanged DISABLED_SingleClientChanged
#define MAYBE_BothChanged DISABLED_BothChanged
#define MAYBE_DeleteIdleSession DISABLED_DeleteIdleSession
#define MAYBE_AllChanged DISABLED_AllChanged
#else
#define MAYBE_SingleClientChanged SingleClientChanged
#define MAYBE_BothChanged BothChanged
#define MAYBE_DeleteIdleSession DeleteIdleSession
#define MAYBE_AllChanged AllChanged
#endif


IN_PROC_BROWSER_TEST_F(TwoClientSessionsSyncTest,
                       E2E_ENABLED(MAYBE_SingleClientChanged)) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Open tab and access a url on client 0
  SessionWindowMap client0_windows;
  std::string url = base::StringPrintf("http://127.0.0.1/bubba%s",
      base::GenerateGUID().c_str());
  ASSERT_TRUE(OpenTabAndGetLocalWindows(0, GURL(url), &client0_windows));

  // Retain the window information on client 0
  std::vector<ScopedWindowMap> expected_windows(1);
  expected_windows[0].Reset(&client0_windows);

  // Check the foreign windows on client 1
  ASSERT_TRUE(AwaitCheckForeignSessionsAgainst(1, expected_windows));
}

IN_PROC_BROWSER_TEST_F(TwoClientSessionsSyncTest,
                       E2E_ENABLED(MAYBE_AllChanged)) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Open tabs on all clients and retain window information.
  std::vector<ScopedWindowMap> client_windows(num_clients());
  for (int i = 0; i < num_clients(); ++i) {
    SessionWindowMap windows;
    std::string url = base::StringPrintf("http://127.0.0.1/bubba%s",
        base::GenerateGUID().c_str());
    ASSERT_TRUE(OpenTabAndGetLocalWindows(i, GURL(url), &windows));
    client_windows[i].Reset(&windows);
  }

  // Get foreign session data from all clients and check it against all
  // client_windows.
  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_TRUE(AwaitCheckForeignSessionsAgainst(i, client_windows));
  }
}

IN_PROC_BROWSER_TEST_F(TwoClientSessionsSyncTest,
                       SingleClientEnabledEncryption) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  ASSERT_TRUE(EnableEncryption(0));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(IsEncryptionComplete(0));
  ASSERT_TRUE(IsEncryptionComplete(1));
}

// This test is flaky on several platforms:
//    http://crbug.com/420979
//    http://crbug.com/421167
IN_PROC_BROWSER_TEST_F(TwoClientSessionsSyncTest,
                       DISABLED_SingleClientEnabledEncryptionAndChanged) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  ScopedWindowMap client0_windows;
  ASSERT_TRUE(OpenTabAndGetLocalWindows(0, GURL(kURL1),
      client0_windows.GetMutable()));
  ASSERT_TRUE(EnableEncryption(0));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  // Get foreign session data from client 1.
  ASSERT_TRUE(IsEncryptionComplete(1));
  SyncedSessionVector sessions1;
  ASSERT_TRUE(GetSessionData(1, &sessions1));

  // Verify client 1's foreign session matches client 0 current window.
  ASSERT_EQ(1U, sessions1.size());
  ASSERT_TRUE(WindowsMatch(sessions1[0]->windows, *client0_windows.Get()));
}

IN_PROC_BROWSER_TEST_F(TwoClientSessionsSyncTest,
                       BothClientsEnabledEncryption) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  ASSERT_TRUE(EnableEncryption(0));
  ASSERT_TRUE(EnableEncryption(1));
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(IsEncryptionComplete(0));
  ASSERT_TRUE(IsEncryptionComplete(1));
}

IN_PROC_BROWSER_TEST_F(TwoClientSessionsSyncTest, MAYBE_BothChanged) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  // Open tabs on both clients and retain window information.
  ScopedWindowMap client0_windows;
  ASSERT_TRUE(OpenTabAndGetLocalWindows(0, GURL(kURL2),
      client0_windows.GetMutable()));
  ScopedWindowMap client1_windows;
  ASSERT_TRUE(OpenTabAndGetLocalWindows(1, GURL(kURL1),
      client1_windows.GetMutable()));

  // Wait for sync.
  ASSERT_TRUE(AwaitQuiescence());

  // Get foreign session data from client 0 and 1.
  SyncedSessionVector sessions0;
  SyncedSessionVector sessions1;
  ASSERT_TRUE(GetSessionData(0, &sessions0));
  ASSERT_TRUE(GetSessionData(1, &sessions1));

  // Verify client 1's foreign session matches client 0's current window and
  // vice versa.
  ASSERT_EQ(1U, sessions0.size());
  ASSERT_EQ(1U, sessions1.size());
  ASSERT_TRUE(WindowsMatch(sessions1[0]->windows, *client0_windows.Get()));
  ASSERT_TRUE(WindowsMatch(sessions0[0]->windows, *client1_windows.Get()));
}

IN_PROC_BROWSER_TEST_F(TwoClientSessionsSyncTest, MAYBE_DeleteIdleSession) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  // Client 0 opened some tabs then went idle.
  ScopedWindowMap client0_windows;
  ASSERT_TRUE(OpenTabAndGetLocalWindows(0, GURL(kURL1),
      client0_windows.GetMutable()));

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  // Get foreign session data from client 1.
  SyncedSessionVector sessions1;
  ASSERT_TRUE(GetSessionData(1, &sessions1));

  // Verify client 1's foreign session matches client 0 current window.
  ASSERT_EQ(1U, sessions1.size());
  ASSERT_TRUE(WindowsMatch(sessions1[0]->windows, *client0_windows.Get()));

  // Client 1 now deletes client 0's tabs. This frees the memory of sessions1.
  DeleteForeignSession(1, sessions1[0]->session_tag);
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));
  ASSERT_FALSE(GetSessionData(1, &sessions1));
}

// Fails all release trybots. crbug.com/263369.
IN_PROC_BROWSER_TEST_F(TwoClientSessionsSyncTest,
                       DISABLED_DeleteActiveSession) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  // Client 0 opened some tabs then went idle.
  ScopedWindowMap client0_windows;
  ASSERT_TRUE(OpenTabAndGetLocalWindows(0, GURL(kURL1),
      client0_windows.GetMutable()));

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  SyncedSessionVector sessions1;
  ASSERT_TRUE(GetSessionData(1, &sessions1));
  ASSERT_EQ(1U, sessions1.size());
  ASSERT_TRUE(WindowsMatch(sessions1[0]->windows, *client0_windows.Get()));

  // Client 1 now deletes client 0's tabs. This frees the memory of sessions1.
  DeleteForeignSession(1, sessions1[0]->session_tag);
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));
  ASSERT_FALSE(GetSessionData(1, &sessions1));

  // Client 0 becomes active again with a new tab.
  ASSERT_TRUE(OpenTabAndGetLocalWindows(0, GURL(kURL2),
      client0_windows.GetMutable()));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(GetSessionData(1, &sessions1));
  ASSERT_EQ(1U, sessions1.size());
  ASSERT_TRUE(WindowsMatch(sessions1[0]->windows, *client0_windows.Get()));
}
