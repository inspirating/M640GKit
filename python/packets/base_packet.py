"""
================================================================================
数据包基类
================================================================================

定义所有数据包的基类, 提供编解码功能

数据包格式:
- Byte 0:     数据长度 (包括包头和CRC)
- Byte 1:     命令类型
- Byte 2:     序列号
- Byte 3:     包索引 (0=不分包/最后一包, 1-N=分包序号)
- Byte 4-5:   响应码
- Byte 6...:  数据内容
- 最后1字节:  CRC8校验

分包规则:
- 单包数据不超过15字节, 超过则分包传输

作者: M640GKit Team
版本: 1.0.0
================================================================================
"""

from encryption import crc8_calculate


class BasePacket:
    """
    数据包基类

    所有命令包都继承自此类, 提供统一的编解码接口

    子类需要设置:
    - command_type: 命令类型 (整数值)
    - get_request_bytes(): 返回请求数据部分
    - parse_response(): 解析响应数据
    """

    command_type = 0

    def __init__(self):
        """初始化数据包"""
        self.data_size = 0
        self.response_code = 0
        self.total_data = b''
        self.sequence_number = 0
        self.failed = False

    def encode(self, sequence_number: int) -> list:
        """
        编码数据包

        参数:
            sequence_number: 序列号, 用于追踪请求-响应对

        返回:
            数据包列表 (可能分包)
        """
        content = self.get_request_bytes()

        # 构建包头: [长度, 命令类型, 序列号, 包索引]
        header = bytearray([
            len(content) + 5,
            self.command_type,
            sequence_number,
            0  # 包索引, 0表示不分包或最后一包
        ])

        # 计算CRC前的数据
        tmp = bytes(header) + content
        total_command = tmp + bytes([crc8_calculate(tmp)])

        # 单包不超过15字节时不分包
        if len(total_command) - len(header) <= 15:
            output = total_command + bytes([0])  # 填充0
            return [output]

        # 分包处理
        packages = []
        pkg_index = 1
        remaining = total_command[4:]

        while len(remaining) > 15:
            header[3] = pkg_index
            tmp2 = bytes(header) + remaining[:15]
            packages.append(tmp2 + bytes([crc8_calculate(tmp2)]))
            remaining = remaining[15:]
            pkg_index += 1

        # 最后一包
        header[3] = pkg_index
        tmp3 = bytes(header) + remaining
        packages.append(tmp3 + bytes([crc8_calculate(tmp3)]))
        return packages

    def decode(self, data: bytes):
        """
        解码数据包

        参数:
            data: 接收到的原始数据
        """
        if not self.total_data:
            if data[1] != self.command_type:
                self.failed = True

            self.total_data = data[:-1]
            self.data_size = data[0]
            self.sequence_number = data[3]
            self.response_code = int.from_bytes(data[4:6], 'little')

            initial_crc = crc8_calculate(data[:-1])
            if initial_crc != data[-1]:
                self.failed = True
            return

        self.total_data += data[4:-1]
        self.sequence_number += 1

        new_crc = crc8_calculate(data[:-1])
        if new_crc != data[-1]:
            self.failed = True
        if self.sequence_number != data[3]:
            self.failed = True

    @property
    def is_complete(self) -> bool:
        """检查是否已接收完整的数据"""
        return len(self.total_data) == self.data_size

    @property
    def has_enough_data(self) -> bool:
        """检查是否有足够的数据 (子类的 mimimum_data_size)"""
        return len(self.total_data) >= getattr(self, 'minimum_data_size', 0)

    def get_request_bytes(self) -> bytes:
        """
        获取请求数据部分
        子类需要实现

        返回:
            请求数据字节
        """
        return b''