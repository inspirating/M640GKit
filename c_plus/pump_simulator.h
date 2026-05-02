/*
================================================================================
M640GKit ESP32 泵模拟器核心 (C++ 版本)
================================================================================

该程序模拟 M640G 胰岛素泵, 作为 GATT Server 运行,
供 iOS Loop app 或其他 BLE 客户端连接和通信

对应 Python: pump_simulator.py
================================================================================
*/

#ifndef M640G_PUMP_SIMULATOR_H
#define M640G_PUMP_SIMULATOR_H

#include <Arduino.h>
#include <vector>
#include <cstdint>
#include <cstring>

#include "enums.h"
#include "encryption/crc8.h"
#include "encryption/crypto.h"
#include "packets/base_packet.h"
#include "packets/authorize_packet.h"
#include "packets/synchronize_packet.h"
#include "packets/subscribe_packet.h"
#include "packets/bolus_packet.h"
#include "packets/basal_packet.h"
#include "packets/pump_control_packet.h"
#include "packets/time_packet.h"
#include "packets/misc_packet.h"
#include "pump_manager/gatt_server.h"
#include "pump_manager/connection_tracker.h"

namespace M640GKit {

// 常量定义
static constexpr const char* PUMP_NAME = "MT";
static constexpr uint8_t PUMP_SN[4] = {0x28, 0xD8, 0x12, 0x4A};
static constexpr uint8_t DEVICE_TYPE = 1;
static constexpr const char* SW_VERSION = "1.0.0";
static constexpr uint16_t MANUFACTURER_ID = 0x6A59;

static constexpr double MAX_RESERVOIR = 300.0;
static constexpr double MAX_BOLUS = 30.0;
static constexpr double MAX_BASAL_RATE = 60.0;
static constexpr double DEFAULT_HOURLY_MAX = 25.0;
static constexpr double DEFAULT_DAILY_MAX = 200.0;

// 日志级别
enum class LogLevel : uint8_t {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3
};

class Logger {
public:
    static LogLevel currentLevel;

    static void setLevel(LogLevel level) {
        currentLevel = level;
    }

    static void log(LogLevel level, const char* tag, const char* message) {
        if (static_cast<uint8_t>(level) >= static_cast<uint8_t>(currentLevel)) {
            const char* levelStr = (level == LogLevel::DEBUG) ? "D" :
                                   (level == LogLevel::INFO) ? "I" :
                                   (level == LogLevel::WARNING) ? "W" : "E";
            Serial.printf("[%s][%s] %s\n", tag, levelStr, message);
        }
    }

    static void debug(const char* msg) { log(LogLevel::DEBUG, "BLE", msg); }
    static void info(const char* msg) { log(LogLevel::INFO, "BLE", msg); }
    static void warning(const char* msg) { log(LogLevel::WARNING, "BLE", msg); }
    static void error(const char* msg) { log(LogLevel::ERROR, "BLE", msg); }
};

LogLevel Logger::currentLevel = LogLevel::INFO;

// 模拟器状态
enum class SimulatorState : uint8_t {
    INITIALIZING = 0,
    READY,
    RUNNING,
    SUSPENDED,
    EJECTING,
    ERROR
};

// 大剂量数据结构
struct BolusInfo {
    uint8_t type;
    double amount;
    uint32_t startTime;
};

// 临时基础率数据结构
struct TempBasalInfo {
    uint8_t type;
    double rate;
    uint32_t startTime;
};

class M640GPumpSimulator {
public:
    M640GPumpSimulator() : initialized(false), lastUpdateTime(0), updateIntervalMs(100),
        patchState(PatchState::ACTIVE), simulatorState(SimulatorState::INITIALIZING),
        reservoir(200.0), activeInsulin(0.0), batteryVoltage(3.8), batteryLevel(100),
        patchStartTime(0), totalElapsedTime(0), currentBolus(nullptr),
        bolusDeliveryProgress(0), tempBasal(nullptr), tempBasalRemaining(0),
        isConnected(false), isSubscribed(false), sessionToken{0}, pumpTimezone(0),
        timeSyncPending(false), sequenceNumber(0), pingCounter(0), lastPingTime(0),
        connectionTimeoutMs(15000), lastActivityTime(0) {
        currentBolus = nullptr;
        tempBasal = nullptr;
    }

    void setup() {
        Logger::info("=");
        Logger::info("M640G 泵模拟器初始化");
        Logger::info("=");

        // 初始化随机数
        randomSeed(millis());

        // 生成会话令牌
        generateSessionToken();

        // 设置初始时间
        patchStartTime = millis() / 1000;

        // 创建默认基础率配置文件
        createDefaultBasalProfile();

        // 初始化 GATT Server
        gattServer.onWriteRequest = handleWriteRequestStatic;
        gattServer.onSubscribe = handleSubscribeStatic;
        gattServer.onConnect = handleConnectStatic;
        gattServer.onDisconnect = handleDisconnectStatic;
        gattServer.start();

        simulatorState = SimulatorState::READY;
        initialized = true;
        lastUpdateTime = millis();

        Logger::info("=");
        Logger::info("M640G 泵模拟器启动");
        Logger::info("=");
        Logger::info("设备名称: MT");
        Logger::info("序列号: 28D8124A");
        Logger::info("设备类型: 1");
        Logger::info("软件版本: 1.0.0");
        Logger::info("=");
    }

