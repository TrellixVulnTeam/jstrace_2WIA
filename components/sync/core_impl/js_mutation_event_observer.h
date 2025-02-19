// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_CORE_IMPL_JS_MUTATION_EVENT_OBSERVER_H_
#define COMPONENTS_SYNC_CORE_IMPL_JS_MUTATION_EVENT_OBSERVER_H_

#include <stdint.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "components/sync/base/sync_export.h"
#include "components/sync/base/weak_handle.h"
#include "components/sync/core/sync_manager.h"
#include "components/sync/syncable/transaction_observer.h"

namespace tracked_objects {
class Location;
}  // namespace tracked_objects

namespace syncer {

class JsEventDetails;
class JsEventHandler;

// Observes all change- and transaction-related events and routes a
// summarized version to a JsEventHandler.
class SYNC_EXPORT JsMutationEventObserver
    : public SyncManager::ChangeObserver,
      public syncable::TransactionObserver,
      public base::NonThreadSafe {
 public:
  JsMutationEventObserver();

  ~JsMutationEventObserver() override;

  base::WeakPtr<JsMutationEventObserver> AsWeakPtr();

  void InvalidateWeakPtrs();

  void SetJsEventHandler(const WeakHandle<JsEventHandler>& event_handler);

  // SyncManager::ChangeObserver implementation.
  void OnChangesApplied(ModelType model_type,
                        int64_t write_transaction_id,
                        const ImmutableChangeRecordList& changes) override;
  void OnChangesComplete(ModelType model_type) override;

  // syncable::TransactionObserver implementation.
  void OnTransactionWrite(
      const syncable::ImmutableWriteTransactionInfo& write_transaction_info,
      ModelTypeSet models_with_changes) override;

 private:
  WeakHandle<JsEventHandler> event_handler_;

  void HandleJsEvent(const tracked_objects::Location& from_here,
                     const std::string& name,
                     const JsEventDetails& details);

  base::WeakPtrFactory<JsMutationEventObserver> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(JsMutationEventObserver);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_CORE_IMPL_JS_MUTATION_EVENT_OBSERVER_H_
