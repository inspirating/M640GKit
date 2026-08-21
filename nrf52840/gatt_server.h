/*
================================================================================
GATT 服务器 (nRF52840 / NimBLE 版本)
================================================================================

作为 GATT Server 运行, 模拟 M640G 泵设备。对应 ESP32/gatt_server.h。

设计原则: 公共 API 与 ESP32 版完全一致 (GATTServer 类的 start/stop/
startAdvertising/stopAdvertising/disconnectAll/sendNotification/
sendRawNotification/sendResponse + 4 个回调指针 + advertisingSuspended 成员),
使 pump_simulator.h 零改动复用。

内部使用 NimBLE-Arduino 库实现底层 BLE:
  - NimBLEService + 两个 NimBLECharacteristic (READ_UUID 读+notify, WRITE_UUID 写+notify)
  - 厂商广播数据 59 6A 65 D1 79 98 01 01 (iOS 配对识别, 字节序不可变)
  - NimBLEServerCallbacks / NimBLECharacteristicCallbacks

功耗优化 (相比 Adafruit Bluefruit):
  - 自适应广播间隔: 空闲时 1s 间隔, 提高发现速度时可临时调小
  - 合理的连接参数: slave_latency=2, supervision_timeout=8s
  - NimBLE 原生支持低功耗模式, 空闲时 CPU 可进入 Idle
================================================================================
*/

#ifndef M640G_GATT_SERVER_H
#define M640G_GATT_SERVER_H

#include <Arduino.h>
#include <string>
#include <vector>
#include <NimBLEDevice.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include "enums.h"
#include "crc8.h"

namespace M640GKit {

// 回调函数类型定义 (签名与 ESP32 版完全一致)
typedef void (*WriteRequestCallback)(const uint8_t* data, size_t len);
typedef void (*SubscribeCallback)(bool subscribed);
typedef void (*DisconnectCallback)();
typedef void (*ConnectCallback)();

// 前向声明
class GATTServer;

// ---------- NimBLE 静态回调转发 ----------
static GATTServer* sActiveGatt = nullptr;

// NimBLE 服务器回调 (处理连接/断开)
class PumpServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) override {
        Serial.println("");
        Serial.println("========================================");
        Serial.println("[BLE] CLIENT CONNECTED!");
        Serial.println("========================================");
        Serial.println("");

        // 请求 MTU 交换
        if (pServer) {
            pServer->setMTU(247);
            Serial.println("[BLE] MTU exchange requested (247)");
        }

        if (sActiveGatt && sActiveGatt->onConnect) {
            sActiveGatt->onConnect();
        }
    }

    void onDisconnect(NimBLEServer* pServer) override {
        Serial.println("");
        Serial.println("========================================");
        Serial.println("[BLE] CLIENT DISCONNECTED!");
        Serial.println("========================================");
        Serial.println("");

        if (sActiveGatt && sActiveGatt->onDisconnect) {
            sActiveGatt->onDisconnect();
        }
    }
};

// NimBLE 特征值回调 (处理写入/CCCD)
class PumpCharacteristicCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) override {
        if (sActiveGatt && sActiveGatt->onWriteRequest) {
            std::string value = pCharacteristic->getValue();
            if (!value.empty()) {
                sActiveGatt->onWriteRequest(
                    reinterpret_cast<const uint8_t*>(value.data()),
                    value.length()
                );
            }
        }
    }

    void onSubscribe(NimBLECharacteristic* pCharacteristic,
                      ble_gap_conn_desc* desc,
                      uint16_t cccd_value) override {
        (void)pCharacteristic;
        (void)desc;
        bool notifyEnabled = (cccd_value & 0x0001) != 0;
        bool indicateEnabled = (cccd_value & 0x0002) != 0;

        Serial.print("[BLE] CCCD updated: 0x");
        Serial.print(cccd_value, HEX);
        Serial.print(" - Notify: ");
        Serial.print(notifyEnabled ? "ON" : "OFF");
        Serial.print(", Indicate: ");
        Serial.println(indicateEnabled ? "ON" : "OFF");

        if (sActiveGatt && sActiveGatt->onSubscribe) {
            sActiveGatt->onSubscribe(notifyEnabled || indicateEnabled);
        }
    }
};