    void loop() {
        if (!initialized) {
            Logger::error("模拟器未初始化, 请先调用 setup()");
            return;
        }

        uint32_t currentTime = millis();
        if (currentTime - lastUpdateTime >= updateIntervalMs) {
            lastUpdateTime = currentTime;
            update();
        }
    }

private:
    bool initialized;
    uint32_t lastUpdateTime;
    uint16_t updateIntervalMs;

    // 泵状态
    PatchState patchState;
    SimulatorState simulatorState;

    // 储药器和胰岛素
    double reservoir;
    double activeInsulin;

    // 电池
    double batteryVoltage;
    uint8_t batteryLevel;

    // 时间
    uint32_t patchStartTime;
    uint32_t totalElapsedTime;

    // 大剂量
    BolusInfo* currentBolus;
    uint8_t bolusDeliveryProgress;
    std::vector<BolusInfo> bolusHistory;

    // 基础率
    std::vector<uint8_t> basalProfile;
    TempBasalInfo* tempBasal;
    double tempBasalRemaining;

    // 连接状态
    bool isConnected;
    bool isSubscribed;
    std::vector<uint32_t> authenticatedClients;

    // 会话
    uint8_t sessionToken[4];
    int16_t pumpTimezone;
    bool timeSyncPending;

    // GATT Server
    GATTServer gattServer;
    ConnectionTracker connectionTracker;

    // 序列号和计时器
    uint8_t sequenceNumber;
    uint32_t pingCounter;
    uint32_t lastPingTime;
    uint16_t connectionTimeoutMs;
    uint32_t lastActivityTime;

    // 数据包缓冲
    std::vector<uint8_t> packetBuffer;
    uint8_t expectedPacketLen;
    uint8_t currentCmdType;
    uint8_t currentSeqNum;
    uint8_t currentPkgIndex;

    void generateSessionToken() {
        for (int i = 0; i < 4; i++) {
            sessionToken[i] = random(256);
        }
    }

    void createDefaultBasalProfile() {
        double defaultRates[32] = {
            0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6,
            0.6, 0.6, 0.6, 0.6, 0.7, 0.7, 0.8, 0.9,
            1.0, 1.0, 0.9, 0.8, 0.8, 0.8, 0.8, 0.7,
            0.7, 0.7, 0.8, 0.9, 1.0, 0.9, 0.8, 0.7
        };
        for (int i = 0; i < 32; i++) {
            uint16_t rawValue = static_cast<uint16_t>(defaultRates[i] / 0.05);
            basalProfile.push_back(rawValue & 0xFF);
            basalProfile.push_back((rawValue >> 8) & 0xFF);
        }
    }

    void update() {
        totalElapsedTime += updateIntervalMs / 1000;
        updateBolusDelivery();
        updateTempBasal();
        updatePrimeProgress();
        checkStateNotifications();
        sendPingHeartbeat();
        checkConnectionTimeout();

        if (random(1000) == 0) {
            batteryVoltage = max(2.8, batteryVoltage - 0.01);
            batteryLevel = max(0, batteryLevel - 1);
        }

        if (isSubscribed && isConnected) {
            sendPeriodicNotification();
        }
    }

    void updateBolusDelivery() {
        if (currentBolus == nullptr) return;

        bolusDeliveryProgress += 2;
        double delivered = currentBolus->amount * (bolusDeliveryProgress / 100.0);
        activeInsulin += delivered;

        if (bolusDeliveryProgress >= 100) {
            bolusHistory.push_back(*currentBolus);
            reservoir = max(0.0, reservoir - currentBolus->amount);
            delete currentBolus;
            currentBolus = nullptr;
            bolusDeliveryProgress = 0;
            Logger::info("大剂量输送完成");
        }
    }

    void updateTempBasal() {
        if (tempBasal == nullptr) return;

        tempBasalRemaining -= updateIntervalMs / 60000.0;

        if (tempBasalRemaining <= 0) {
            Logger::info("临时基础率结束");
            delete tempBasal;
            tempBasal = nullptr;
            tempBasalRemaining = 0;
        }
    }

    void updatePrimeProgress() {
        static uint8_t primeProgress = 0;
        if (patchState == PatchState::PRIMING) {
            primeProgress++;
            if (primeProgress >= 240) {
                patchState = PatchState::PRIMED;
                Logger::info("预充完成");
            }
        }
    }

    void checkStateNotifications() {
        static PatchState lastNotifiedState = PatchState::NONE;
        if (patchState == lastNotifiedState) return;

        bool shouldNotify = false;
        if (patchState == PatchState::SUSPENDED) {
            if (totalElapsedTime % 7200 == 0) {
                Logger::info("通知: 每日最大暂停");
                shouldNotify = true;
            }
        } else if (patchState == PatchState::OCCLUSION) {
            Logger::info("通知: 堵管报警");
            shouldNotify = true;
        } else if (patchState == PatchState::PATCH_FAULT) {
            Logger::info("通知: Patch 故障");
            shouldNotify = true;
        } else if (patchState == PatchState::RESERVOIR_EMPTY) {
            Logger::info("通知: 储药器空");
            shouldNotify = true;
        }

        if (shouldNotify) {
            lastNotifiedState = patchState;
            sendStateNotification();
        }
    }

