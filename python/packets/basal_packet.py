"""
================================================================================
基础率数据包
================================================================================

用于设置基础率配置文件和临时基础率

作者: M640GKit Team
版本: 1.0.0
================================================================================
"""

from packets.base_packet import BasePacket
from enums import CommandType, BasalType


class SetBasalProfilePacket(BasePacket):
    """
    设置基础率配置文件包

    用于配置泵的基础率

    基础率配置文件包含48个时段的每小时基础率
    """

    command_type = CommandType.SET_BASAL_PROFILE

    def __init__(self, basal_profile: bytes):
        """
        初始化基础率配置包

        参数:
            basal_profile: 基础率配置文件数据
        """
        super().__init__()
        self.basal_profile = basal_profile

    def get_request_bytes(self) -> bytes:
        """
        获取请求数据

        返回:
            [1] + 基础率配置数据
        """
        return bytes([1]) + self.basal_profile

    def parse_response(self) -> dict:
        """解析响应"""
        return {
            'basal_type': BasalType(self.total_data[6]) if self.total_data[6] else BasalType.NONE,
            'basal_value': int.from_bytes(self.total_data[7:9], 'little') * 0.05,
            'basal_sequence': int.from_bytes(self.total_data[9:11], 'little'),
            'basal_patch_id': int.from_bytes(self.total_data[11:13], 'little'),
            'basal_start_time': int.from_bytes(self.total_data[13:17], 'little')
        }


class SetTempBasalPacket(BasePacket):
    """
    设置临时基础率包

    用于临时调整基础率

    参数:
        rate: 临时基础率 (U/hr)
        duration: 持续时间 (分钟)
    """

    command_type = CommandType.SET_TEMP_BASAL

    def __init__(self, rate: float, duration: float):
        """
        初始化临时基础率包

        参数:
            rate: 临时基础率 (U/hr)
            duration: 持续时间 (分钟)
        """
        super().__init__()
        self.rate = rate
        self.duration = duration

    def get_request_bytes(self) -> bytes:
        """
        获取请求数据

        返回:
            [6, 速率(2字节), 时长(2字节)]
        """
        output = bytearray([6])  # 固定为临时基础率类型
        temp_rate = int(self.rate / 0.05)
        output += temp_rate.to_bytes(2, 'little')
        temp_duration = int(self.duration)
        output += temp_duration.to_bytes(2, 'little')
        return bytes(output)

    def parse_response(self) -> dict:
        """解析响应"""
        return {
            'basal_type': BasalType(self.total_data[6]) if self.total_data[6] else BasalType.NONE,
            'basal_value': int.from_bytes(self.total_data[7:9], 'little') * 0.05,
            'basal_sequence': int.from_bytes(self.total_data[9:11], 'little'),
            'basal_patch_id': int.from_bytes(self.total_data[11:13], 'little'),
            'basal_start_time': int.from_bytes(self.total_data[13:17], 'little')
        }


class CancelTempBasalPacket(BasePacket):
    """
    取消临时基础率包

    用于取消正在执行的临时基础率
    """

    command_type = CommandType.CANCEL_TEMP_BASAL

    def get_request_bytes(self) -> bytes:
        """取消临时基础率无请求数据"""
        return b''

    def parse_response(self) -> dict:
        """解析响应"""
        return {
            'basal_type': BasalType(self.total_data[6]) if self.total_data[6] else BasalType.NONE,
            'basal_value': int.from_bytes(self.total_data[7:9], 'little') * 0.05,
            'basal_sequence': int.from_bytes(self.total_data[9:11], 'little'),
            'basal_patch_id': int.from_bytes(self.total_data[11:13], 'little'),
            'basal_start_time': int.from_bytes(self.total_data[13:17], 'little')
        }