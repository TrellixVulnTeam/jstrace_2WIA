// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_EVENTS_GET_UPDATES_RESPONSE_EVENT_H_
#define COMPONENTS_SYNC_ENGINE_EVENTS_GET_UPDATES_RESPONSE_EVENT_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/sync/base/sync_export.h"
#include "components/sync/base/syncer_error.h"
#include "components/sync/engine/events/protocol_event.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

// An event representing a GetUpdates response event from the server.
//
// Unlike the events for the request message, the response events are generic
// and do not vary for each type of GetUpdate cycle.
class SYNC_EXPORT GetUpdatesResponseEvent : public ProtocolEvent {
 public:
  GetUpdatesResponseEvent(base::Time timestamp,
                          const sync_pb::ClientToServerResponse& response,
                          SyncerError error);

  ~GetUpdatesResponseEvent() override;

  base::Time GetTimestamp() const override;
  std::string GetType() const override;
  std::string GetDetails() const override;
  std::unique_ptr<base::DictionaryValue> GetProtoMessage() const override;
  std::unique_ptr<ProtocolEvent> Clone() const override;

 private:
  const base::Time timestamp_;
  const sync_pb::ClientToServerResponse response_;
  const SyncerError error_;

  DISALLOW_COPY_AND_ASSIGN(GetUpdatesResponseEvent);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_EVENTS_GET_UPDATES_RESPONSE_EVENT_H_