    void sendStateNotification() {
        std::vector<uint8_t> syncData = buildSynchronizeData();
        gattServer.sendNotification(syncData.data(), syncData.size(), false);
    }

    void sendPingHeartbeat() {
        if (!isSubscribed || !isConnected) return;

        uint32_t currentTime = millis();
        if (currentTime - lastPingTime >= 5000) {
            lastPingTime = currentTime;
            pingCounter++;

            uint8_t pingData[8] = {0x07, 0x00, sequenceNumber, 0, 0, 0, 0, 0};
            uint8_t crc = crc8Calculate(pingData, 6);
            pingData[6] = crc;
            pingData[7] = 0;

            gattServer.sendNotificationWithCrcHack(pingData, 8);
        }
    }

    void checkConnectionTimeout() {
        if (!isConnected || isSubscribed) return;

        uint32_t currentTime = millis();
        if (currentTime - lastActivityTime > connectionTimeoutMs) {
            Logger::warning("未认证连接超时,清理状态");
            connectionTracker.onDisconnect("认证超时");
            isConnected = false;
            authenticatedClients.clear();
        }
    }

    void sendPeriodicNotification() {
        if (totalElapsedTime % 5 == 0) {
            sendSynchronizeNotification();
        }
    }

    void sendSynchronizeNotification() {
        std::vector<uint8_t> syncData = buildSynchronizeData();
        gattServer.sendNotification(syncData.data(), syncData.size(), false);
    }

    std::vector<uint8_t> buildSynchronizeData() {
        std::vector<uint8_t> data;
        data.push_back(static_cast<uint8_t>(patchState));

        uint16_t fieldMask = (
            MASK_SUSPEND | MASK_NORMAL_BOLUS | MASK_EXTENDED_BOLUS | MASK_BASAL |
            MASK_SETUP | MASK_RESERVOIR | MASK_START_TIME | MASK_BATTERY |
            MASK_STORAGE | MASK_ALARM | MASK_AGE | MASK_MAGNETO_PLACE |
            MASK_UNUSED_CGM | MASK_UNUSED_COMMAND_CONFIRM |
            MASK_UNUSED_AUTO_STATUS | MASK_UNUSED_LEGACY
        );
        data.push_back(fieldMask & 0xFF);
        data.push_back((fieldMask >> 8) & 0xFF);

        if (fieldMask & MASK_SUSPEND) {
            data.push_back(totalElapsedTime & 0xFF);
            data.push_back((totalElapsedTime >> 8) & 0xFF);
            data.push_back((totalElapsedTime >> 16) & 0xFF);
            data.push_back((totalElapsedTime >> 24) & 0xFF);
        }

        if (fieldMask & MASK_NORMAL_BOLUS) {
            if (currentBolus) {
                data.push_back(currentBolus->type | (bolusDeliveryProgress >= 100 ? 0x80 : 0));
                uint16_t delivered = static_cast<uint16_t>(currentBolus->amount * (bolusDeliveryProgress / 100.0) / 0.05);
                data.push_back(delivered & 0xFF);
                data.push_back((delivered >> 8) & 0xFF);
            } else {
                data.push_back(0);
                data.push_back(0);
                data.push_back(0);
            }
        }

        if (fieldMask & MASK_EXTENDED_BOLUS) {
            data.push_back(0);
            data.push_back(0);
            data.push_back(0);
        }

        if (fieldMask & MASK_BASAL) {
            data.push_back(tempBasal ? static_cast<uint8_t>(BasalType::ABSOLUTE_TEMP) : static_cast<uint8_t>(BasalType::STANDARD));
            data.push_back(0);
            data.push_back(0);
            uint16_t patchId = random(65535) + 1;
            data.push_back(patchId & 0xFF);
            data.push_back((patchId >> 8) & 0xFF);
            uint32_t startTime = patchStartTime;
            data.push_back(startTime & 0xFF);
            data.push_back((startTime >> 8) & 0xFF);
            data.push_back((startTime >> 16) & 0xFF);
            data.push_back((startTime >> 24) & 0xFF);
            uint16_t rate = tempBasal ? static_cast<uint16_t>(tempBasal->rate / 0.05) : static_cast<uint16_t>(0.6 / 0.05);
            uint16_t delivery = tempBasal ? rate : 0;
            uint32_t rateDelivery = (static_cast<uint32_t>(delivery) << 12) | (rate & 0x0FFF);
            data.push_back(rateDelivery & 0xFF);
            data.push_back((rateDelivery >> 8) & 0xFF);
            data.push_back((rateDelivery >> 16) & 0xFF);
        }

        if (fieldMask & MASK_SETUP) {
            data.push_back(patchState == PatchState::PRIMING ? 100 : 0);
        }

        if (fieldMask & MASK_RESERVOIR) {
            uint16_t reservoirRaw = static_cast<uint16_t>(reservoir / 0.05);
            data.push_back(reservoirRaw & 0xFF);
            data.push_back((reservoirRaw >> 8) & 0xFF);
        }

        if (fieldMask & MASK_START_TIME) {
            data.push_back(patchStartTime & 0xFF);
            data.push_back((patchStartTime >> 8) & 0xFF);
            data.push_back((patchStartTime >> 16) & 0xFF);
            data.push_back((patchStartTime >> 24) & 0xFF);
        }

        if (fieldMask & MASK_BATTERY) {
            uint16_t voltageA = static_cast<uint16_t>(batteryVoltage * 512);
            uint16_t voltageB = static_cast<uint16_t>(batteryVoltage * 512);
            uint32_t packed = (static_cast<uint32_t>(voltageB) << 12) | (voltageA & 0x0FFF);
            data.push_back(packed & 0xFF);
            data.push_back((packed >> 8) & 0xFF);
            data.push_back((packed >> 16) & 0xFF);
        }

        if (fieldMask & MASK_STORAGE) {
            data.push_back(0);
            data.push_back(0);
            data.push_back(0);
            data.push_back(0);
        }

        if (fieldMask & MASK_ALARM) {
            data.push_back(0);
            data.push_back(0);
            data.push_back(0);
            data.push_back(0);
        }

        if (fieldMask & MASK_AGE) {
            data.push_back(totalElapsedTime & 0xFF);
            data.push_back((totalElapsedTime >> 8) & 0xFF);
            data.push_back((totalElapsedTime >> 16) & 0xFF);
            data.push_back((totalElapsedTime >> 24) & 0xFF);
        }

        if (fieldMask & MASK_MAGNETO_PLACE) {
            data.push_back(100);
            data.push_back(0);
        }

        if (fieldMask & MASK_UNUSED_CGM) {
            for (int i = 0; i < 5; i++) data.push_back(0);
        }

        if (fieldMask & MASK_UNUSED_COMMAND_CONFIRM) {
            data.push_back(0);
            data.push_back(0);
        }

        if (fieldMask & MASK_UNUSED_AUTO_STATUS) {
            data.push_back(0);
            data.push_back(0);
        }

        if (fieldMask & MASK_UNUSED_LEGACY) {
            data.push_back(0);
            data.push_back(0);
        }

        return data;
    }