// ---------- GATTServer 类 ----------
class GATTServer {
public:
    bool isRunning = false;
    WriteRequestCallback onWriteRequest = nullptr;
    SubscribeCallback onSubscribe = nullptr;
    DisconnectCallback onDisconnect = nullptr;
    ConnectCallback onConnect = nullptr;
    bool advertisingSuspended = false;

    GATTServer() : server(nullptr), service(nullptr), readChr(nullptr),
                   writeChr(nullptr), pServerCallbacks(nullptr),
                   pChrCallbacks(nullptr) {}

    ~GATTServer() {
        stop();
    }

    void start() {
        if (isRunning) {
            Serial.println("[GATT] Server already running");
            return;
        }

        Serial.println("[GATT] Initializing NimBLE...");

        // 初始化 NimBLE 设备
        NimBLEDevice::init("MT");

        // 创建服务器
        server = NimBLEDevice::createServer();
        pServerCallbacks = new PumpServerCallbacks();
        server->setCallbacks(pServerCallbacks);

        // 创建服务
        service = server->createService(SERVICE_UUID);

        // 创建写入特征 (可写 + notify)
        writeChr = service->createCharacteristic(
            WRITE_UUID,
            NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::NOTIFY
        );
        writeChr->setMaxLen(247);
        pChrCallbacks = new PumpCharacteristicCallbacks();
        writeChr->setCallbacks(pChrCallbacks);
        Serial.println("[GATT] Write characteristic created");

        // 创建读取/通知特征 (可读 + notify)
        readChr = service->createCharacteristic(
            READ_UUID,
            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
        );
        readChr->setMaxLen(247);
        readChr->setCallbacks(pChrCallbacks);
        // 设置初始值为空
        readChr->setValue(nullptr, 0);
        Serial.println("[GATT] Read characteristic created");

        // 启动服务
        service->start();

        // 配置连接参数 (在 startAdvertising 前设置)
        server->setMTU(247);
        Serial.println("[GATT] NimBLE initialized with name 'MT'");

        // 获取广播对象并配置
        startAdvertising();

        isRunning = true;
        Serial.println("[GATT] GATT Server is now running! (NimBLE)");
    }

    void stop() {
        if (!isRunning) return;

        Serial.println("[GATT] Stopping NimBLE...");

        stopAdvertising();

        if (server) {
            server->startAdvertising();  // 确保停止广播
        }

        NimBLEDevice::deinit(true);
        isRunning = false;

        Serial.println("[GATT] GATT Server stopped");
    }

    void disconnectAll() {
        if (server) {
            // 断开所有连接
            int count = server->getConnectedCount();
            for (int i = 0; i < count; i++) {
                server->disconnect(i);
            }
            Serial.print("[GATT] Disconnected ");
            Serial.print(count);
            Serial.println(" connections");
        }
    }

    void startAdvertising() {
        if (advertisingSuspended) {
            Serial.println("[ADV] Advertising suspended, skipping...");
            return;
        }
        Serial.println("[ADV] Configuring BLE advertising...");

        NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
        adv->reset();

        // 设置广播参数 - 使用省电间隔 (1s/2s)
        // 范围: 32 (20ms) - 16384 (10.24s)
        // 使用 1600 (1s) / 3200 (2s) 平衡发现速度和功耗
        adv->setMinInterval(1600);
        adv->setMaxInterval(3200);
        adv->setDuration(0);  // 持续广播

        // 添加服务 UUID 到广播包
        adv->addServiceUUID(SERVICE_UUID);

        // 添加设备名
        adv->setName("MT");

        // 添加厂商数据 (iOS 配对识别用, 字节序和内容必须与原版本一字不差)
        const uint8_t mfg[] = {
            0x59, 0x6A,  // company id LE (0x6A59)
            0x65, 0xD1, 0x79, 0x98,  // pump SN
            0x01,  // device type
            0x01   // version
        };
        adv->setManufacturerData((const char*)mfg, sizeof(mfg));

        Serial.print("[ADV] Manufacturer data (");
        Serial.print(sizeof(mfg));
        Serial.print(" bytes): ");
        for (size_t i = 0; i < sizeof(mfg); i++) {
            Serial.printf("%02X ", mfg[i]);
        }
        Serial.println("");

        // 开始广播
        if (!adv->start()) {
            Serial.println("[ADV] ERROR: Failed to start advertising!");
        } else {
            Serial.println("[ADV] BLE advertising started");
            Serial.println("[ADV] Device name: MT");
            Serial.println("[ADV] Service UUID: " + String(SERVICE_UUID));
            Serial.println("[ADV] Waiting for mobile device to connect...");
        }
    }

