/*
================================================================================
泵控制数据包 (C++ 版本)
================================================================================

对应 Python: packets/pump_control_packet.py
================================================================================
*/

#ifndef M640G_PUMP_CONTROL_PACKET_H
#define M640G_PUMP_CONTROL_PACKET_H

#include "base_packet.h"
#include <cstdint>

namespace M640GKit {

class SuspendPumpPacket : public BasePacket {
public:
    SuspendPumpPacket() {
        commandType = static_cast<uint8_t>(CommandType::SUSPEND_PUMP);
    }

    std::vector<uint8_t> getRequestBytes() const override {
        return {};
    }
};

class ResumePumpPacket : public BasePacket {
public:
    ResumePumpPacket() {
        commandType = static_cast<uint8_t>(CommandType::RESUME_PUMP);
    }

    std::vector<uint8_t> getRequestBytes() const override {
        return {};
    }
};

class StopPatchPacket : public BasePacket {
public:
    StopPatchPacket() {
        commandType = static_cast<uint8_t>(CommandType::STOP_PATCH);
    }

    std::vector<uint8_t> getRequestBytes() const override {
        return {};
    }
};

class ActivatePacket : public BasePacket {
public:
    ActivatePacket() {
        commandType = static_cast<uint8_t>(CommandType::ACTIVATE);
    }

    std::vector<uint8_t> getRequestBytes() const override {
        return {};
    }
};

class SetPatchPacket : public BasePacket {
public:
    SetPatchPacket() {
        commandType = static_cast<uint8_t>(CommandType::SET_PATCH);
    }

    std::vector<uint8_t> getRequestBytes() const override {
        return {};
    }
};

class PrimePacket : public BasePacket {
public:
    PrimePacket() {
        commandType = static_cast<uint8_t>(CommandType::PRIME);
    }

    std::vector<uint8_t> getRequestBytes() const override {
        return {};
    }
};

} // namespace M640GKit

#endif // M640G_PUMP_CONTROL_PACKET_H