    // 静态回调处理函数
    static void handleWriteRequestStatic(const uint8_t* data, size_t len) {
        // 这里需要通过单例或全局指针访问实例
        // 简化处理: 使用全局实例
        extern M640GPumpSimulator* gSimulator;
        if (gSimulator) {
            gSimulator->handleWriteRequest(data, len);
        }
    }

    static void handleSubscribeStatic(bool subscribed) {
        extern M640GPumpSimulator* gSimulator;
        if (gSimulator) {
            gSimulator->handleSubscribe(subscribed);
        }
    }

    static void handleConnectStatic() {
        extern M640GPumpSimulator* gSimulator;
        if (gSimulator) {
            gSimulator->handleBleConnect();
        }
    }

    static void handleDisconnectStatic() {
        extern M640GPumpSimulator* gSimulator;
        if (gSimulator) {
            gSimulator->handleBleDisconnect();
        }
    }

    void handleWriteRequest(const uint8_t* data, size_t len) {
        lastActivityTime = millis();

        if (len < 4) {
            Logger::warning("数据长度太短");
            return;
        }

        uint8_t packetLen = data[0];
        uint8_t cmdType = data[1];
        uint8_t seqNum = data[2];
        uint8_t pkgIndex = data[3];

        if (cmdType == 0x00) return;

        if (packetBuffer.empty()) {
            packetBuffer.assign(data, data + len - 1);
            expectedPacketLen = packetLen;
            currentCmdType = cmdType;
            currentSeqNum = seqNum;
            currentPkgIndex = pkgIndex;

            if (packetBuffer.size() >= expectedPacketLen) {
                processCompleteCommand(packetBuffer.data(), expectedPacketLen);
                packetBuffer.clear();
                expectedPacketLen = 0;
                currentCmdType = 0;
            }
        } else {
            if (cmdType != currentCmdType || pkgIndex != currentPkgIndex + 1) {
                Logger::error("包索引不匹配");
                sendErrorResponse(static_cast<CommandType>(currentCmdType), 0x0102);
                packetBuffer.clear();
                expectedPacketLen = 0;
                currentCmdType = 0;
                return;
            }

            packetBuffer.insert(packetBuffer.end(), data + 4, data + len - 1);
            currentPkgIndex = pkgIndex;

            if (packetBuffer.size() >= expectedPacketLen) {
                processCompleteCommand(packetBuffer.data(), expectedPacketLen);
                packetBuffer.clear();
                expectedPacketLen = 0;
                currentCmdType = 0;
            }
        }
    }

    void handleSubscribe(bool subscribed) {
        isSubscribed = subscribed;
        if (subscribed) {
            connectionTracker.onConnect();
            Logger::info("订阅状态变化: 已订阅");
        } else {
            connectionTracker.onDisconnect("客户端取消订阅");
            Logger::info("订阅状态变化: 已取消订阅");
        }
    }

