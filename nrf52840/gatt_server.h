/*
================================================================================
GATT 服务器 (nRF52840 / Adafruit Bluefruit 版本)
================================================================================

作为 GATT Server 运行, 模拟 M640G 泵设备。对应 ESP32/gatt_server.h。

设计原则: 公共 API 与 ESP32 版完全一致 (GATTServer 类的 start/stop/
startAdvertising/stopAdvertising/disconnectAll/sendNotification/
sendRawNotification/sendResponse + 4 个回调指针 + advertisingSuspended 成员),
使 pump_simulator.h 零改动复用。

内部用 Adafruit Bluefruit 库 (bluefruit.h) 实现底层 BLE:
  - BLEService + 两个 BLECharacteristic (READ_UUID 读+notify, WRITE_UUID 写+notify)
  - 厂商广播数据 59 6A 65 D1 79 98 01 01 (iOS 配对识别, 字节序不可变)
  - Bluefruit.setConnectCallback / setDisconnectCallback / chr.setWriteCallback

与 ESP32 版的关键差异:
  - 删除 esp_efuse_mac_get_default / esp_base_mac_addr_set: nRF52840 的 MAC 由
    SoftDevice 器件 ID 决定, 跨重启天然稳定 (比 ESP32 更可靠), 但与 ESP32 不同,
    故 Trio 首次需重新配对一次 (见 README)。
  - 回调模型: ESP32 用继承 BLE*Callbacks; Adafruit 用全局静态函数回调 + 一个
    静态 gattServer 指针转发到实例 (Bluefruit 回调不支持用户数据传参)。
================================================================================
*/

#ifndef M640G_GATT_SERVER_H
#define M640G_GATT_SERVER_H

#include <Arduino.h>
#include <string>
#include <vector>
#include <bluefruit.h>
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

// ---------- Bluefruit 静态回调转发 ----------
// Adafruit Bluefruit 的回调只接受无用户数据的静态函数, 故用全局指针转发到当前
// GATTServer 实例。(pump_simulator.h 全局只有一个 GATTServer 实例 gattServer)
//
// 关键差异: ESP32 BLE 有 onConnect/onDisconnect 两个独立回调; Adafruit Bluefruit
// 只有一个 setEventCallback(ble_evt_t*), 需在内部按 evt->header.evt_id 分发。
// gattEventCallback 负责识别 BLE_GAP_EVT_CONNECTED / BLE_GAP_EVT_DISCONNECTED
// 并转发到原有的 onConnect / onDisconnect 逻辑。
static GATTServer* sActiveGatt = nullptr;

void gattEventCallback(ble_evt_t* evt);
void gattWriteCallback(uint16_t conn_handle, BLECharacteristic* chr, uint8_t* data, uint16_t len);
void gattCccdWriteCallback(uint16_t conn_handle, BLECharacteristic* chr, uint16_t cccd_value);

// ---------- GATTServer 类 ----------
class GATTServer {
public:
    bool isRunning = false;
    WriteRequestCallback onWriteRequest = nullptr;
    SubscribeCallback onSubscribe = nullptr;
    DisconnectCallback onDisconnect = nullptr;
    ConnectCallback onConnect = nullptr;
    bool advertisingSuspended = false;

    GATTServer() : bledis(nullptr), notifyMutex(nullptr) {}

    ~GATTServer() {
        if (notifyMutex) {
            vSemaphoreDelete(notifyMutex);
        }
    }

