// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_FAKE_BLUETOOTH_INSTANCE_H_
#define COMPONENTS_ARC_TEST_FAKE_BLUETOOTH_INSTANCE_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "components/arc/common/bluetooth.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace arc {

class FakeBluetoothInstance : public mojom::BluetoothInstance {
 public:
  class GattDBResult {
   public:
    GattDBResult(mojom::BluetoothAddressPtr&& remote_addr,
                 mojo::Array<mojom::BluetoothGattDBElementPtr>&& db);
    ~GattDBResult();

    const mojom::BluetoothAddressPtr& remote_addr() const {
      return remote_addr_;
    }

    const mojo::Array<mojom::BluetoothGattDBElementPtr>& db() const {
      return db_;
    }

   private:
    mojom::BluetoothAddressPtr remote_addr_;
    mojo::Array<mojom::BluetoothGattDBElementPtr> db_;

    DISALLOW_COPY_AND_ASSIGN(GattDBResult);
  };

  class LEDeviceFoundData {
   public:
    LEDeviceFoundData(
        mojom::BluetoothAddressPtr&& addr,
        int32_t rssi,
        mojo::Array<mojom::BluetoothAdvertisingDataPtr>&& adv_data);
    ~LEDeviceFoundData();

    const mojom::BluetoothAddressPtr& addr() const { return addr_; }

    int32_t rssi() const { return rssi_; }

    const mojo::Array<mojom::BluetoothAdvertisingDataPtr>& adv_data() const {
      return adv_data_;
    }

   private:
    mojom::BluetoothAddressPtr addr_;
    int32_t rssi_;
    mojo::Array<mojom::BluetoothAdvertisingDataPtr> adv_data_;

    DISALLOW_COPY_AND_ASSIGN(LEDeviceFoundData);
  };

  FakeBluetoothInstance();
  ~FakeBluetoothInstance() override;

  // interface BluetoothInstance
  void Init(mojom::BluetoothHostPtr host_ptr) override;
  void OnAdapterProperties(
      mojom::BluetoothStatus status,
      mojo::Array<mojom::BluetoothPropertyPtr> properties) override;
  void OnRemoteDeviceProperties(
      mojom::BluetoothStatus status,
      mojom::BluetoothAddressPtr address,
      mojo::Array<mojom::BluetoothPropertyPtr> properties) override;
  void OnDeviceFound(
      mojo::Array<mojom::BluetoothPropertyPtr> properties) override;
  void OnDiscoveryStateChanged(mojom::BluetoothDiscoveryState state) override;
  void OnBondStateChanged(mojom::BluetoothStatus status,
                          mojom::BluetoothAddressPtr remote_addr,
                          mojom::BluetoothBondState state) override;
  void OnAclStateChanged(mojom::BluetoothStatus status,
                         mojom::BluetoothAddressPtr remote_addr,
                         mojom::BluetoothAclState state) override;
  void OnLEDeviceFound(
      mojom::BluetoothAddressPtr addr,
      int32_t rssi,
      mojo::Array<mojom::BluetoothAdvertisingDataPtr> adv_data) override;
  void OnLEConnectionStateChange(mojom::BluetoothAddressPtr remote_addr,
                                 bool connected) override;
  void OnSearchComplete(mojom::BluetoothAddressPtr remote_addr,
                        mojom::BluetoothGattStatus status) override;
  void OnGetGattDB(mojom::BluetoothAddressPtr remote_addr,
                   mojo::Array<mojom::BluetoothGattDBElementPtr> db) override;
  void OnServicesRemoved(mojom::BluetoothAddressPtr remote_addr,
                         uint16_t start_handle,
                         uint16_t end_handle) override;
  void OnServicesAdded(
      mojom::BluetoothAddressPtr remote_addr,
      mojo::Array<mojom::BluetoothGattDBElementPtr> db) override;

  void OnGattNotify(mojom::BluetoothAddressPtr remote_addr,
                    mojom::BluetoothGattServiceIDPtr service_id,
                    mojom::BluetoothGattIDPtr char_id,
                    bool is_notify,
                    mojo::Array<uint8_t> value) override;

  void RequestGattRead(mojom::BluetoothAddressPtr address,
                       int32_t attribute_handle,
                       int32_t offset,
                       bool is_long,
                       const RequestGattReadCallback& callback) override;

  void RequestGattWrite(mojom::BluetoothAddressPtr address,
                        int32_t attribute_handle,
                        int32_t offset,
                        mojo::Array<uint8_t> value,
                        const RequestGattWriteCallback& callback) override;

  const std::vector<mojo::Array<mojom::BluetoothPropertyPtr>>&
  device_found_data() const {
    return device_found_data_;
  }

  const std::vector<std::unique_ptr<LEDeviceFoundData>>& le_device_found_data()
      const {
    return le_device_found_data_;
  }

  const std::vector<std::unique_ptr<GattDBResult>>& gatt_db_result() const {
    return gatt_db_result_;
  }

 private:
  std::vector<mojo::Array<mojom::BluetoothPropertyPtr>> device_found_data_;
  std::vector<std::unique_ptr<LEDeviceFoundData>> le_device_found_data_;
  std::vector<std::unique_ptr<GattDBResult>> gatt_db_result_;

  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothInstance);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_FAKE_BLUETOOTH_INSTANCE_H_
