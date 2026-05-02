"""
================================================================================
时间和警报数据包
================================================================================

用于时间同步和警报处理

作者: M640GKit Team
版本: 1.0.0
================================================================================
"""

from packets.base_packet import BasePacket
from enums import CommandType, AlertType


def m640gkit_seconds() -> int:
    """
    获取 M640GKit 格式的时间戳

    M640GKit 使用自定义的时间基准:
    - 基准时间: 2000年1月1日 00:00:00 UTC

    返回:
        从基准时间到现在的秒数
    """
    import utime
    BASE_YEAR = 2000
    SECONDS_PER_YEAR = 31536000
    now = utime.time()
    base = (BASE_YEAR - 1970) * SECONDS_PER_YEAR
    return now - base


def date_from_m640gkit_seconds(seconds: int):
    """
    从 M640GKit 时间戳转换为 datetime

    参数:
        seconds: M640GKit 格式的秒数

    返回:
        元组 (year, month, day, hour, minute, second)
    """
    import utime
    BASE_YEAR = 2000
    SECONDS_PER_YEAR = 31536000
    total = seconds + (BASE_YEAR - 1970) * SECONDS_PER_YEAR
    return utime.localtime(total)


class GetTimePacket(BasePacket):
    """
    获取泵时间包

    用于同步泵的时间
    """

    command_type = CommandType.GET_TIME

    def get_request_bytes(self) -> bytes:
        """获取时间包无请求数据"""
        return b''

    def parse_response(self) -> dict:
        """解析时间响应"""
        seconds_passed = int.from_bytes(self.total_data[6:10], 'little')
        return {'time': date_from_m640gkit_seconds(seconds_passed)}


class SetTimePacket(BasePacket):
    """
    设置泵时间包

    用于同步泵的系统时间
    """

    command_type = CommandType.SET_TIME

    def __init__(self, date: tuple):
        """
        初始化设置时间包

        参数:
            date: datetime 元组 (year, month, day, hour, minute, second)
        """
        super().__init__()
        self.date = date

    def get_request_bytes(self) -> bytes:
        """获取请求数据"""
        seconds = m640gkit_seconds()
        output = bytearray([2])
        output += seconds.to_bytes(4, 'little')
        return bytes(output)


class SetTimeZonePacket(BasePacket):
    """
    设置时区包

    配置泵的时区设置
    """

    command_type = CommandType.SET_TIME_ZONE

    def __init__(self, time_zone: int):
        """
        初始化设置时区包

        参数:
            time_zone: 时区值 (乘以4后的值)
        """
        super().__init__()
        self.time_zone = time_zone

    def get_request_bytes(self) -> bytes:
        offset = self.time_zone
        if offset > 12 * 60:
            offset -= 24 * 60
        if offset < 0:
            offset += 65536

        base = bytearray([
            offset & 0xFF,
            (offset >> 8) & 0xFF
        ])
        seconds = m640gkit_seconds()
        base += seconds.to_bytes(4, 'little')
        return bytes(base)


class ClearAlertPacket(BasePacket):
    """
    清除警报包

    用于清除泵的警报状态

    警报类型:
    4 = 每小时警报
    5 = 每日警报
    """

    command_type = CommandType.CLEAR_ALARM

    def __init__(self, alert_type: AlertType):
        """
        初始化清除警报包

        参数:
            alert_type: 警报类型
        """
        super().__init__()
        self.alert_type = alert_type

    def get_request_bytes(self) -> bytes:
        """获取请求数据"""
        value = self.alert_type
        return bytes([value & 0xFF, (value >> 8) & 0xFF])