    void start() {
        if (isRunning) {
            Serial.println("[GATT] Server already running");
            return;
        }

        Serial.println("[GATT] Initializing Bluefruit BLE...");

        // 配置 Peripheral 连接参数: MTU=247, 事件长度, HVN 队列, WRITE CMD 队列
        // 必须在 Bluefruit.begin() 之前调用。MTU 247 支持 244 字节有效 notify 数据,
        // 这样 SYNCHRONIZE 响应 46 字节可一次性发送, iOS 能正确解析。
        Bluefruit.configPrphConn(247, BLE_GAP_EVENT_LENGTH_DEFAULT,
                                 BLE_GATTS_HVN_TX_QUEUE_SIZE_DEFAULT,
                                 BLE_GATTC_WRITE_CMD_TX_QUEUE_SIZE_DEFAULT);
        Serial.println("[GATT] Peripheral MTU configured to 247");

        // 初始化 Bluefruit: maxPrph=1 (Peripheral 角色, 作为 GATT Server 广播), maxCentral=0
        Bluefruit.begin(1, 0);
        // 限制发射功率, 省电 (0 dBm 足够近距离配对)
        Bluefruit.setTxPower(4);
        // 设备名 (iOS 扫描显示 + 用于广播包)
        Bluefruit.setName("MT");
        // 设置连接参数: iOS 要求 15ms - 4s, 与 ESP32 版 setMinPreferred(0x06)/setMaxPreferred(0x12) 对齐
        Bluefruit.Periph.setConnInterval(6, 12);  // 单位 1.25ms -> 7.5ms / 15ms
        Serial.println("[GATT] Bluefruit initialized with name 'MT'");

        // 连接/断开回调: Adafruit Bluefruit 用单一 setEventCallback(ble_evt_t*),
        // 不像 ESP32 有 setConnectCallback/setDisconnectCallback。在回调内部按事件 ID 分发。
        sActiveGatt = this;
        Bluefruit.setEventCallback(gattEventCallback);

        // 创建 GATT 服务 + 两个特征值
        Serial.println("[GATT] Creating BLE service...");
        bleService = new BLEService(SERVICE_UUID);

        // 写入特征 (可写 + notify): Trio 下发命令走这里, 也用于 sendResponse 回包
        // Adafruit 用 CHR_PROPS_* 枚举 (ESP32 是 PROPERTY_*)。
        // CHR_PROPS_WRITE = 写需响应; CHR_PROPS_WRITE_WO_RESP = 写无需响应。
        // CHR_PROPS_NOTIFY = 允许 iOS 订阅后接收 notify 回包 (与 ESP32 版 PROPERTY_NOTIFY 对齐)
        writeChr = new BLECharacteristic(WRITE_UUID,
            CHR_PROPS_WRITE | CHR_PROPS_WRITE_WO_RESP | CHR_PROPS_NOTIFY);
        writeChr->setPermission(SECMODE_OPEN, SECMODE_OPEN);  // 读/写均开放
        writeChr->setMaxLen(247);  // 兼容 MTU=247 下的大包 notify
        writeChr->setWriteCallback(gattWriteCallback, true);  // true = 延迟到主循环执行, 避免 ISR 上下文 notify 冲突
        Serial.println("[GATT] Write characteristic created");

        // 读取/通知特征 (可读 + notify): 泵向 Trio 上报走这里
        readChr = new BLECharacteristic(READ_UUID,
            CHR_PROPS_READ | CHR_PROPS_NOTIFY);
        readChr->setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);  // 只读, CCCD 可写由底层自动处理
        readChr->setMaxLen(247);  // 兼容 MTU=247 下的大包 notify
        readChr->setCccdWriteCallback(gattCccdWriteCallback, true);  // 监听 iOS 订阅/取消订阅
        Serial.println("[GATT] Read characteristic created");

        // 启动服务并注册特征值
        bleService->begin();
        writeChr->begin();
        readChr->begin();
        // 给读特征值设一个初始空值 (避免 iOS 首次读到脏数据)
        readChr->write(nullptr, 0);

        Serial.println("[GATT] BLE service started");

        // 开始广播
        Serial.println("[GATT] Starting BLE advertising...");
        startAdvertising();
        Serial.println("[GATT] BLE advertising started");

        isRunning = true;
        Serial.println("[GATT] GATT Server is now running! v20250621-1");

