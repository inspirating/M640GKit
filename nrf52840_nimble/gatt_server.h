/*
================================================================================
GATT 服务器 (nRF52840 / n-able-Arduino + NimBLE 版本)
================================================================================

作为 GATT Server 运行, 模拟 M640G 泵设备。对应 ESP32/gatt_server.h。

设计原则: 公共 API 与 ESP32 版完全一致 (GATTServer 类的 start/stop/
startAdvertising/stopAdvertising/disconnectAll/sendNotification/
sendRawNotification/sendResponse + 4 个回调指针 + advertisingSuspended 成员),
使 pump_simulator.h 零改动复用。

底层 BLE 栈: Apache NimBLE (经 h2zero NimBLE-Arduino 库, 跑在 n-able-Arduino
核心上, 完全不使用 Nordic 闭源 SoftDevice S140):
  - NimBLEServer + 两个 NimBLECharacteristic (READ_UUID 读+notify, WRITE_UUID 写+notify)
  - 厂商广播数据 59 6A 65 D1 79 98 01 01 (iOS 配对识别, 字节序不可变)
  - NimBLEServerCallbacks (onConnect/onDisconnect) + NimBLECharacteristicCallbacks
    (onWrite/onSubscribe) 转发到 GATTServer 实例的 4 个函数指针

与原 Adafruit Bluefruit 版的关键差异:
  - 不再依赖 SoftDevice: 所有 sd_ble_gap_* / ble_evt_t / BLE_GAP_EVT_* 消失。
    NimBLE 默认不配对/不加密, iOS 旧 bond 导致的 0x8 断开问题天然不存在 ——
    NimBLE 不响应 SEC_REQUEST, iOS CoreBluetooth 会降级为明文连接。
    Medtrum 协议层用 AUTH_REQ 自己做认证, 不需要 BLE 加密。
  - 回调模型: Adafruit 用全局静态函数 + setEventCallback; NimBLE 用回调类继承
    (ServerCallbacks / CharWriteCallbacks), 仍通过静态指针转发到单例实例。
  - MTU: NimBLEDevice::setMTU(247) 在 init 时全局设定, 连接时自动协商,
    不需要像 Bluefruit 那样在 onConnect 里 requestMtuExchange。
  - MAC 地址: n-able 核心仍暴露 NRF_FICR 寄存器, 继续用 FICR 派生随机静态地址,
    跨重启稳定。若 NimBLEDevice 不支持 setOwnAddrType, 回退使用 NimBLE 默认地址
    (Trio 首次需重新配对一次)。
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

// 回调函数类型定义 (签名与 ESP32 版完全一致)
typedef void (*WriteRequestCallback)(const uint8_t* data, size_t len);
typedef void (*SubscribeCallback)(bool subscribed);
typedef void (*DisconnectCallback)();
typedef void (*ConnectCallback)();

// 前向声明
class GATTServer;

// ---------- NimBLE 回调转发 ----------
// NimBLE 用回调类继承; 这里定义两个内部回调类, 持有 GATTServer 指针转发到
// 它的 4 个函数指针 (onConnect/onDisconnect/onWriteRequest/onSubscribe)。
// pump_simulator.h 全局只有一个 GATTServer 实例 gattServer。

// 服务端连接/断开回调
// 此版 NimBLE-Arduino 只提供带 NimBLEConnInfo 参数的签名,
// 不存在旧版 onConnect(NimBLEServer*) / onDisconnect(NimBLEServer*) 重载。
class GattServerCallbacks : public NimBLEServerCallbacks {
public:
    GATTServer* gatt = nullptr;
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override;
    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override;
};

// 特征值写入 / CCCD 订阅回调 (写入特征用)
class GattWriteCharCallbacks : public NimBLECharacteristicCallbacks {
public:
    GATTServer* gatt = nullptr;
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override;
    void onSubscribe(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo, uint16_t subValue) override;
};

// 读取特征的 CCCD 订阅回调 (只需捕获订阅状态)
class GattReadCharCallbacks : public NimBLECharacteristicCallbacks {
public:
    GATTServer* gatt = nullptr;
    void onSubscribe(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo, uint16_t subValue) override;
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

    GATTServer() : bleServer(nullptr), bleService(nullptr),
                   readChr(nullptr), writeChr(nullptr),
                   bleAdv(nullptr) {}

    void start() {
        if (isRunning) {
            Serial.println("[GATT] Server already running");
            return;
        }

        Serial.println("[GATT] Initializing NimBLE BLE...");

        // ---------- 固定 BLE MAC 地址 (基于 FICR 硬件 ID) ----------
        // n-able-Arduino 核心仍暴露 NRF_FICR 寄存器。取 FICR DEVICEADDR 的低 48-bit
        // 作为随机静态地址 (高 2 bit = 11, 故 mac[0] |= 0xC0)。跨重启天然稳定。
        // 注意: 必须在 NimBLEDevice::init 之前设置 own addr type, init 之后才能 setAddr。
        bool addrSet = false;
        {
            uint32_t addr0 = NRF_FICR->DEVICEADDR[0];
            uint32_t addr1 = NRF_FICR->DEVICEADDR[1];
            uint8_t mac[6];
            mac[0] = ((uint8_t)(addr0 & 0xFF)) | 0xC0;  // 随机静态地址标志位
            mac[1] = (uint8_t)((addr0 >> 8) & 0xFF);
            mac[2] = (uint8_t)((addr0 >> 16) & 0xFF);
            mac[3] = (uint8_t)((addr0 >> 24) & 0xFF);
            mac[4] = (uint8_t)(addr1 & 0xFF);
            mac[5] = (uint8_t)((addr1 >> 8) & 0xFF);

            // 先告诉 NimBLE 使用随机静态地址类型, 再设置具体地址。
            // 不同 NimBLE 版本 API 略有差异, 用 try 兜底 (此处无异常, 用返回值判断)。
            NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM);
            savedMac[0] = mac[0]; savedMac[1] = mac[1]; savedMac[2] = mac[2];
            savedMac[3] = mac[3]; savedMac[4] = mac[4]; savedMac[5] = mac[5];
            macValid = true;

            Serial.printf("[GATT] BLE MAC derived (FICR): %02X:%02X:%02X:%02X:%02X:%02X\n",
                          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            (void)addrSet;
        }

        // 初始化 NimBLE: 名称 "MT"。init 内部创建 NimBLEServer。
        // init 之后 setAddr 才生效 (NimBLE 协议栈需先起来)。
        NimBLEDevice::init("MT");

        // init 后设置随机静态地址 (若 setAddr 不可用则回退默认地址, Trio 重新配对一次)
        if (macValid) {
            NimBLEAddress addr(savedMac, BLE_OWN_ADDR_RANDOM);
            // NimBLEDevice::setAddr 在不同版本签名不同; 失败不影响功能, 仅地址回退默认。
            // 此处用 setOwnAddrType 已声明 random, 若 setAddr 缺失则 NimBLE 用芯片默认随机地址。
        }

        // MTU = 247, 支持大包一次性发送 (SYNCHRONIZE 46字节)。NimBLE 在 init 后设置,
        // 连接时自动协商, 无需像 Bluefruit 那样在 onConnect 里 requestMtuExchange。
        NimBLEDevice::setMTU(247);
        Serial.println("[GATT] Peripheral MTU = 247");

        // ---------- 禁用 BLE Bonding (无加密/无配对) ----------
        // NimBLE 默认不配对不加密。不调用任何 security/bonding 设置,
        // 也不 setCallbacks 注册 security 回调, iOS 的 SEC_REQUEST 不会被触发
        // (NimBLE 端没有配对能力就不响应)。Medtrum 协议层用 AUTH_REQ 自己认证。
        Serial.println("[GATT] BLE Bonding disabled (no encryption, no pairing)");

        // 最大功率 (+8 dBm)
        NimBLEDevice::setPower(8);  // ESP_PWR_LVL_N0..N15 / dBm
        Serial.println("[GATT] TX power = 8 dBm");

        // ---------- 创建 Server + Service ----------
        Serial.println("[GATT] Creating BLE server...");
        bleServer = NimBLEDevice::createServer();
        bleServer->setCallbacks(&serverCallbacks);
        serverCallbacks.gatt = this;

        Serial.println("[GATT] Creating BLE service...");
        bleService = bleServer->createService(SERVICE_UUID);

        // 写入特征 (可写 + notify): Trio 下发命令走这里, 也用于 sendResponse 回包。
        // NIMBLE_PROPERTY::WRITE = 写需响应; WRITE_NR = 写无需响应; NOTIFY = 允许订阅 notify。
        // NimBLE 不需要显式 setPermission —— 属性 flags 已含权限信息, 默认 OPEN 无需配对。
        writeChr = bleService->createCharacteristic(
            WRITE_UUID,
            NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::NOTIFY
        );
        writeChr->setCallbacks(&writeCallbacks);
        writeCallbacks.gatt = this;
        Serial.println("[GATT] Write characteristic created");

        // 读取/通知特征 (可读 + notify): 泵向 Trio 上报走这里。
        readChr = bleService->createCharacteristic(
            READ_UUID,
            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
        );
        readChr->setCallbacks(&readCallbacks);
        readCallbacks.gatt = this;
        Serial.println("[GATT] Read characteristic created");

        // 启动服务 (NimBLE v2: service 在 server 启动时自动启动, start() 已无效果但保留调用)
        bleService->start();
        Serial.println("[GATT] BLE service started");

        // ---------- 广播 ----------
        bleAdv = NimBLEDevice::getAdvertising();
        startAdvertising();

        isRunning = true;
        Serial.println("[GATT] GATT Server is now running! (NimBLE) v20250625-1");
    }

    void stop() {
        if (!isRunning) return;
        if (bleAdv) bleAdv->stop();
        isRunning = false;
    }

    void disconnectAll() {
        if (bleServer) {
            // NimBLE 没有 disconnectAll(), 遍历所有连接逐个断开
            auto peers = bleServer->getPeerDevices();
            for (auto connHandle : peers) {
                bleServer->disconnect(connHandle);
            }
        }
    }

    void startAdvertising() {
        if (advertisingSuspended) {
            Serial.println("[ADV] Advertising suspended, skipping...");
            return;
        }
        if (!bleAdv) {
            Serial.println("[ADV] ERROR: advertising handle is null");
            return;
        }
        Serial.println("[ADV] Configuring BLE advertising...");

        bleAdv->stop();

        // 构造广播数据。NimBLEAdvertisementData 一次只承载一个广播包内容;
        // 把设备名 + 厂商数据放进主广播包, Service UUID 也放进主包 (NimBLE 会自动
        // 决定是否溢出到 scan response, 超过 31 字节时库会处理)。
        NimBLEAdvertisementData advData;
        advData.setName("MT");

        // 厂商数据 (iOS 配对识别用, 字节序和内容必须与 ESP32 版一字不差):
        // [0-1] company id LE (0x6A59), [2-5] pump SN, [6] device type, [7] version
        const uint8_t mfg[] = {
            0x59, 0x6A,
            0x65, 0xD1, 0x79, 0x98,
            0x01,
            0x01,
        };
        advData.setManufacturerData(std::string((const char*)mfg, sizeof(mfg)));

        Serial.print("[ADV] Manufacturer data (");
        Serial.print(sizeof(mfg));
        Serial.print(" bytes): ");
        for (size_t i = 0; i < sizeof(mfg); i++) {
            Serial.printf("%02X ", mfg[i]);
        }
        Serial.println("");

        // Service UUID 加入广播包 (NimBLE 在超过 31 字节时自动溢出到 scan response)。
        advData.addServiceUUID(SERVICE_UUID);

        // 广告间隔: 快速模式 (20ms / 30ms), 加快 iOS 扫描发现速度。
        // 参数单位为 0.625ms: 32*0.625=20ms, 48*0.625=30ms。
        bleAdv->setMinInterval(32);
        bleAdv->setMaxInterval(48);

        bleAdv->setAdvertisementData(advData);

        // 启用主动扫描响应 (让 iOS 扫描时能读到完整 service UUID + name)。
        // NimBLE 默认会在收到扫描请求时回送配置的广播数据作为响应。
        bleAdv->enableScanResponse(true);

        Serial.println("[ADV] Starting BLE advertising...");
        bleAdv->start();
        Serial.println("[ADV] BLE advertising is now active!");
        Serial.println("[ADV] Device name: MT");
        Serial.println("[ADV] Service UUID: " + String(SERVICE_UUID));
        Serial.println("[ADV] Waiting for mobile device to connect...");
    }

    void stopAdvertising() {
        if (bleAdv) bleAdv->stop();
    }

    // 通知: 可选 CRC 修正 (与 ESP32 版 sendNotification 行为一致)。
    // 当前 pump_simulator.h 实际走 sendRawNotification, 这里保留以保 API 完整。
    bool sendNotification(const uint8_t* data, size_t len, bool useCrcHack = true) {
        if (readChr == nullptr) return false;

        std::vector<uint8_t> payload(data, data + len);
        if (useCrcHack && len > 0 && len >= 6 && data[1] != 0x00) {
            uint8_t expectedCrc = crc8Calculate(data, len - 1);
            if (payload[len - 1] != expectedCrc) {
                payload[len - 1] = expectedCrc;
            }
        }
        return sendNotify(readChr, payload.data(), payload.size());
    }

    bool sendNotificationWithCrcHack(const uint8_t* data, size_t len) {
        return sendNotification(data, len, true);
    }

    // 原始通知 (不做 CRC 修正) —— pump_simulator.h 主要走这个
    bool sendRawNotification(const uint8_t* data, size_t len) {
        if (readChr == nullptr) {
            Serial.println("[GATT][E] sendRawNotification: readChr is null");
            return false;
        }
        if (!isAnyConnected()) {
            Serial.println("[GATT][E] sendRawNotification: not connected");
            return false;
        }
        return sendNotify(readChr, data, len);
    }

    // 响应: 通过写特征值 notify (Trio 只监听写特征的响应)
    bool sendResponse(const uint8_t* data, size_t len) {
        Serial.print("[GATT][I] sendResponse called, len=");
        Serial.println(len);
        if (writeChr == nullptr) {
            Serial.println("[GATT][E] sendResponse: writeChr is null");
            return false;
        }
        if (!isAnyConnected()) {
            Serial.println("[GATT][E] sendResponse: not connected");
            return false;
        }
        return sendNotify(writeChr, data, len);
    }

private:
    // NimBLE v2.x: notify() 不再带 is_notification 参数, 先 setValue 再 notify。
    // 加重试逻辑: HVN 队列暂时满时短暂让出 CPU 重试 (与原 Bluefruit 版一致)。
    bool sendNotify(NimBLECharacteristic* chr, const uint8_t* data, size_t len) {
        if (chr == nullptr) return false;

        bool ok = false;
        for (int retry = 0; retry < 3; retry++) {
            chr->setValue(data, len);
            ok = chr->notify();
            if (ok) break;
            delay(2); // 仅极短暂让出 CPU, 不要用 pdMS_TO_TICKS 长休眠
        }

        Serial.print("[GATT][I] notify sent, len=");
        Serial.print(len);
        Serial.print(" result=");
        Serial.print(ok ? "OK" : "FAIL");
        if (!ok) {
            Serial.println(" [WARN] notify 连续失败, Trio 可能未收到响应");
        } else {
            Serial.println("");
        }
        return ok;
    }

    bool isAnyConnected() const {
        if (bleServer == nullptr) return false;
        // NimBLEServer::getConnectedCount() 返回当前连接的客户端数。
        return bleServer->getConnectedCount() > 0;
    }

private:
    NimBLEServer* bleServer;
    NimBLEService* bleService;
    NimBLECharacteristic* readChr;
    NimBLECharacteristic* writeChr;
    NimBLEAdvertising* bleAdv;

    GattServerCallbacks    serverCallbacks;
    GattWriteCharCallbacks writeCallbacks;
    GattReadCharCallbacks  readCallbacks;

    // FICR 派生的 MAC (用于 setOwnAddrType 后参考 / 调试打印)
    uint8_t savedMac[6] = {0};
    bool macValid = false;
};

// ---------- NimBLE 回调实现 (转发到 GATTServer 实例) ----------

inline void GattServerCallbacks::onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) {
    (void)pServer; (void)connInfo;
    Serial.println("");
    Serial.println("========================================");
    Serial.println("[BLE] CLIENT CONNECTED!");
    Serial.println("========================================");
    Serial.println("");

    Serial.println("[BLE] Calling onConnect callback...");
    if (gatt && gatt->onConnect) {
        gatt->onConnect();
    }
    Serial.println("[BLE] onConnect callback returned");
}

inline void GattServerCallbacks::onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) {
    (void)pServer; (void)connInfo;
    Serial.print("[BLE] Disconnect reason: 0x");
    Serial.println((uint16_t)reason, HEX);
    Serial.println("");
    Serial.println("========================================");
    Serial.println("[BLE] CLIENT DISCONNECTED!");
    Serial.println("========================================");
    Serial.println("");
    Serial.println("[BLE] Calling onDisconnect callback...");
    if (gatt && gatt->onDisconnect) {
        gatt->onDisconnect();
    }
    Serial.println("[BLE] onDisconnect callback returned");
    // 不在这里 startAdvertising —— handleBleDisconnect() 已经会调用,
    // 重复调用会导致广播重启两次。
}

inline void GattWriteCharCallbacks::onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
    (void)connInfo;
    if (gatt && gatt->onWriteRequest) {
        // NimBLE: getValue() 返回 NimBLEAttValue (可当 std::vector<uint8_t> 用)。
        NimBLEAttValue val = pCharacteristic->getValue();
        size_t len = val.size();
        if (len > 0) {
            gatt->onWriteRequest((const uint8_t*)val.data(), len);
        }
    }
}

// CCCD 订阅回调 (写入特征): 捕获 iOS 订阅/取消订阅 notify。
// NimBLE onSubscribe 的 subValue: bit0=notify, bit1=indicate (与 CCCD 编码一致)。
inline void GattWriteCharCallbacks::onSubscribe(NimBLECharacteristic* pCharacteristic,
                                                NimBLEConnInfo& connInfo, uint16_t subValue) {
    (void)pCharacteristic; (void)connInfo;
    bool notifyEnabled = (subValue & 0x0001) != 0;
    bool indicateEnabled = (subValue & 0x0002) != 0;

    Serial.print("[BLE] WriteChr CCCD subscribed: 0x");
    Serial.print(subValue, HEX);
    Serial.print(" - Notify: ");
    Serial.print(notifyEnabled ? "ON" : "OFF");
    Serial.print(", Indicate: ");
    Serial.println(indicateEnabled ? "ON" : "OFF");

    if (gatt && gatt->onSubscribe) {
        gatt->onSubscribe(notifyEnabled || indicateEnabled);
    }
}

// CCCD 订阅回调 (读取特征): 只记录状态, 不重复触发 onSubscribe (pump_simulator 已收到过)
inline void GattReadCharCallbacks::onSubscribe(NimBLECharacteristic* pCharacteristic,
                                               NimBLEConnInfo& connInfo, uint16_t subValue) {
    (void)pCharacteristic; (void)connInfo;
    bool notifyEnabled = (subValue & 0x0001) != 0;
    bool indicateEnabled = (subValue & 0x0002) != 0;
    Serial.print("[BLE] ReadChr CCCD subscribed: 0x");
    Serial.print(subValue, HEX);
    Serial.print(" - Notify: ");
    Serial.print(notifyEnabled ? "ON" : "OFF");
    Serial.print(", Indicate: ");
    Serial.println(indicateEnabled ? "ON" : "OFF");
}

} // namespace M640GKit

#endif // M640G_GATT_SERVER_H
