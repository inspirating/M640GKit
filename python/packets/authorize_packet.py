"""
================================================================================
认证请求包
================================================================================

发送会话令牌和密钥进行身份验证

认证流程:
1. 生成随机会话令牌
2. 使用泵序列号生成密钥
3. 发送 [角色=2, 会话令牌, 密钥]
4. 解析响应获取设备类型和软件版本

作者: M640GKit Team
版本: 1.0.0
================================================================================
"""

from packets.base_packet import BasePacket
from enums import CommandType
from encryption import Crypto


class AuthorizePacket(BasePacket):
    """
    认证请求包

    认证流程:
    1. 生成随机会话令牌
    2. 使用泵序列号生成密钥
    3. 发送 [角色=2, 会话令牌, 密钥]
    4. 解析响应获取设备类型和软件版本
    """

    command_type = CommandType.AUTH_REQ
    minimum_data_size = 10

    def __init__(self, pump_sn: bytes, session_token: bytes):
        """
        初始化认证包

        参数:
            pump_sn: 泵序列号 (4字节)
            session_token: 会话令牌 (4字节)
        """
        super().__init__()
        self.pump_sn = bytes(reversed(pump_sn))
        self.session_token = session_token

    def get_request_bytes(self) -> bytes:
        """
        获取请求数据部分

        返回:
            [角色(1字节) + 会话令牌(4字节) + 密钥(4字节)]
        """
        key = Crypto.gen_key(self.pump_sn)
        output = bytearray([2])  # 角色 = 2 (控制器)
        output += self.session_token
        output += key
        return bytes(output)

    def parse_response(self) -> dict:
        """
        解析响应数据

        返回:
            包含设备类型和软件版本
        """
        return {
            'device_type': self.total_data[7],
            'sw_version': f"{self.total_data[8]}.{self.total_data[9]}.{self.total_data[10]}"
        }