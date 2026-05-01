"""
================================================================================
泵控制数据包
================================================================================

用于暂停、恢复泵和 Patch 管理

作者: M640GKit Team
版本: 1.0.0
================================================================================
"""

from packets.base_packet import BasePacket
from enums import CommandType


class SuspendPumpPacket(BasePacket):
    """
    暂停泵包

    用于暂停泵的所有输出

    参数:
        duration: 暂停持续时间 (分钟)
    """

    command_type = CommandType.SUSPEND_PUMP

    def __init__(self, duration: float):
        """
        初始化暂停包

        参数:
            duration: 暂停时间 (分钟)
        """
        super().__init__()
        self.duration = duration

    def get_request_bytes(self) -> bytes:
        """
        获取请求数据

        返回:
            [3, 持续时间]
        """
        return bytes([3, int(self.duration)])


class ResumePumpPacket(BasePacket):
    """
    恢复泵包

    用于恢复被暂停的泵
    """

    command_type = CommandType.RESUME_PUMP

    def get_request_bytes(self) -> bytes:
        """恢复泵无请求数据"""
        return b''


class StopPatchPacket(BasePacket):
    """
    停止 Patch 包

    用于停止当前 Patch
    """

    command_type = CommandType.STOP_PATCH

    def get_request_bytes(self) -> bytes:
        """停止 Patch 无请求数据"""
        return b''

    def parse_response(self) -> dict:
        """解析响应"""
        return {
            'sequence': int.from_bytes(self.total_data[6:8], 'little'),
            'patch_id': int.from_bytes(self.total_data[8:10], 'little')
        }


class ActivatePacket(BasePacket):
    """
    激活 Patch 包

    激活新的 Patch 并设置初始参数
    """

    command_type = CommandType.ACTIVATE

    def __init__(self, expiration_timer: int, alarm_setting: int,
                 hourly_max_insulin: float, daily_max_insulin: float,
                 current_tdd: float, basal_profile: bytes):
        """
        初始化激活包

        参数:
            expiration_timer: 过期计时器 (0=无, 1=72小时提醒)
            alarm_setting: 警报设置
            hourly_max_insulin: 每小时最大胰岛素量
            daily_max_insulin: 每日最大胰岛素量
            current_tdd: 当日总剂量
            basal_profile: 基础率配置文件
        """
        super().__init__()
        self.expiration_timer = expiration_timer
        self.alarm_setting = alarm_setting
        self.hourly_max_insulin = hourly_max_insulin
        self.daily_max_insulin = daily_max_insulin
        self.current_tdd = current_tdd
        self.basal_profile = basal_profile

    def get_request_bytes(self) -> bytes:
        """获取请求数据"""
        base = bytearray([
            0,
            12,
            self.expiration_timer,
            self.alarm_setting,
            0, 0, 30,
        ])
        hourly = int(self.hourly_max_insulin / 0.05)
        base += hourly.to_bytes(2, 'little')
        daily = int(self.daily_max_insulin / 0.05)
        base += daily.to_bytes(2, 'little')
        tdd = int(self.current_tdd / 0.05)
        base += tdd.to_bytes(2, 'little')
        base.append(1)
        base += self.basal_profile
        return bytes(base)

    def parse_response(self) -> dict:
        """解析激活响应"""
        return {
            'patch_id': self.total_data[6:10],
            'time': int.from_bytes(self.total_data[10:14], 'little')
        }


class SetPatchPacket(BasePacket):
    """
    设置 Patch 包

    用于配置 Patch 的运行参数
    """

    command_type = CommandType.SET_PATCH

    def __init__(self, alarm_settings: int, hourly_max_insulin: float,
                 daily_max_insulin: float, expiration_timer: int):
        """
        初始化设置 Patch 包

        参数:
            alarm_settings: 警报设置
            hourly_max_insulin: 每小时最大胰岛素量
            daily_max_insulin: 每日最大胰岛素量
            expiration_timer: 过期计时器
        """
        super().__init__()
        self.alarm_settings = alarm_settings
        self.hourly_max_insulin = hourly_max_insulin
        self.daily_max_insulin = daily_max_insulin
        self.expiration_timer = expiration_timer

    def get_request_bytes(self) -> bytes:
        """获取请求数据"""
        base = bytearray([self.alarm_settings])
        hourly = int(self.hourly_max_insulin / 0.05)
        base += hourly.to_bytes(2, 'little')
        daily = int(self.daily_max_insulin / 0.05)
        base += daily.to_bytes(2, 'little')
        base.append(self.expiration_timer)
        base += bytes([0, 12, 0, 0, 30])
        return bytes(base)


class PrimePacket(BasePacket):
    """
    灌注请求包

    在激活新 Patch 前进行灌注操作
    """

    command_type = CommandType.PRIME

    def get_request_bytes(self) -> bytes:
        """灌注包无请求数据"""
        return b''