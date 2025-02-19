// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "mojo/common/traits_test_service.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace common {
namespace {

class StructTraitsTest : public testing::Test, public mojom::TraitsTestService {
 public:
  StructTraitsTest() {}

 protected:
  mojom::TraitsTestServicePtr GetTraitsTestProxy() {
    return traits_test_bindings_.CreateInterfacePtrAndBind(this);
  }

 private:
  // TraitsTestService:
  void EchoVersion(const base::Version& m,
                   const EchoVersionCallback& callback) override {
    callback.Run(m);
  }

  base::MessageLoop loop_;
  mojo::BindingSet<TraitsTestService> traits_test_bindings_;

  DISALLOW_COPY_AND_ASSIGN(StructTraitsTest);
};

TEST_F(StructTraitsTest, Version) {
  const std::string& version_str = "1.2.3.4";
  base::Version input(version_str);
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  base::Version output;
  proxy->EchoVersion(input, &output);
  base::Version test_version(version_str);

  EXPECT_EQ(version_str, output.GetString());
}

}  // namespace
}  // namespace common
}  // namespace mojo
