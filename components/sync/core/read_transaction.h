// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_CORE_READ_TRANSACTION_H_
#define COMPONENTS_SYNC_CORE_READ_TRANSACTION_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/sync/api/attachments/attachment_id.h"
#include "components/sync/base/sync_export.h"
#include "components/sync/core/base_transaction.h"

namespace tracked_objects {
class Location;
}  // namespace tracked_objects

namespace sync_pb {
class DataTypeContext;
}

namespace syncer {

struct UserShare;

// Sync API's ReadTransaction is a read-only BaseTransaction.  It wraps
// a syncable::ReadTransaction.
class SYNC_EXPORT ReadTransaction : public BaseTransaction {
 public:
  // Start a new read-only transaction on the specified repository.
  ReadTransaction(const tracked_objects::Location& from_here, UserShare* share);

  // Resume the middle of a transaction. Will not close transaction.
  ReadTransaction(UserShare* share, syncable::BaseTransaction* trans);

  ~ReadTransaction() override;

  // BaseTransaction override.
  syncable::BaseTransaction* GetWrappedTrans() const override;

  // Return |transaction_version| of |type| stored in sync directory's
  // persisted info.
  int64_t GetModelVersion(ModelType type) const;

  // Fills |context| with the datatype context associated with |type|.
  void GetDataTypeContext(ModelType type,
                          sync_pb::DataTypeContext* context) const;

  // Clear |ids| and fill it with the ids of attachments that need to be
  // uploaded to the sync server.
  void GetAttachmentIdsToUpload(ModelType type, AttachmentIdList* ids) const;

  // Return the current (opaque) store birthday.
  std::string GetStoreBirthday() const;

 private:
  void* operator new(size_t size);  // Transaction is meant for stack use only.

  // The underlying syncable object which this class wraps.
  syncable::BaseTransaction* transaction_;
  bool close_transaction_;

  DISALLOW_COPY_AND_ASSIGN(ReadTransaction);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_CORE_READ_TRANSACTION_H_
