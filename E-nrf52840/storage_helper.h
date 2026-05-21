#ifndef M640G_STORAGE_HELPER_H
#define M640G_STORAGE_HELPER_H

#include <Arduino.h>
#include <EEPROM.h>

namespace M640GKit {

class StorageHelper {
public:
    static void begin() {
        EEPROM.begin(512);
    }

    static void putUChar(const char* key, uint8_t value) {
        int addr = getKeyAddress(key);
        if (addr < 0) return;
        EEPROM.write(addr, value);
        EEPROM.commit();
    }

    static uint8_t getUChar(const char* key, uint8_t defaultValue = 0) {
        int addr = getKeyAddress(key);
        if (addr < 0) return defaultValue;
        return EEPROM.read(addr);
    }

    static void putUInt(const char* key, uint32_t value) {
        int addr = getKeyAddress(key);
        if (addr < 0) return;
        EEPROM.write(addr, value & 0xFF);
        EEPROM.write(addr + 1, (value >> 8) & 0xFF);
        EEPROM.write(addr + 2, (value >> 16) & 0xFF);
        EEPROM.write(addr + 3, (value >> 24) & 0xFF);
        EEPROM.commit();
    }

    static uint32_t getUInt(const char* key, uint32_t defaultValue = 0) {
        int addr = getKeyAddress(key);
        if (addr < 0) return defaultValue;
        uint32_t value = 0;
        value |= EEPROM.read(addr);
        value |= (uint32_t)EEPROM.read(addr + 1) << 8;
        value |= (uint32_t)EEPROM.read(addr + 2) << 16;
        value |= (uint32_t)EEPROM.read(addr + 3) << 24;
        return value;
    }

    static void remove(const char* key) {
        int addr = getKeyAddress(key);
        if (addr < 0) return;
        for (int i = 0; i < 8; i++) {
            EEPROM.write(addr + i, 0);
        }
        EEPROM.commit();
    }

    static void clear() {
        for (int i = 0; i < 512; i++) {
            EEPROM.write(i, 0);
        }
        EEPROM.commit();
    }

private:
    static int getKeyAddress(const char* key) {
        if (strcmp(key, "patchStart") == 0) return 0;
        if (strcmp(key, "patchState") == 0) return 8;
        if (strcmp(key, "elapsedTime") == 0) return 16;
        return -1;
    }
};

} // namespace M640GKit

#endif // M640G_STORAGE_HELPER_H