    void handleBleConnect() {
        Logger::info("BLE 客户端已连接");
        lastActivityTime = millis();
    }

    void handleBleDisconnect() {
        Logger::info("BLE 客户端已断开");
        isConnected = false;
        isSubscribed = false;
        authenticatedClients.clear();
        connectionTracker.onDisconnect("BLE 断开");
    }

    void processCompleteCommand(const uint8_t* data, uint8_t len) {
        uint8_t cmdType = data[1];
        uint8_t seqNum = data[2];

        switch (static_cast<CommandType>(cmdType)) {
            case CommandType::AUTH_REQ:
                handleAuthRequest(data, len, seqNum);
                break;
            case CommandType::SYNCHRONIZE:
                handleSynchronizeRequest(data, len, seqNum);
                break;
            case CommandType::SUBSCRIBE:
                handleSubscribeRequest(data, len, seqNum);
                break;
            case CommandType::GET_DEVICE_TYPE:
                handleGetDeviceTypeRequest(data, len, seqNum);
                break;
            case CommandType::GET_TIME:
                handleGetTimeRequest(data, len, seqNum);
                break;
            case CommandType::SET_TIME:
                handleSetTimeRequest(data, len, seqNum);
                break;
            case CommandType::SET_TIME_ZONE:
                handleSetTimeZoneRequest(data, len, seqNum);
                break;
            case CommandType::PRIME:
                handlePrimeRequest(data, len, seqNum);
                break;
            case CommandType::SET_BOLUS:
                handleSetBolusRequest(data, len, seqNum);
                break;
            case CommandType::CANCEL_BOLUS:
                handleCancelBolusRequest(data, len, seqNum);
                break;
            case CommandType::READ_BOLUS_STATE:
                handleReadBolusStateRequest(data, len, seqNum);
                break;
            case CommandType::SET_TEMP_BASAL:
                handleSetTempBasalRequest(data, len, seqNum);
                break;
            case CommandType::CANCEL_TEMP_BASAL:
                handleCancelTempBasalRequest(data, len, seqNum);
                break;
            case CommandType::SUSPEND_PUMP:
                handleSuspendRequest(data, len, seqNum);
                break;
            case CommandType::RESUME_PUMP:
                handleResumeRequest(data, len, seqNum);
                break;
            case CommandType::SET_BASAL_PROFILE:
                handleSetBasalProfileRequest(data, len, seqNum);
                break;
            case CommandType::CLEAR_ALARM:
                handleClearAlarmRequest(data, len, seqNum);
                break;
            case CommandType::ACTIVATE:
                handleActivateRequest(data, len, seqNum);
                break;
            case CommandType::STOP_PATCH:
                handleStopPatchRequest(data, len, seqNum);
                break;
            case CommandType::SET_PATCH:
                handleSetPatchRequest(data, len, seqNum);
                break;
            case CommandType::POLL_PATCH:
                handlePollPatchRequest(data, len, seqNum);
                break;
            case CommandType::GET_RECORD:
                handleGetRecordRequest(data, len, seqNum);
                break;
            case CommandType::SET_BOLUS_MOTOR:
                handleSetBolusMotorRequest(data, len, seqNum);
                break;
            default:
                Logger::warning("未知命令类型");
                sendErrorResponse(static_cast<CommandType>(cmdType), 0x0101);
        }
    }

    void handleAuthRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("收到认证请求");

        if (len < 13) {
            Logger::error("认证请求数据长度不足");
            sendErrorResponse(CommandType::AUTH_REQ, 0x0101);
            return;
        }

        uint8_t role = data[4];
        uint8_t clientToken[4] = {data[5], data[6], data[7], data[8]};
        uint8_t clientKey[4] = {data[9], data[10], data[11], data[12]};

        uint8_t reversedSN[4] = {PUMP_SN[3], PUMP_SN[2], PUMP_SN[1], PUMP_SN[0]};
        uint8_t correctKey[4];
        Crypto::genKey(reversedSN, correctKey);

        if (memcmp(clientKey, correctKey, 4) != 0) {
            Logger::error("认证失败: 密钥不匹配");
            sendErrorResponse(CommandType::AUTH_REQ, 0x0201);
            return;
        }

        Logger::info("认证成功");

        uint8_t responseData[5] = {0x02, DEVICE_TYPE, 1, 0, 0};
        sendResponse(CommandType::AUTH_REQ, seqNum, responseData, 5);

        isConnected = true;
        uint32_t token = clientToken[0] | (clientToken[1] << 8) | (clientToken[2] << 16) | (clientToken[3] << 24);
        authenticatedClients.push_back(token);
    }

