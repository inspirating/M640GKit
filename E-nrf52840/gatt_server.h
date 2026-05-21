#ifndef M640G_GATT_SERVER_H
#define M640G_GATT_SERVER_H

#include <Arduino.h>
#include <ArduinoBLE.h>
#include <vector>
#include "enums.h"
#include "crc8.h"

namespace M640GKit {

typedef void (*WriteRequestCallback)(const uint8_t* data, size_t len);
typedef void (*SubscribeCallback)(bool subscribed);
typedef void (*DisconnectCallback)();
typedef void (*ConnectCallback)();

class GATTServer {
public:
    bool isRunning = false;
    bool advertisingSuspended = false;
    WriteRequestCallback onWriteRequest = nullptr;
    SubscribeCallback onSubscribe = nullptr;
    DisconnectCallback onDisconnectCallback = nullptr;
    ConnectCallback onConnectCallback = nullptr;

    GATTServer() : service(nullptr), writeChar(nullptr), notifyChar(nullptr) {}

    void start() {
        if (isRunning) return;

        Serial.println("[GATT] Initializing BLE...");
        BLE.setLocalName("MT");
        BLE.setDeviceName("MT");
        BLE.setConnectionInterval(12, 24);

        if (!BLE.begin()) {
            Serial.println("[GATT] ERROR: Failed to initialize BLE!");
            return;
        }

        service = new BLEService(SERVICE_UUID);
        writeChar = new BLECharacteristic(WRITE_UUID, BLEWrite | BLEWriteWithoutResponse, 128, false);
        notifyChar = new BLECharacteristic(READ_UUID, BLERead | BLENotify, 128, false);

        service->addCharacteristic(*writeChar);
        service->addCharacteristic(*notifyChar);

        BLE.addService(*service);

        writeChar->setEventHandler(BLEWritten, handleWrite);
        notifyChar->setEventHandler(BLESubscribed, handleSubscribed);
        notifyChar->setEventHandler(BLEUnsubscribed, handleUnsubscribed);

        BLE.setEventHandler(BLEConnected, handleConnect);
        BLE.setEventHandler(BLEDisconnected, handleDisconnect);

        const uint8_t mfgData[] = {0x59, 0x6A, 0x65, 0xD1, 0x79, 0x98, 0x01, 0x01};

        BLEAdvertisingData advData;
        advData.setLocalName("MT");
        advData.setAdvertisedServiceUuid(SERVICE_UUID);
        BLE.setAdvertisingData(advData);

        BLEAdvertisingData scanData;
        scanData.setLocalName("MT");
        BLE.setScanResponseData(scanData);

        BLE.advertise();

        isRunning = true;
        Serial.println("[GATT] GATT Server is now running!");
    }

    void stop() {
        if (!isRunning) return;
        BLE.stopAdvertise();
        BLE.end();
        isRunning = false;
    }

    void disconnectAll() {
        BLEDevice central = BLE.central();
        if (central && central.connected()) {
            central.disconnect();
        }
    }

    void startAdvertising() {
        if (advertisingSuspended) {
            Serial.println("[ADV] Advertising suspended, skipping...");
            return;
        }
        BLE.advertise();
        Serial.println("[ADV] BLE advertising started");
    }

    bool sendNotification(const uint8_t* data, size_t len, bool useCrcHack = true) {
        if (notifyChar == nullptr) return false;

        std::vector<uint8_t> payload(data, data + len);

        if (useCrcHack && len >= 6 && data[1] != 0x00) {
            uint8_t expectedCrc = crc8Calculate(data, len - 1);
            if (payload[len - 1] != expectedCrc) {
                payload[len - 1] = expectedCrc;
            }
        }

        notifyChar->writeValue(payload.data(), payload.size());
        return true;
    }

    bool sendRawNotification(const uint8_t* data, size_t len) {
        if (notifyChar == nullptr) return false;
        notifyChar->writeValue(data, len);
        return true;
    }

    bool sendResponse(const uint8_t* data, size_t len) {
        if (writeChar == nullptr) return false;
        writeChar->writeValue(data, len);
        return true;
    }

    void poll() {
        BLE.poll();
    }

    bool isCentralConnected() {
        BLEDevice central = BLE.central();
        return central && central.connected();
    }

    void setManufacturerDataInAdvertisement() {
        const uint8_t mfgData[] = {0x59, 0x6A, 0x65, 0xD1, 0x79, 0x98, 0x01, 0x01};
        BLEAdvertisingData advData;
        advData.setLocalName("MT");
        advData.setAdvertisedServiceUuid(SERVICE_UUID);
        BLE.setAdvertisingData(advData);
    }

private:
    BLEService* service;
    BLECharacteristic* writeChar;
    BLECharacteristic* notifyChar;

    static void handleWrite(BLECentral& central, BLECharacteristic& characteristic) {
        size_t len = characteristic.valueLength();
        if (len > 0) {
            const uint8_t* data = (const uint8_t*)characteristic.value();
            Logger::hexDump("RX", "<--", data, len);
            if (gInstance && gInstance->onWriteRequest) {
                gInstance->onWriteRequest(data, len);
            }
        }
    }

    static void handleSubscribed(BLECentral& central, BLECharacteristic& characteristic) {
        if (gInstance && gInstance->onSubscribe) {
            gInstance->onSubscribe(true);
        }
    }

    static void handleUnsubscribed(BLECentral& central, BLECharacteristic& characteristic) {
        if (gInstance && gInstance->onSubscribe) {
            gInstance->onSubscribe(false);
        }
    }

    static void handleConnect(BLEDevice central) {
        Serial.println("[GATT] Central connected");
        if (gInstance && gInstance->onConnectCallback) {
            gInstance->onConnectCallback();
        }
    }

    static void handleDisconnect(BLEDevice central) {
        Serial.println("[GATT] Central disconnected");
        if (gInstance && gInstance->onDisconnectCallback) {
            gInstance->onDisconnectCallback();
        }
    }

public:
    static GATTServer* gInstance;
};

GATTServer* GATTServer::gInstance = nullptr;

} // namespace M640GKit

#endif // M640G_GATT_SERVER_H