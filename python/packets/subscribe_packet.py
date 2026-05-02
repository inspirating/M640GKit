"""
================================================================================
订阅请求包
================================================================================

订阅后可以接收泵的实时通知

作者: M640GKit Team
版本: 1.0.0
================================================================================
"""

from packets.base_packet import BasePacket
from enums import CommandType


class SubscribePacket(BasePacket):
    """
    订阅请求包

    订阅后可以接收泵的实时通知
    """

    command_type = CommandType.SUBSCRIBE

    def get_request_bytes(self) -> bytes:
        return bytes([0xFF, 0x0F])