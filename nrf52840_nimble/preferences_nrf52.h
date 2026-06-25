/*
================================================================================
nRF52840 持久化层 (Preferences 兼容 stub)
================================================================================

ESP32 的 NVS Preferences 在 nRF52 上不可用。
此文件提供 API 兼容的空操作实现, 运行时所有数据仅存内存,
重启后丢失。

如需真实持久化, 可替换为:
  - Adafruit LittleFS (Adafruit nRF52 核心)
  - 逐字节 Flash 写入 (n-able 核心)
================================================================================
*/

#ifndef PREFERENCES_NRF52_H
#define PREFERENCES_NRF52_H

#include <Arduino.h>
#include <cstdint>

class Preferences {
public:
    Preferences() : _started(false) {}

    bool begin(const char* name, bool readOnly = false) {
        (void)name;
        (void)readOnly;
        _started = true;
        return true;
    }

    void end() {
        _started = false;
    }

    // --- 读操作 (返回默认值, 无持久化) ---
    uint32_t getUInt(const char* key, uint32_t defaultValue = 0) const {
        (void)key;
        return defaultValue;
    }

    int32_t getInt(const char* key, int32_t defaultValue = 0) const {
        (void)key;
        return defaultValue;
    }

    uint8_t getUChar(const char* key, uint8_t defaultValue = 0) const {
        (void)key;
        return defaultValue;
    }

    double getDouble(const char* key, double defaultValue = 0.0) const {
        (void)key;
        return defaultValue;
    }

    size_t getBytes(const char* key, void* buf, size_t maxLen) const {
        (void)key;
        (void)buf;
        (void)maxLen;
        return 0;
    }

    // --- 写操作 (空操作) ---
    size_t putUInt(const char* key, uint32_t value) {
        (void)key; (void)value;
        return 0;
    }

    size_t putInt(const char* key, int32_t value) {
        (void)key; (void)value;
        return 0;
    }

    size_t putUChar(const char* key, uint8_t value) {
        (void)key; (void)value;
        return 0;
    }

    size_t putDouble(const char* key, double value) {
        (void)key; (void)value;
        return 0;
    }

    size_t putBytes(const char* key, const void* value, size_t len) {
        (void)key; (void)value; (void)len;
        return 0;
    }

    bool remove(const char* key) {
        (void)key;
        return false;
    }

    void clear() {}

private:
    bool _started;
};

#endif // PREFERENCES_NRF52_H
