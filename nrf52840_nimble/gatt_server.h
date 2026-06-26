/*
================================================================================
GATT 服务器 (C++ 版本 - NimBLE-Arduino)
================================================================================

作为 GATT Server 运行, 模拟 M640G 泵设备
从 ESP32 Arduino BLE 库移植至 NimBLE-Arduino API

对应 ESP32: gatt_server.h
================================================================================
*/

#ifndef M640G_GATT_SERVER_H
#define M640G_GATT_SERVER_H

#include <Arduino.h>
#include <string>
#include <vector>
#include <NimBLEDevice.h>
#include "enums.h"
#include "crc8.h"

namespace M640GKit {

// 回调函数类型定义
typedef void (*WriteRequestCallback)(const uint8_t* data, size_t len);
typedef void (*SubscribeCallback)(bool subscribed);
typedef void (*DisconnectCallback)();
typedef void (*ConnectCallback)();

// 前向声明
class GATTServer;

// ========== NimBLE 服务器回调 ==========
class ServerCallbacks : public NimBLEServerCallbacks {
public:
    GATTServer* server = nullptr;
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override;
    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override;
};

// ========== NimBLE 特征回调 ==========
class CharCallbacks : public NimBLECharacteristicCallbacks {
public:
    GATTServer* server = nullptr;
    NimBLECharacteristic* writeChar = nullptr;
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override;
    void onSubscribe(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo, uint16_t subValue) override;
};

class GATTServer {
public:
    bool isRunning = false;
    bool clientSubscribed = false;  // 客户端是否已订阅通知
    WriteRequestCallback onWriteRequest = nullptr;
    SubscribeCallback onSubscribe = nullptr;
    DisconnectCallback onDisconnect = nullptr;
    ConnectCallback onConnect = nullptr;

    GATTServer() : pServer(nullptr), pService(nullptr),
                   readCharacteristic(nullptr), writeCharacteristic(nullptr) {}

    void start() {
        if (isRunning) {
            Serial.println("[GATT] Server already running");
            return;
        }

        Serial.println("[GATT] Initializing NimBLE device...");

        // 初始化 NimBLE
        NimBLEDevice::init("MT");

        // 禁用隐私地址(RPA)，使用静态地址，确保 iOS 能稳定识别设备
        NimBLEDevice::enablePrivacy(false);

        // 禁用 bonding，避免配对流程导致连接超时
        NimBLEDevice::setSecurityAuth(false, false, false);

        Serial.println("[GATT] NimBLE device initialized with name 'MT'");

        Serial.println("[GATT] Creating BLE server...");
        pServer = NimBLEDevice::createServer();
        if (!pServer) {
            Serial.println("[GATT] ERROR: Failed to create BLE server!");
            return;
        }
        serverCallbacks.server = this;
        pServer->setCallbacks(&serverCallbacks, false);
        Serial.println("[GATT] BLE server created with callbacks");

        // 创建服务
        Serial.println("[GATT] Creating BLE service...");
        pService = pServer->createService(SERVICE_UUID);
        if (!pService) {
            Serial.println("[GATT] ERROR: Failed to create BLE service!");
            return;
        }
        Serial.println("[GATT] BLE service created");

        // 创建写入特征 (可写, 可通知)
        Serial.println("[GATT] Creating write characteristic...");
        writeCharacteristic = pService->createCharacteristic(
            WRITE_UUID,
            NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::NOTIFY
        );
        if (!writeCharacteristic) {
            Serial.println("[GATT] ERROR: Failed to create write characteristic!");
            return;
        }
        charCallbacks.server = this;
        charCallbacks.writeChar = writeCharacteristic;
        writeCharacteristic->setCallbacks(&charCallbacks);
        Serial.println("[GATT] Write characteristic created");

        // 创建读取/通知特征 (可读, 可通知)
        Serial.println("[GATT] Creating read characteristic...");
        readCharacteristic = pService->createCharacteristic(
            READ_UUID,
            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
        );
        if (!readCharacteristic) {
            Serial.println("[GATT] ERROR: Failed to create read characteristic!");
            return;
        }
        // 订阅回调绑定到 read characteristic (iOS 订阅通知时触发)
        readCharacteristic->setCallbacks(&charCallbacks);
        Serial.println("[GATT] Read characteristic created with subscribe callback");

        // 启动服务
        Serial.println("[GATT] Starting BLE service...");
        pService->start();
        Serial.println("[GATT] BLE service started");

        // 开始广播
        Serial.println("[GATT] Starting BLE advertising...");
        startAdvertising();
        Serial.println("[GATT] BLE advertising started");

        isRunning = true;
        Serial.println("[GATT] GATT Server is now running!");
    }

    void stop() {
        if (!isRunning) return;

        NimBLEDevice::stopAdvertising();
        if (pServer) {
            pServer->removeService(pService);
        }
        isRunning = false;
    }

