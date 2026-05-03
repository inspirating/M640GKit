/*
================================================================================
GATT 服务器 (C++ 版本 - Arduino BLE 库)
================================================================================

作为 GATT Server 运行, 模拟 M640G 泵设备

对应 Python: pump_manager/gatt_server.py
================================================================================
*/

#ifndef M640G_GATT_SERVER_H
#define M640G_GATT_SERVER_H

#include <Arduino.h>
#include <string>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLEAdvertising.h>
#include "../enums.h"
#include "../encryption/crc8.h"

namespace M640GKit {

// 回调函数类型定义
typedef void (*WriteRequestCallback)(const uint8_t* data, size_t len);
typedef void (*SubscribeCallback)(bool subscribed);

class GATTServerCallbacks : public BLEServerCallbacks, public BLECharacteristicCallbacks {
public:
    GATTServer* server;

    GATTServerCallbacks(GATTServer* srv) : server(srv) {}

    void onConnect(BLEServer* pServer) override;
    void onDisconnect(BLEServer* pServer) override;
    void onWrite(BLECharacteristic* pCharacteristic) override;
    void onSubscribe(BLECharacteristic* pCharacteristic, uint16_t subValue) override;
};

typedef void (*DisconnectCallback)();
typedef void (*ConnectCallback)();

class GATTServer {
public:
    bool isRunning = false;
    WriteRequestCallback onWriteRequest = nullptr;
    SubscribeCallback onSubscribe = nullptr;
    DisconnectCallback onDisconnect = nullptr;
    ConnectCallback onConnect = nullptr;

    GATTServer() : server(nullptr), service(nullptr), readCharacteristic(nullptr),
                   writeCharacteristic(nullptr), callbacks(this) {}

    void start() {
        if (isRunning) return;

        // 初始化 BLE
        BLEDevice::init("MT");
        server = BLEDevice::createServer();
        server->setCallbacks(&callbacks);

        // 创建服务
        service = server->createService(SERVICE_UUID);

        // 创建写入特征 (可写)
        writeCharacteristic = service->createCharacteristic(
            WRITE_UUID,
            BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY
        );
        writeCharacteristic->setCallbacks(&callbacks);

        // 创建读取/通知特征 (可读, 可通知)
        readCharacteristic = service->createCharacteristic(
            READ_UUID,
            BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
        );
        readCharacteristic->addDescriptor(new BLE2902());
        readCharacteristic->setCallbacks(&callbacks);

        // 启动服务
        service->start();

        // 开始广播
        startAdvertising();

        isRunning = true;
    }

    void stop() {
        if (!isRunning) return;

        BLEDevice::stopAdvertising();
        if (service) {
            service->stop();
        }
        isRunning = false;
    }

    void startAdvertising() {
        BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
        pAdvertising->addServiceUUID(SERVICE_UUID);
        pAdvertising->setScanResponse(true);
        pAdvertising->setMinPreferred(0x06);
        pAdvertising->setMinPreferred(0x12);

        // Match iOS `BluetoothManager.centralManager(_:didDiscover:)` parsing:
        // [0-1] company id LE, [2-5] pump SN, [6] device type, [7] version (>= 8 bytes).
        const uint8_t mfg[] = {
            0x59,
            0x6A,
            0x28,
            0xD8,
            0x12,
            0x4A,
            0x01,
            0x01,
        };
        BLEAdvertisementData advData;
        advData.setManufacturerData(std::string(reinterpret_cast<const char*>(mfg), sizeof(mfg)));
        pAdvertising->setAdvertisementData(advData);

        BLEDevice::startAdvertising();
    }

    bool sendNotification(const uint8_t* data, size_t len, bool useCrcHack = true) {
        if (readCharacteristic == nullptr) return false;

        std::vector<uint8_t> payload(data, data + len);

        if (useCrcHack && len > 0 && data[1] != 0x00) {
            if (len >= 6) {
                uint8_t expectedCrc = crc8Calculate(data, len - 2);
                if (payload[len - 2] != expectedCrc) {
                    payload[len - 1] = 0x00;
                }
            }
        }

        readCharacteristic->setValue(payload.data(), payload.size());
        readCharacteristic->notify();
        return true;
    }

    bool sendNotificationWithCrcHack(const uint8_t* data, size_t len) {
        return sendNotification(data, len, true);
    }

    bool sendRawNotification(const uint8_t* data, size_t len) {
        if (readCharacteristic == nullptr) return false;
        readCharacteristic->setValue(data, len);
        readCharacteristic->notify();
        return true;
    }

    bool sendResponse(const uint8_t* data, size_t len) {
        if (writeCharacteristic == nullptr) return false;

        writeCharacteristic->setValue(data, len);
        writeCharacteristic->notify();
        return true;
    }

private:
    BLEServer* server;
    BLEService* service;
    BLECharacteristic* readCharacteristic;
    BLECharacteristic* writeCharacteristic;
    GATTServerCallbacks callbacks;

    friend class GATTServerCallbacks;
};

inline void GATTServerCallbacks::onConnect(BLEServer* pServer) {
    Serial.println("[BLE] 客户端已连接");
    if (server && server->onConnect) {
        server->onConnect();
    }
}

inline void GATTServerCallbacks::onDisconnect(BLEServer* pServer) {
    Serial.println("[BLE] 客户端已断开");
    if (server && server->onDisconnect) {
        server->onDisconnect();
    }
    if (server) {
        server->startAdvertising();
    }
}

inline void GATTServerCallbacks::onWrite(BLECharacteristic* pCharacteristic) {
    if (server && server->onWriteRequest) {
        std::string value = pCharacteristic->getValue();
        server->onWriteRequest(reinterpret_cast<const uint8_t*>(value.data()), value.length());
    }
}

inline void GATTServerCallbacks::onSubscribe(BLECharacteristic* pCharacteristic, uint16_t subValue) {
    if (server && server->onSubscribe) {
        server->onSubscribe(subValue != 0);
    }
}

} // namespace M640GKit

#endif // M640G_GATT_SERVER_H
