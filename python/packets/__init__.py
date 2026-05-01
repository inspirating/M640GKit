"""
================================================================================
数据包模块
================================================================================

包含所有与泵通信的数据包类

数据包类:
- BasePacket: 基类
- AuthorizePacket: 认证请求
- SynchronizePacket: 同步请求
- SubscribePacket: 订阅请求
- SetBolusPacket: 设置大剂量
- CancelBolusPacket: 取消大剂量
- SetBasalProfilePacket: 设置基础率
- SetTempBasalPacket: 设置临时基础率
- CancelTempBasalPacket: 取消临时基础率
- SuspendPumpPacket: 暂停泵
- ResumePumpPacket: 恢复泵
- 等等...

作者: M640GKit Team
版本: 1.0.0
================================================================================
"""

from packets.base_packet import BasePacket
from packets.authorize_packet import AuthorizePacket
from packets.synchronize_packet import SynchronizePacket, SynchronizeResponseParser
from packets.subscribe_packet import SubscribePacket
from packets.bolus_packet import (
    SetBolusPacket,
    CancelBolusPacket,
    ReadBolusStatePacket,
    SetBolusMotorPacket
)
from packets.basal_packet import (
    SetBasalProfilePacket,
    SetTempBasalPacket,
    CancelTempBasalPacket
)
from packets.pump_control_packet import (
    SuspendPumpPacket,
    ResumePumpPacket,
    StopPatchPacket,
    ActivatePacket,
    SetPatchPacket,
    PrimePacket
)
from packets.time_packet import (
    GetTimePacket,
    SetTimePacket,
    SetTimeZonePacket,
    ClearAlertPacket,
    m640gkit_seconds,
    date_from_m640gkit_seconds
)
from packets.misc_packet import (
    PollPatchPacket,
    GetDeviceTypePacket,
    GetRecordPacket
)

__all__ = [
    'BasePacket',
    'AuthorizePacket',
    'SynchronizePacket',
    'SynchronizeResponseParser',
    'SubscribePacket',
    'SetBolusPacket',
    'CancelBolusPacket',
    'ReadBolusStatePacket',
    'SetBolusMotorPacket',
    'SetBasalProfilePacket',
    'SetTempBasalPacket',
    'CancelTempBasalPacket',
    'SuspendPumpPacket',
    'ResumePumpPacket',
    'StopPatchPacket',
    'ActivatePacket',
    'SetPatchPacket',
    'PrimePacket',
    'GetTimePacket',
    'SetTimePacket',
    'SetTimeZonePacket',
    'ClearAlertPacket',
    'PollPatchPacket',
    'GetDeviceTypePacket',
    'GetRecordPacket',
    'm640gkit_seconds',
    'date_from_m640gkit_seconds'
]