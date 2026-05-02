/*
================================================================================
认证数据包 (C++ 版本)
================================================================================

对应 Python: packets/authorize_packet.py
================================================================================
*/

#ifndef M640G_AUTHORIZE_PACKET_H
#define M640G_AUTHORIZE_PACKET_H

#include "base_packet.h"

namespace M640GKit {

class AuthorizePacket : public BasePacket {
public:
    AuthorizePacket() {
        commandType = static_cast<uint8_t>(CommandType::AUTH_REQ);
        minimumDataSize = 10;
    }

    std::vector<uint8_t> getRequestBytes() const override {
        return {};
    }

    struct Response {
        uint8_t deviceType;
        uint8_t version;
    };

    Response parseResponse() const {
        Response resp;
        if (totalData.size() >= 8) {
            resp.deviceType = totalData[7];
            resp.version = totalData[8];
        }
        return resp;
    }
};

} // namespace M640GKit

#endif // M640G_AUTHORIZE_PACKET_H