    bool advertisingSuspended = false;

    void disconnectAll() {
        if (pServer) {
            NimBLEConnInfo connInfo = pServer->getPeerInfo(0);
            pServer->disconnect(connInfo.getConnHandle());
        }
    }

    void startAdvertising() {
        if (advertisingSuspended) {
            Serial.println("[ADV] Advertising suspended, skipping...");
            return;
        }
        Serial.println("[ADV] Configuring BLE advertising...");

        NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();

        // Match iOS `BluetoothManager.centralManager(_:didDiscover:)` parsing:
        // [0-1] company id LE, [2-5] pump SN, [6] device type, [7] version (>= 8 bytes).
        const uint8_t mfg[] = {
            0x59,
            0x6A,
            0x65,
            0xD1,
            0x79,
            0x98,
            0x01,
            0x01,
        };

        Serial.print("[ADV] Manufacturer data (");
        Serial.print(sizeof(mfg));
        Serial.print(" bytes): ");
        for (size_t i = 0; i < sizeof(mfg); i++) {
            Serial.printf("%02X ", mfg[i]);
        }
        Serial.println("");

        // 构建 NimBLE 广播数据
        NimBLEAdvertisementData advData;
        advData.setName("MT");
        advData.setManufacturerData(std::string(reinterpret_cast<const char*>(mfg), sizeof(mfg)));

        pAdvertising->setAdvertisementData(advData);

        // Scan response: 空，与 ESP32 原版行为一致
        pAdvertising->setScanResponseData(NimBLEAdvertisementData());

        Serial.println("[ADV] Starting BLE advertising...");
        NimBLEDevice::startAdvertising();
        Serial.println("[ADV] BLE advertising is now active!");
        Serial.println("[ADV] Device name: MT");
        Serial.println("[ADV] Service UUID: " + String(SERVICE_UUID));
        Serial.println("[ADV] Waiting for mobile device to connect...");
    }

    bool sendNotification(const uint8_t* data, size_t len, bool useCrcHack = true) {
        if (readCharacteristic == nullptr) return false;

        std::vector<uint8_t> payload(data, data + len);

        if (useCrcHack && len > 0 && len >= 6 && data[1] != 0x00) {
            uint8_t expectedCrc = crc8Calculate(data, len - 1);
            if (payload[len - 1] != expectedCrc) {
                payload[len - 1] = expectedCrc;
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
    NimBLEServer* pServer;
    NimBLEService* pService;
    NimBLECharacteristic* readCharacteristic;
    NimBLECharacteristic* writeCharacteristic;
    ServerCallbacks serverCallbacks;
    CharCallbacks charCallbacks;

    friend class ServerCallbacks;
    friend class CharCallbacks;
};

// ========== 回调实现 ==========

inline void ServerCallbacks::onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) {
    Serial.println("");
    Serial.println("========================================");
    Serial.println("[BLE] CLIENT CONNECTED!");
    Serial.print("[BLE] Connected clients: ");
    Serial.println(pServer->getConnectedCount());
    Serial.println("========================================");
    Serial.println("");

    // 更新连接参数，确保 iOS 稳定连接
    // interval: 15ms (12*1.25ms), latency: 0, timeout: 4s (400*10ms)
    pServer->updateConnParams(connInfo.getConnHandle(), 12, 24, 0, 400);

    if (server && server->onConnect) {
        server->onConnect();
    }
}

inline void ServerCallbacks::onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) {
    Serial.println("");
    Serial.println("========================================");
    Serial.println("[BLE] CLIENT DISCONNECTED!");
    Serial.print("[BLE] Disconnect reason: ");
    Serial.println(reason);
    Serial.println("========================================");
    Serial.println("");

    if (server) {
        server->clientSubscribed = false;
    }
    if (server && server->onDisconnect) {
        server->onDisconnect();
    }
    if (server) {
        Serial.println("[BLE] Restarting advertising...");
        server->startAdvertising();
    }
}

inline void CharCallbacks::onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
    if (server && server->onWriteRequest) {
        auto val = pCharacteristic->getValue();
        if (val.size() > 0) {
            server->onWriteRequest(reinterpret_cast<const uint8_t*>(val.data()), val.size());
        }
    }
}

inline void CharCallbacks::onSubscribe(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo, uint16_t subValue) {
    bool subscribed = (subValue & 0x0001) != 0;  // 0x0001 = notifications enabled
    Serial.print("[BLE] onSubscribe: ");
    Serial.print(subscribed ? "SUBSCRIBED" : "UNSUBSCRIBED");
    Serial.print(" (subValue=0x");
    Serial.print(subValue, HEX);
    Serial.println(")");

    if (server) {
        server->clientSubscribed = subscribed;
    }

    if (server && server->onSubscribe) {
        server->onSubscribe(subscribed);
    }
}

} // namespace M640GKit

#endif // M640G_GATT_SERVER_H
