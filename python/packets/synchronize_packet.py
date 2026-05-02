"""
================================================================================
同步请求包
================================================================================

轮询泵状态, 包括大剂量、基础率、电池等

作者: M640GKit Team
版本: 1.0.0
================================================================================
"""

from packets.base_packet import BasePacket
from enums import CommandType, PatchState


class SynchronizePacket(BasePacket):
    """
    同步请求包

    轮询泵状态, 包括大剂量、基础率、电池等

    同步响应包含多个字段, 通过掩码标识哪些字段存在
    """

    command_type = CommandType.SYNCHRONIZE
    minimum_data_size = 3

    def get_request_bytes(self) -> bytes:
        """同步包无请求数据"""
        return b''

    def parse_response(self) -> dict:
        """
        解析同步响应

        返回:
            包含状态、字段掩码和同步数据
        """
        state = PatchState(self.total_data[6]) if self.total_data[6] else PatchState.NONE
        field_mask = int.from_bytes(self.total_data[7:9], 'little')
        sync_data = self.total_data[9:]
        return {
            'state': state,
            'field_mask': field_mask,
            'sync_data': sync_data
        }


class SynchronizeResponseParser:
    """
    同步响应解析器

    根据字段掩码解析同步响应数据
    """

    # 同步响应掩码
    MASK_SUSPEND = 0x0001
    MASK_NORMAL_BOLUS = 0x0002
    MASK_EXTENDED_BOLUS = 0x0004
    MASK_BASAL = 0x0008
    MASK_SETUP = 0x0010
    MASK_RESERVOIR = 0x0020
    MASK_START_TIME = 0x0040
    MASK_BATTERY = 0x0080
    MASK_STORAGE = 0x0100
    MASK_ALARM = 0x0200
    MASK_AGE = 0x0400
    MASK_MAGNETO_PLACE = 0x0800

    @staticmethod
    def parse(field_mask: int, sync_data: bytes) -> dict:
        """
        解析同步响应数据

        参数:
            field_mask: 字段掩码
            sync_data: 同步数据

        返回:
            解析后的字典
        """
        from enums import BasalType

        result = {
            'state': PatchState.NONE,
            'suspend_time': None,
            'bolus': None,
            'basal': None,
            'prime_progress': None,
            'reservoir': None,
            'start_time': None,
            'battery': None,
            'storage': None,
            'active_alarms': [],
            'patch_age': None,
            'magneto_placement': None
        }

        offset = 0

        # 处理各个字段
        if field_mask & SynchronizeResponseParser.MASK_SUSPEND:
            seconds = int.from_bytes(sync_data[offset:offset+4], 'little')
            result['suspend_time'] = seconds
            offset += 4

        if field_mask & SynchronizeResponseParser.MASK_NORMAL_BOLUS:
            bolus_type = sync_data[offset] & 0x7F
            completed = (sync_data[offset] & 0x80) != 0
            delivered = int.from_bytes(sync_data[offset+1:offset+3], 'little') * 0.05
            result['bolus'] = {
                'type': bolus_type,
                'completed': completed,
                'delivered': delivered
            }
            offset += 3

        if field_mask & SynchronizeResponseParser.MASK_EXTENDED_BOLUS:
            offset += 3

        if field_mask & SynchronizeResponseParser.MASK_BASAL:
            rate_delivery = int.from_bytes(sync_data[offset+9:offset+12], 'little')
            delivery = rate_delivery >> 12
            rate = rate_delivery & 0x0FFF
            result['basal'] = {
                'type': BasalType(sync_data[offset]) if sync_data[offset] else BasalType.NONE,
                'sequence': int.from_bytes(sync_data[offset+1:offset+3], 'little'),
                'patch_id': int.from_bytes(sync_data[offset+3:offset+5], 'little'),
                'start_time': int.from_bytes(sync_data[offset+5:offset+9], 'little'),
                'rate': rate * 0.05,
                'delivery': delivery * 0.05
            }
            offset += 12

        if field_mask & SynchronizeResponseParser.MASK_SETUP:
            result['prime_progress'] = sync_data[offset]
            offset += 1

        if field_mask & SynchronizeResponseParser.MASK_RESERVOIR:
            result['reservoir'] = int.from_bytes(sync_data[offset:offset+2], 'little') * 0.05
            offset += 2

        if field_mask & SynchronizeResponseParser.MASK_START_TIME:
            seconds = int.from_bytes(sync_data[offset:offset+4], 'little')
            result['start_time'] = seconds
            offset += 4

        if field_mask & SynchronizeResponseParser.MASK_BATTERY:
            value = int.from_bytes(sync_data[offset:offset+3], 'little')
            result['battery'] = {
                'voltage_a': (value & 0xFFF) / 512,
                'voltage_b': (value >> 12) / 512
            }
            offset += 3

        if field_mask & SynchronizeResponseParser.MASK_STORAGE:
            result['storage'] = {
                'sequence': int.from_bytes(sync_data[offset:offset+2], 'little'),
                'patch_id': int.from_bytes(sync_data[offset+2:offset+4], 'little')
            }
            offset += 4

        if field_mask & SynchronizeResponseParser.MASK_ALARM:
            flags = int.from_bytes(sync_data[offset:offset+2], 'little')
            if flags != 0:
                for i in range(3):
                    if flags & (1 << i):
                        result['active_alarms'].append(1 << i)
            offset += 4

        if field_mask & SynchronizeResponseParser.MASK_AGE:
            result['patch_age'] = int.from_bytes(sync_data[offset:offset+4], 'little')
            offset += 4

        if field_mask & SynchronizeResponseParser.MASK_MAGNETO_PLACE:
            result['magneto_placement'] = int.from_bytes(sync_data[offset:offset+2], 'little')
            offset += 2

        return result