"""
================================================================================
其他数据包
================================================================================

包含轮询、设备类型、记录获取等数据包

作者: M640GKit Team
版本: 1.0.0
================================================================================
"""

from packets.base_packet import BasePacket
from enums import CommandType, PatchState


class PollPatchPacket(BasePacket):
    """
    轮询 Patch 包

    定期轮询以检查 Patch 是否有状态变化
    """

    command_type = CommandType.POLL_PATCH

    def __init__(self, last_sequence: int = 0):
        """
        初始化轮询包

        参数:
            last_sequence: 上次收到的序列号, 用于检测遗漏
        """
        super().__init__()
        self.last_sequence = last_sequence

    def get_request_bytes(self) -> bytes:
        """获取请求数据"""
        return self.last_sequence.to_bytes(2, 'little')

    def parse_response(self) -> dict:
        """解析轮询响应"""
        return {
            'sequence': int.from_bytes(self.total_data[6:8], 'little'),
            'patch_state': PatchState(self.total_data[8]) if self.total_data[8] else PatchState.NONE,
            'active_alarms': list(self.total_data[9:9 + self.total_data[9]]) if len(self.total_data) > 9 else []
        }


class GetDeviceTypePacket(BasePacket):
    """
    获取设备类型包

    查询设备的类型和版本信息
    """

    command_type = CommandType.GET_DEVICE_TYPE

    def get_request_bytes(self) -> bytes:
        """获取设备类型包无请求数据"""
        return b''

    def parse_response(self) -> dict:
        """解析设备类型响应"""
        return {
            'device_type': self.total_data[6],
            'hw_version': f"{self.total_data[7]}.{self.total_data[8]}",
            'sw_version': f"{self.total_data[9]}.{self.total_data[10]}.{self.total_data[11]}",
            'serial': self.total_data[12:16].hex()
        }


class GetRecordPacket(BasePacket):
    """
    获取记录包

    从泵的历史记录中获取指定类型的记录

    记录类型:
    - 1: 大剂量记录
    - 2: 基础率记录
    - 3: 临时基础率记录
    - 4: 警报记录
    """

    command_type = CommandType.GET_RECORD

    def __init__(self, record_type: int, start_index: int = 0, count: int = 10):
        """
        初始化获取记录包

        参数:
            record_type: 记录类型
            start_index: 起始索引
            count: 获取数量
        """
        super().__init__()
        self.record_type = record_type
        self.start_index = start_index
        self.count = count

    def get_request_bytes(self) -> bytes:
        """获取请求数据"""
        data = bytearray()
        data.append(self.record_type)
        data.extend(self.start_index.to_bytes(2, 'little'))
        data.append(self.count)
        return bytes(data)

    def parse_response(self) -> dict:
        """解析记录响应"""
        record_count = self.total_data[6]
        records = []
        offset = 7
        for _ in range(record_count):
            if offset + 4 > len(self.total_data):
                break
            record = {
                'timestamp': int.from_bytes(self.total_data[offset:offset+4], 'little'),
                'type': self.record_type
            }
            records.append(record)
            offset += 4
        return {'record_count': record_count, 'records': records}