        // 创建 notify 互斥量, 防止多个任务并发调用 notify 导致 HVN 队列竞争
        notifyMutex = xSemaphoreCreateMutex();
        if (notifyMutex == nullptr) {
            Serial.println("[GATT][E] Failed to create notify mutex");
        }
    }

    void stop() {
        if (!isRunning) return;
        Bluefruit.Advertising.stop();
        isRunning = false;
    }

    void disconnectAll() {
        uint16_t conn = Bluefruit.connHandle();
        if (conn != BLE_CONN_HANDLE_INVALID) {
            Bluefruit.disconnect(conn);
        }
    }

    void startAdvertising() {
        if (advertisingSuspended) {
            Serial.println("[ADV] Advertising suspended, skipping...");
            return;
        }
        Serial.println("[ADV] Configuring BLE advertising...");

        Bluefruit.Advertising.stop();
        Bluefruit.Advertising.clearData();

        // 广播参数: 允许主动扫描响应
        Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
        // 设备名 (MT)
        Bluefruit.Advertising.addName();

        // 厂商数据 (iOS 配对识别用, 字节序和内容必须与 ESP32 版一字不差):
        // [0-1] company id LE (0x6A59), [2-5] pump SN, [6] device type, [7] version
        const uint8_t mfg[] = {
            0x59, 0x6A,
            0x65, 0xD1, 0x79, 0x98,
            0x01,
            0x01,
        };
        Bluefruit.Advertising.addData(
            BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA, mfg, sizeof(mfg));

        Serial.print("[ADV] Manufacturer data (");
        Serial.print(sizeof(mfg));
        Serial.print(" bytes): ");
        for (size_t i = 0; i < sizeof(mfg); i++) {
            Serial.printf("%02X ", mfg[i]);
        }
        Serial.println("");

        // 间隔: 20ms - 150ms (与 ESP32 的 min 0x06 / max 0x12 量级相当)
        Bluefruit.Advertising.setInterval(32, 243);  // 单位 0.625ms -> 20ms / 152ms
        // 超时: 0 = 永久广播
        Bluefruit.Advertising.setFastTimeout(30);

        // 将 Service UUID 加入 Scan Response (扫描响应包),
        // iOS 主动扫描时能读到 UUID, 用于 Trio/Loop 识别设备。
        // 注意: 放在广播包里会增大广播包, 可能超出 31 字节; 放 Scan Response 更安全。
        Bluefruit.ScanResponse.clearData();
        Bluefruit.ScanResponse.addUuid(BLEUuid(SERVICE_UUID));
        Bluefruit.ScanResponse.addName();

        Serial.println("[ADV] Starting BLE advertising...");
        // Adafruit nRF52: start() 参数是广播秒数, 0 = 持续广播
        Bluefruit.Advertising.start(0);
        Serial.println("[ADV] BLE advertising is now active!");
        Serial.println("[ADV] Device name: MT");
        Serial.println("[ADV] Service UUID: " + String(SERVICE_UUID));
        Serial.println("[ADV] Waiting for mobile device to connect...");
    }

    void stopAdvertising() {
        Bluefruit.Advertising.stop();
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
        readChr->notify(payload.data(), payload.size());
        return true;
    }

    bool sendNotificationWithCrcHack(const uint8_t* data, size_t len) {
        return sendNotification(data, len, true);
    }

    // 原始通知 (不做 CRC 修正) —— pump_simulator.h 主要走这个
    // 直接调用 notify, 与 ESP32 版行为一致。不经过 sendNotifyLocked,
    // 避免互斥锁在 readChr 上可能导致的死锁或 HVN 队列竞争。
    bool sendRawNotification(const uint8_t* data, size_t len) {
        if (readChr == nullptr) {
            Serial.println("[GATT][E] sendRawNotification: readChr is null");
            return false;
        }
        if (!Bluefruit.connected()) {
            Serial.println("[GATT][E] sendRawNotification: not connected");
            return false;
        }
        bool ok = readChr->notify(data, len);
        Serial.print("[GATT][I] raw notify sent, len=");
        Serial.print(len);
        Serial.print(" result=");
        Serial.println(ok ? "OK" : "FAIL");
        return ok;
    }

    // 响应: 通过写特征值 notify (Trio 只监听写特征的响应)
    bool sendResponse(const uint8_t* data, size_t len) {
        Serial.print("[GATT][I] sendResponse called, len=");
        Serial.println(len);
        return sendNotifyLocked(writeChr, "response notify", data, len);
    }

