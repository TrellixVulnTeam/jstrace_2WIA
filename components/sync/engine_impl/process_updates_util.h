// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_PROCESS_UPDATES_UTIL_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_PROCESS_UPDATES_UTIL_H_

#include <stdint.h>

#include <vector>

#include "components/sync/base/model_type.h"

namespace sync_pb {
class SyncEntity;
}

namespace syncer {

namespace sessions {
class StatusController;
}

namespace syncable {
class ModelNeutralWriteTransaction;
class Directory;
}

struct UpdateCounters;

typedef std::vector<const sync_pb::SyncEntity*> SyncEntityList;

// Processes all the updates associated with a single ModelType.
void ProcessDownloadedUpdates(syncable::Directory* dir,
                              syncable::ModelNeutralWriteTransaction* trans,
                              ModelType type,
                              const SyncEntityList& applicable_updates,
                              sessions::StatusController* status,
                              UpdateCounters* counters);

// Tombstones all entries of |type| whose versions are older than
// |version_watermark| unless they are type root or unsynced/unapplied.
void ExpireEntriesByVersion(syncable::Directory* dir,
                            syncable::ModelNeutralWriteTransaction* trans,
                            ModelType type,
                            int64_t version_watermark);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_PROCESS_UPDATES_UTIL_H_
