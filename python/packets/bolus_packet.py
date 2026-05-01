"""
================================================================================
大剂量数据包
================================================================================

用于设置和取消大剂量

大剂量类型:
1 = 普通大剂量
2 = 延时大剂量
3 = 组合大剂量

作者: M640GKit Team
版本: 1.0.0
================================================================================
"""

from packets.base_packet import BasePacket
from enums import CommandType


class SetBolusPacket(BasePacket):
    """
    设置大剂量包

    用于注射餐前大剂量

    大剂量类型:
    1 = 普通大剂量
    2 = 延时大剂量
    3 = 组合大剂量
    """

    command_type = CommandType.SET_BOLUS

    def __init__(self, bolus_amount: float, bolus_type: int = 1):
        """
        初始化大剂量包

        参数:
            bolus_amount: 大剂量单位 (U)
            bolus_type: 大剂量类型 (1=普通, 2=延时, 3=组合)
        """
        super().__init__()
        self.bolus_amount = bolus_amount
        self.bolus_type = bolus_type

    def get_request_bytes(self) -> bytes:
        """
        获取请求数据

        返回:
            [类型, 剂量(2字节), 0]
        """
        amount = int(self.bolus_amount / 0.05)
        output = bytearray([self.bolus_type])
        output += amount.to_bytes(2, 'little')
        output.append(0)
        return bytes(output)


class CancelBolusPacket(BasePacket):
    """
    取消大剂量包

    用于取消正在执行的大剂量

    取消类型:
    1 = 普通大剂量
    2 = 延时大剂量
    3 = 组合大剂量
    """

    command_type = CommandType.CANCEL_BOLUS

    def __init__(self, bolus_type: int = 1):
        """
        初始化取消大剂量包

        参数:
            bolus_type: 大剂量类型
        """
        super().__init__()
        self.bolus_type = bolus_type

    def get_request_bytes(self) -> bytes:
        """获取请求数据"""
        return bytes([self.bolus_type])


class ReadBolusStatePacket(BasePacket):
    """
    读取大剂量状态包

    查询当前大剂量的输送进度和状态
    """

    command_type = CommandType.READ_BOLUS_STATE

    def __init__(self, bolus_id: int = 0):
        """
        初始化读取大剂量状态包

        参数:
            bolus_id: 大剂量 ID (0=当前大剂量)
        """
        super().__init__()
        self.bolus_id = bolus_id

    def get_request_bytes(self) -> bytes:
        """获取请求数据"""
        return bytes([self.bolus_id])

    def parse_response(self) -> dict:
        """解析大剂量状态响应"""
        result = {
            'bolus_id': self.total_data[6],
            'state': 'idle' if self.total_data[7] == 0 else 'active',
            'delivered': int.from_bytes(self.total_data[8:10], 'little') * 0.05,
            'remaining': int.from_bytes(self.total_data[10:12], 'little') * 0.05
        }
        if len(self.total_data) >= 16:
            result['programmed'] = int.from_bytes(self.total_data[12:14], 'little') * 0.05
            result['duration'] = int.from_bytes(self.total_data[14:16], 'little')
        return result


class SetBolusMotorPacket(BasePacket):
    """
    设置大剂量电机包

    配置大剂量输送电机的参数
    """

    command_type = CommandType.SET_BOLUS_MOTOR

    def __init__(self, speed: int = 100, acceleration: int = 50):
        """
        初始化设置大剂量电机包

        参数:
            speed: 电机速度 (0-255)
            acceleration: 加速参数 (0-255)
        """
        super().__init__()
        self.speed = speed
        self.acceleration = acceleration

    def get_request_bytes(self) -> bytes:
        """获取请求数据"""
        return bytes([self.speed, self.acceleration])

    def parse_response(self) -> dict:
        """解析响应"""
        return {'ack': self.total_data[6] == 1}