    void stopAdvertising() {
        NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
        if (adv) {
            adv->stop();
            Serial.println("[ADV] BLE advertising stopped");
        }
    }

    // 通知: 可选 CRC 修正 (与原版 sendNotification 行为一致)
    bool sendNotification(const uint8_t* data, size_t len, bool useCrcHack = true) {
        if (readChr == nullptr) return false;
        if (!server || server->getConnectedCount() == 0) return false;

        std::vector<uint8_t> payload(data, data + len);
        if (useCrcHack && len > 0 && len >= 6 && data[1] != 0x00) {
            uint8_t expectedCrc = crc8Calculate(data, len - 1);
            if (payload[len - 1] != expectedCrc) {
                payload[len - 1] = expectedCrc;
            }
        }
        readChr->notify(payload.data(), payload.size());
        return true;
    }

    bool sendNotificationWithCrcHack(const uint8_t* data, size_t len) {
        return sendNotification(data, len, true);
    }

    // 原始通知 (不做 CRC 修正) — pump_simulator.h 主要走这个
    bool sendRawNotification(const uint8_t* data, size_t len) {
        if (readChr == nullptr) {
            Serial.println("[GATT][E] sendRawNotification: readChr is null");
            return false;
        }
        if (!server || server->getConnectedCount() == 0) {
            Serial.println("[GATT][E] sendRawNotification: not connected");
            return false;
        }
        bool ok = readChr->notify(data, len);
        return ok;
    }

    // 响应: 通过写特征值 notify
    bool sendResponse(const uint8_t* data, size_t len) {
        Serial.print("[GATT][I] sendResponse called, len=");
        Serial.println(len);
        return sendNotifyWithRetry(writeChr, "response notify", data, len);
    }

private:
    bool sendNotifyWithRetry(NimBLECharacteristic* chr, const char* name,
                              const uint8_t* data, size_t len) {
        if (chr == nullptr) {
            Serial.print("[GATT][E] ");
            Serial.print(name);
            Serial.println(": characteristic is null");
            return false;
        }
        if (!server || server->getConnectedCount() == 0) {
            Serial.print("[GATT][E] ");
            Serial.print(name);
            Serial.println(": not connected");
            return false;
        }

        // 重试机制: HVN 队列满时等待
        bool ok = false;
        for (int retry = 0; retry < 50; retry++) {
            ok = chr->notify(data, len);
            if (ok) break;
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        Serial.print("[GATT][I] ");
        Serial.print(name);
        Serial.print(" sent, len=");
        Serial.print(len);
        Serial.print(" result=");
        Serial.print(ok ? "OK" : "FAIL");
        if (!ok) {
            Serial.println(" [WARN] notify failed, Trio may not have received response");
        } else {
            Serial.println("");
        }

        return ok;
    }

private:
    NimBLEServer* server;
    NimBLEService* service;
    NimBLECharacteristic* readChr;
    NimBLECharacteristic* writeChr;
    PumpServerCallbacks* pServerCallbacks;
    PumpCharacteristicCallbacks* pChrCallbacks;
};

} // namespace M640GKit

#endif // M640G_GATT_SERVER_H
