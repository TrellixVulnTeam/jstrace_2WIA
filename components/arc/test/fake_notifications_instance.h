// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_FAKE_NOTIFICATIONS_INSTANCE_H_
#define COMPONENTS_ARC_TEST_FAKE_NOTIFICATIONS_INSTANCE_H_

#include <utility>
#include <vector>

#include "components/arc/common/notifications.mojom.h"
#include "components/arc/test/fake_arc_bridge_instance.h"

namespace arc {

class FakeNotificationsInstance : public mojom::NotificationsInstance {
 public:
  FakeNotificationsInstance();
  ~FakeNotificationsInstance() override;

  void Init(mojom::NotificationsHostPtr host_ptr) override;

  void SendNotificationEventToAndroid(
      const mojo::String& key,
      mojom::ArcNotificationEvent event) override;

  const std::vector<std::pair<mojo::String, mojom::ArcNotificationEvent>>&
  events() const;

 private:
  std::vector<std::pair<mojo::String, mojom::ArcNotificationEvent>> events_;

  DISALLOW_COPY_AND_ASSIGN(FakeNotificationsInstance);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_FAKE_NOTIFICATIONS_INSTANCE_H_