private:
    bool sendNotifyLocked(BLECharacteristic* chr, const char* name, const uint8_t* data, size_t len) {
        if (chr == nullptr) {
            Serial.print("[GATT][E] ");
            Serial.print(name);
            Serial.println(": characteristic is null");
            return false;
        }
        if (!Bluefruit.connected()) {
            Serial.print("[GATT][E] ");
            Serial.print(name);
            Serial.println(": not connected");
            return false;
        }

        // 加锁, 串行化 notify 调用, 避免 HVN 队列竞争
        if (notifyMutex && xSemaphoreTake(notifyMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            Serial.print("[GATT][E] ");
            Serial.print(name);
            Serial.println(": failed to take notify mutex");
            return false;
        }

        // Bluefruit 内部会根据当前协商的 MTU 自动在 ATT 层分片,
        // iOS 底层会自动重组。不要应用层手动分包。
        bool ok = false;
        for (int retry = 0; retry < 5; retry++) {
            ok = chr->notify(data, len);
            if (ok) break;
            // HVN 队列暂时满, 短暂等待后重试
            delayMicroseconds(500);
        }

        Serial.print("[GATT][I] ");
        Serial.print(name);
        Serial.print(" sent, len=");
        Serial.print(len);
        Serial.print(" result=");
        Serial.println(ok ? "OK" : "FAIL");

        if (notifyMutex) xSemaphoreGive(notifyMutex);
        return ok;
    }

private:
    BLEService* bleService;
    BLECharacteristic* readChr;
    BLECharacteristic* writeChr;
    BLEDis* bledis;  // Device Information Service (可选, 当前未启用)
    SemaphoreHandle_t notifyMutex;  // 串行化 notify 调用

    friend void gattEventCallback(ble_evt_t*);
    friend void gattWriteCallback(uint16_t, BLECharacteristic*, uint8_t*, uint16_t);
    friend void gattCccdWriteCallback(uint16_t, BLECharacteristic*, uint16_t);
};

// ---------- 静态回调实现 (转发到 GATTServer 实例) ----------

// 统一事件回调: Adafruit Bluefruit 的 setEventCallback 只给一个入口,
// 在这里按 evt_id 分发到 connect / disconnect 逻辑。
inline void gattEventCallback(ble_evt_t* evt) {
    if (evt == nullptr || sActiveGatt == nullptr) return;

    // evt_id 编码: 高 8 位是模块 BLE_GAP_EVT/BLE_GATTS_EVT..., 低 8 位是子事件号。
    // 直接比对完整的 BLE_GAP_EVT_CONNECTED / BLE_GAP_EVT_DISCONNECTED 即可。
    if (evt->header.evt_id == BLE_GAP_EVT_CONNECTED) {
        Serial.println("");
        Serial.println("========================================");
        Serial.println("[BLE] CLIENT CONNECTED!");
        Serial.println("========================================");
        Serial.println("");

        if (sActiveGatt->onConnect) {
            sActiveGatt->onConnect();
        }
    } else if (evt->header.evt_id == BLE_GAP_EVT_DISCONNECTED) {
        Serial.println("");
        Serial.println("========================================");
        Serial.println("[BLE] CLIENT DISCONNECTED!");
        Serial.println("========================================");
        Serial.println("");
        if (sActiveGatt->onDisconnect) {
            sActiveGatt->onDisconnect();
        }
        Serial.println("[BLE] Restarting advertising...");
        sActiveGatt->startAdvertising();
    }
    // 其他事件 (扫描响应、配对等) 不处理, pump_simulator.h 不依赖。
}

inline void gattWriteCallback(uint16_t conn_handle, BLECharacteristic* chr,
                              uint8_t* data, uint16_t len) {
    (void)conn_handle;
    (void)chr;
    if (sActiveGatt && sActiveGatt->onWriteRequest) {
        if (len > 0) {
            sActiveGatt->onWriteRequest(data, len);
        }
    }
}

// CCCD (Client Characteristic Configuration Descriptor) 写入回调:
// iOS 订阅/取消订阅 notify 时会写入 0x2902 描述符, 触发此回调。
// value 的 bit0 = notify, bit1 = indicate。
// 必须捕获此事件并设置 isSubscribed, 否则 pump_simulator.h 中所有 notify 都不会发送。
inline void gattCccdWriteCallback(uint16_t conn_handle, BLECharacteristic* chr,
                                  uint16_t cccd_value) {
    (void)conn_handle;
    (void)chr;
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

} // namespace M640GKit

#endif // M640G_GATT_SERVER_H