    void handleSynchronizeRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("收到同步请求");
        std::vector<uint8_t> syncData = buildSynchronizeData();
        sendResponse(CommandType::SYNCHRONIZE, seqNum, syncData.data(), syncData.size());
    }

    void handleSubscribeRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("收到订阅请求");
        isSubscribed = true;
        sendResponse(CommandType::SUBSCRIBE, seqNum, nullptr, 0);
    }

    void handleGetDeviceTypeRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("收到获取设备类型请求");
        uint8_t responseData[10] = {DEVICE_TYPE, 1, 0, 1, 0, 0, PUMP_SN[0], PUMP_SN[1], PUMP_SN[2], PUMP_SN[3]};
        sendResponse(CommandType::GET_DEVICE_TYPE, seqNum, responseData, 10);
    }

    void handleGetTimeRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("收到获取时间请求");
        uint32_t currentTime = millis() / 1000;
        uint8_t responseData[4] = {
            static_cast<uint8_t>(currentTime & 0xFF),
            static_cast<uint8_t>((currentTime >> 8) & 0xFF),
            static_cast<uint8_t>((currentTime >> 16) & 0xFF),
            static_cast<uint8_t>((currentTime >> 24) & 0xFF)
        };
        sendResponse(CommandType::GET_TIME, seqNum, responseData, 4);
    }

    void handleSetTimeRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("收到设置时间请求");
        if (len >= 9) {
            uint32_t newTime = data[5] | (data[6] << 8) | (data[7] << 16) | (data[8] << 24);
            patchStartTime = newTime;
            timeSyncPending = false;
            Logger::info("时间已同步");
        }
        sendResponse(CommandType::SET_TIME, seqNum, nullptr, 0);
    }

    void handleSetTimeZoneRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("收到设置时区请求");
        if (len >= 10) {
            int16_t tzOffset = data[4] | (data[5] << 8);
            pumpTimezone = tzOffset;
            uint32_t timeVal = data[6] | (data[7] << 8) | (data[8] << 16) | (data[9] << 24);
            patchStartTime = timeVal;
            Logger::info("时区和时间已更新");
        }
        sendResponse(CommandType::SET_TIME_ZONE, seqNum, nullptr, 0);
    }

    void handlePrimeRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("收到预充请求");
        if (patchState == PatchState::FILLED || patchState == PatchState::ACTIVE) {
            patchState = PatchState::PRIMING;
            Logger::info("预充已开始");
        } else {
            Logger::warning("当前状态不允许预充");
            sendErrorResponse(CommandType::PRIME, 0x0400);
            return;
        }
        sendResponse(CommandType::PRIME, seqNum, nullptr, 0);
    }

    void handleSetBolusRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("收到设置大剂量请求");

        if (patchState != PatchState::ACTIVE && patchState != PatchState::ACTIVE_ALT) {
            Logger::warning("泵未处于运行状态,无法执行大剂量");
            sendErrorResponse(CommandType::SET_BOLUS, 0x0400);
            return;
        }

        if (len >= 7) {
            uint8_t bolusType = data[4];
            uint16_t amountRaw = data[5] | (data[6] << 8);
            double amount = amountRaw * 0.05;

            if (currentBolus != nullptr) {
                Logger::warning("已有大剂量在执行中");
                sendErrorResponse(CommandType::SET_BOLUS, 0x0201);
                return;
            }

            if (amount > reservoir) {
                Logger::warning("储药器余量不足");
                sendErrorResponse(CommandType::SET_BOLUS, 0x0202);
                return;
            }

            currentBolus = new BolusInfo{bolusType, amount, millis() / 1000};
            bolusDeliveryProgress = 0;
            Logger::info("大剂量已开始输送");
        }
        sendResponse(CommandType::SET_BOLUS, seqNum, nullptr, 0);
    }

    void handleCancelBolusRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("收到取消大剂量请求");
        if (currentBolus) {
            double delivered = currentBolus->amount * (bolusDeliveryProgress / 100.0);
            reservoir += (currentBolus->amount - delivered);
            delete currentBolus;
            currentBolus = nullptr;
            bolusDeliveryProgress = 0;
        }
        sendResponse(CommandType::CANCEL_BOLUS, seqNum, nullptr, 0);
    }

    void handleReadBolusStateRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("收到读取大剂量状态请求");
        uint8_t responseData[10];
        if (currentBolus) {
            responseData[0] = 1;
            responseData[1] = 0x01;
            uint16_t amountRaw = static_cast<uint16_t>(currentBolus->amount / 0.05);
            uint16_t deliveredRaw = static_cast<uint16_t>(amountRaw * bolusDeliveryProgress / 100.0);
            uint16_t remainingRaw = amountRaw - deliveredRaw;
            responseData[2] = deliveredRaw & 0xFF;
            responseData[3] = (deliveredRaw >> 8) & 0xFF;
            responseData[4] = remainingRaw & 0xFF;
            responseData[5] = (remainingRaw >> 8) & 0xFF;
            responseData[6] = amountRaw & 0xFF;
            responseData[7] = (amountRaw >> 8) & 0xFF;
            responseData[8] = 0;
            responseData[9] = 0;
        } else {
            memset(responseData, 0, 10);
        }
        sendResponse(CommandType::READ_BOLUS_STATE, seqNum, responseData, 10);
    }

    void handleSetTempBasalRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("收到设置临时基础率请求");

        if (patchState != PatchState::ACTIVE && patchState != PatchState::ACTIVE_ALT) {
            Logger::warning("泵未处于运行状态,无法设置临时基础率");
            sendErrorResponse(CommandType::SET_TEMP_BASAL, 0x0400);
            return;
        }

        if (len >= 9) {
            uint8_t basalType = data[4];
            uint16_t rateRaw = data[5] | (data[6] << 8);
            uint16_t durationRaw = data[7] | (data[8] << 8);
            double rate = rateRaw * 0.05;

            if (tempBasal) delete tempBasal;
            tempBasal = new TempBasalInfo{basalType, rate, millis() / 1000};
            tempBasalRemaining = durationRaw;
        }

        uint8_t responseData[11];
        responseData[0] = tempBasal ? static_cast<uint8_t>(BasalType::ABSOLUTE_TEMP) : static_cast<uint8_t>(BasalType::STANDARD);
        uint16_t basalValue = tempBasal ? static_cast<uint16_t>(tempBasal->rate / 0.05) : static_cast<uint16_t>(0.6 / 0.05);
        responseData[1] = basalValue & 0xFF;
        responseData[2] = (basalValue >> 8) & 0xFF;
        responseData[3] = 0;
        responseData[4] = 0;
        uint16_t patchId = random(65535) + 1;
        responseData[5] = patchId & 0xFF;
        responseData[6] = (patchId >> 8) & 0xFF;
        uint32_t startTime = patchStartTime;
        responseData[7] = startTime & 0xFF;
        responseData[8] = (startTime >> 8) & 0xFF;
        responseData[9] = (startTime >> 16) & 0xFF;
        responseData[10] = (startTime >> 24) & 0xFF;

        sendResponse(CommandType::SET_TEMP_BASAL, seqNum, responseData, 11);
    }

    void handleCancelTempBasalRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("收到取消临时基础率请求");
        if (tempBasal) {
            delete tempBasal;
            tempBasal = nullptr;
            tempBasalRemaining = 0;
        }

        uint8_t responseData[11];
        responseData[0] = static_cast<uint8_t>(BasalType::STANDARD);
        uint16_t basalValue = static_cast<uint16_t>(0.6 / 0.05);
        responseData[1] = basalValue & 0xFF;
        responseData[2] = (basalValue >> 8) & 0xFF;
        responseData[3] = 0;
        responseData[4] = 0;
        uint16_t patchId = random(65535) + 1;
        responseData[5] = patchId & 0xFF;
        responseData[6] = (patchId >> 8) & 0xFF;
        uint32_t startTime = patchStartTime;
        responseData[7] = startTime & 0xFF;
        responseData[8] = (startTime >> 8) & 0xFF;
        responseData[9] = (startTime >> 16) & 0xFF;
        responseData[10] = (startTime >> 24) & 0xFF;

        sendResponse(CommandType::CANCEL_TEMP_BASAL, seqNum, responseData, 11);
    }

    void handleSuspendRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("收到暂停泵请求");

        if (patchState != PatchState::ACTIVE && patchState != PatchState::ACTIVE_ALT) {
            Logger::warning("泵未处于运行状态,无法暂停");
            sendErrorResponse(CommandType::SUSPEND_PUMP, 0x0400);
            return;
        }

        patchState = PatchState::SUSPENDED;
        simulatorState = SimulatorState::SUSPENDED;

        if (currentBolus) {
            double delivered = currentBolus->amount * (bolusDeliveryProgress / 100.0);
            reservoir += (currentBolus->amount - delivered);
            delete currentBolus;
            currentBolus = nullptr;
            bolusDeliveryProgress = 0;
        }

        if (tempBasal) {
            delete tempBasal;
            tempBasal = nullptr;
            tempBasalRemaining = 0;
        }

        sendResponse(CommandType::SUSPEND_PUMP, seqNum, nullptr, 0);
    }

    void handleResumeRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("收到恢复泵请求");

        if (patchState != PatchState::SUSPENDED) {
            Logger::warning("泵未处于暂停状态,无法恢复");
            sendErrorResponse(CommandType::RESUME_PUMP, 0x0400);
            return;
        }

        patchState = PatchState::ACTIVE;
        simulatorState = SimulatorState::RUNNING;
        sendResponse(CommandType::RESUME_PUMP, seqNum, nullptr, 0);
    }

    void handleSetBasalProfileRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("收到设置基础率配置文件请求");
        if (len >= 5) {
            uint8_t profileType = data[4];
        }
        uint8_t responseData[11];
        responseData[0] = static_cast<uint8_t>(BasalType::STANDARD);
        uint16_t basalValue = static_cast<uint16_t>(0.6 / 0.05);
        responseData[1] = basalValue & 0xFF;
        responseData[2] = (basalValue >> 8) & 0xFF;
        responseData[3] = 0;
        responseData[4] = 0;
        uint16_t patchId = random(65535) + 1;
        responseData[5] = patchId & 0xFF;
        responseData[6] = (patchId >> 8) & 0xFF;
        uint32_t startTime = patchStartTime;
        responseData[7] = startTime & 0xFF;
        responseData[8] = (startTime >> 8) & 0xFF;
        responseData[9] = (startTime >> 16) & 0xFF;
        responseData[10] = (startTime >> 24) & 0xFF;
        sendResponse(CommandType::SET_BASAL_PROFILE, seqNum, responseData, 11);
    }

    void handleClearAlarmRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("收到清除警报请求");
        if (len >= 6) {
            uint16_t alertType = data[4] | (data[5] << 8);
        }
        sendResponse(CommandType::CLEAR_ALARM, seqNum, nullptr, 0);
    }

    void handleActivateRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("收到激活 Patch 请求");

        if (patchState != PatchState::PRIMED) {
            Logger::warning("Patch 未完成预充,无法激活");
            sendErrorResponse(CommandType::ACTIVATE, 0x0400);
            return;
        }

        patchState = PatchState::ACTIVE;
        simulatorState = SimulatorState::RUNNING;
        reservoir = MAX_RESERVOIR;
        patchStartTime = millis() / 1000;
        totalElapsedTime = 0;

        uint8_t responseData[19];
        uint32_t patchId = random(65535) + 1;
        responseData[0] = patchId & 0xFF;
        responseData[1] = (patchId >> 8) & 0xFF;
        responseData[2] = (patchId >> 16) & 0xFF;
        responseData[3] = (patchId >> 24) & 0xFF;
        uint32_t startTime = patchStartTime;
        responseData[4] = startTime & 0xFF;
        responseData[5] = (startTime >> 8) & 0xFF;
        responseData[6] = (startTime >> 16) & 0xFF;
        responseData[7] = (startTime >> 24) & 0xFF;
        responseData[8] = static_cast<uint8_t>(BasalType::STANDARD);
        uint16_t basalValue = static_cast<uint16_t>(0.6 / 0.05);
        responseData[9] = basalValue & 0xFF;
        responseData[10] = (basalValue >> 8) & 0xFF;
        responseData[11] = 0;
        responseData[12] = 0;
        responseData[13] = patchId & 0xFF;
        responseData[14] = (patchId >> 8) & 0xFF;
        responseData[15] = startTime & 0xFF;
        responseData[16] = (startTime >> 8) & 0xFF;
        responseData[17] = (startTime >> 16) & 0xFF;
        responseData[18] = (startTime >> 24) & 0xFF;
        sendResponse(CommandType::ACTIVATE, seqNum, responseData, 19);
    }

    void handleStopPatchRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("收到停止 Patch 请求");
        patchState = PatchState::STOPPED;
        simulatorState = SimulatorState::EJECTING;
        uint8_t responseData[4] = {0, 0, 0, 0};
        sendResponse(CommandType::STOP_PATCH, seqNum, responseData, 4);
    }

    void handleSetPatchRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("收到设置 Patch 请求");
        sendResponse(CommandType::SET_PATCH, seqNum, nullptr, 0);
    }

    void handlePollPatchRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("收到轮询 Patch 请求");
        std::vector<uint8_t> syncData = buildSynchronizeData();
        sendResponse(CommandType::POLL_PATCH, seqNum, syncData.data(), syncData.size());
    }

    void handleGetRecordRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("收到获取记录请求");
        uint8_t responseData[4] = {0, 0, 0, 0};
        sendResponse(CommandType::GET_RECORD, seqNum, responseData, 4);
    }

    void handleSetBolusMotorRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("收到设置大剂量电机请求");
        if (len >= 7) {
            uint16_t motorSteps = data[4] | (data[5] << 8);
            uint8_t direction = data[6];
        }
        sendResponse(CommandType::SET_BOLUS_MOTOR, seqNum, nullptr, 0);
    }

    void sendResponse(CommandType cmdType, uint8_t seqNum, const uint8_t* data, uint8_t dataLen) {
        uint8_t totalContentLen = 2 + dataLen;

        uint8_t header[4] = {
            static_cast<uint8_t>(totalContentLen + 5),
            static_cast<uint8_t>(cmdType),
            seqNum,
            0
        };

        uint8_t packet[256];
        memcpy(packet, header, 4);
        packet[4] = 0;
        packet[5] = 0;
        if (dataLen > 0) {
            memcpy(packet + 6, data, dataLen);
        }

        uint8_t crc = crc8Calculate(packet, totalContentLen + 4);
        packet[totalContentLen + 4] = crc;
        packet[totalContentLen + 5] = 0;

        gattServer.sendResponse(packet, totalContentLen + 6);
    }

    void sendErrorResponse(CommandType cmdType, uint16_t errorCode) {
        uint8_t header[4] = {
            7,
            static_cast<uint8_t>(cmdType),
            0,
            0
        };
        uint8_t packet[8];
        memcpy(packet, header, 4);
        packet[4] = static_cast<uint8_t>(errorCode & 0xFF);
        packet[5] = static_cast<uint8_t>((errorCode >> 8) & 0xFF);
        uint8_t crc = crc8Calculate(packet, 6);
        packet[6] = crc;
        packet[7] = 0;
        gattServer.sendResponse(packet, 8);
    }
};

// 全局实例指针 (用于静态回调)
M640GPumpSimulator* gSimulator = nullptr;

} // namespace M640GKit

#endif // M640G_PUMP_SIMULATOR_H
