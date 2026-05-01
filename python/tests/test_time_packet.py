"""
================================================================================
TimePacket 单元测试
================================================================================

测试时间相关数据包的编码和响应解析

包含:
- GetTimePacket
- SetTimePacket
- SetTimeZonePacket
- ClearAlertPacket

运行测试:
    import tests.test_time_packet
    tests.test_time_packet.run_tests()

作者: M640GKit Team
版本: 1.0.0
================================================================================
"""

import sys
sys.path.insert(0, '.')

from packets.time_packet import GetTimePacket, SetTimePacket, SetTimeZonePacket, ClearAlertPacket
from enums import CommandType, AlertType
from encryption import crc8_calculate


def test_get_time_command_type():
    """测试GetTimePacket命令类型"""
    print("测试 test_get_time_command_type...")

    packet = GetTimePacket()

    assert packet.command_type == CommandType.GET_TIME, \
        f"命令类型错误: {packet.command_type}"

    print("  ✓ 通过")


def test_get_time_encode():
    """测试GetTimePacket编码"""
    print("测试 test_get_time_encode...")

    packet = GetTimePacket()
    packages = packet.encode(1)

    assert len(packages) == 1, f"期望1个包, 实际{len(packages)}"

    pkg = packages[0]

    content = pkg[6:-2]
    assert len(content) == 0, f"获取时间请求应无数据: {content.hex()}"

    print(f"  ✓ 通过 (编码: {pkg.hex()})")


def test_get_time_get_request_bytes():
    """测试GetTimePacket无请求数据"""
    print("测试 test_get_time_get_request_bytes...")

    packet = GetTimePacket()

    result = packet.get_request_bytes()

    assert result == b'', f"获取时间应返回空字节: {result.hex()}"

    print("  ✓ 通过")


def test_set_time_command_type():
    """测试SetTimePacket命令类型"""
    print("测试 test_set_time_command_type...")

    packet = SetTimePacket((2024, 1, 15, 12, 30, 0))

    assert packet.command_type == CommandType.SET_TIME, \
        f"命令类型错误: {packet.command_type}"

    print("  ✓ 通过")


def test_set_time_encode():
    """测试SetTimePacket编码"""
    print("测试 test_set_time_encode...")

    packet = SetTimePacket((2024, 1, 15, 12, 30, 0))
    packages = packet.encode(1)

    assert len(packages) == 1, f"期望1个包, 实际{len(packages)}"

    pkg = packages[0]

    content = pkg[6:-2]
    assert content[0] == 2, f"首字节应为2: {content[0]}"
    assert len(content) == 5, f"数据长度应为5: {len(content)}"

    seconds = int.from_bytes(content[1:5], 'little')
    assert seconds > 0, f"秒数应大于0: {seconds}"

    print(f"  ✓ 通过 (编码: {pkg.hex()})")


def test_set_time_date_storage():
    """测试SetTimePacket日期存储"""
    print("测试 test_set_time_date_storage...")

    test_date = (2024, 6, 15, 10, 30, 45)
    packet = SetTimePacket(test_date)

    assert packet.date == test_date, f"日期存储错误: {packet.date}"

    print("  ✓ 通过")


def test_set_time_zone_command_type():
    """测试SetTimeZonePacket命令类型"""
    print("测试 test_set_time_zone_command_type...")

    packet = SetTimeZonePacket(14400)

    assert packet.command_type == CommandType.SET_TIME_ZONE, \
        f"命令类型错误: {packet.command_type}"

    print("  ✓ 通过")


def test_set_time_zone_encode():
    """测试SetTimeZonePacket编码"""
    print("测试 test_set_time_zone_encode...")

    packet = SetTimeZonePacket(14400)
    packages = packet.encode(1)

    assert len(packages) == 1, f"期望1个包, 实际{len(packages)}"

    pkg = packages[0]

    content = pkg[6:-2]
    tz_value = int.from_bytes(content, 'little')
    assert tz_value == 14400, f"时区值应为14400: {tz_value}"

    print(f"  ✓ 通过 (时区值: {tz_value})")


def test_set_time_zone_negative():
    """测试SetTimeZonePacket负时区值"""
    print("测试 test_set_time_zone_negative...")

    packet = SetTimeZonePacket(-12)
    packages = packet.encode(1)

    pkg = packages[0]
    content = pkg[6:-2]

    tz_value = int.from_bytes(content, 'little', signed=True)
    assert tz_value == -12, f"负时区值处理错误: {tz_value}"

    print(f"  ✓ 通过 (负时区值: {tz_value})")


def test_clear_alert_command_type():
    """测试ClearAlertPacket命令类型"""
    print("测试 test_clear_alert_command_type...")

    packet = ClearAlertPacket(AlertType.HOURLY_ALERT)

    assert packet.command_type == CommandType.CLEAR_ALARM, \
        f"命令类型错误: {packet.command_type}"

    print("  ✓ 通过")


def test_clear_alert_encode():
    """测试ClearAlertPacket编码"""
    print("测试 test_clear_alert_encode...")

    packet = ClearAlertPacket(AlertType.HOURLY_ALERT)
    packages = packet.encode(1)

    assert len(packages) == 1, f"期望1个包, 实际{len(packages)}"

    pkg = packages[0]

    content = pkg[6:-2]
    assert len(content) == 2, f"清除警报数据长度应为2: {len(content)}"

    alert_value = int.from_bytes(content, 'little')
    assert alert_value == AlertType.HOURLY_ALERT, \
        f"警报类型值错误: {alert_value}"

    print(f"  ✓ 通过 (警报类型: {alert_value})")


def test_clear_alert_daily():
    """测试ClearAlertPacket每日警报"""
    print("测试 test_clear_alert_daily...")

    packet = ClearAlertPacket(AlertType.DAILY_ALERT)
    packages = packet.encode(1)

    pkg = packages[0]
    content = pkg[6:-2]

    alert_value = int.from_bytes(content, 'little')
    assert alert_value == AlertType.DAILY_ALERT, \
        f"每日警报类型值错误: {alert_value}"

    print(f"  ✓ 通过 (每日警报类型: {alert_value})")


def test_time_encode_decode_roundtrip():
    """测试时间包编码解码往返"""
    print("测试 test_time_encode_decode_roundtrip...")

    packet1 = SetTimeZonePacket(18000)
    packages = packet1.encode(42)

    packet2 = SetTimeZonePacket(0)
    packet2.decode(packages[0])

    assert not packet2.failed, "解码过程不应该失败"

    content = packet2.get_request_bytes()
    tz_value = int.from_bytes(content, 'little')
    assert tz_value == 18000, f"往返转换后时区值应仍为18000: {tz_value}"

    print("  ✓ 通过")


def run_tests():
    """运行所有测试"""
    print("=" * 60)
    print("TimePacket 单元测试")
    print("=" * 60)

    test_get_time_command_type()
    test_get_time_encode()
    test_get_time_get_request_bytes()
    test_set_time_command_type()
    test_set_time_encode()
    test_set_time_date_storage()
    test_set_time_zone_command_type()
    test_set_time_zone_encode()
    test_set_time_zone_negative()
    test_clear_alert_command_type()
    test_clear_alert_encode()
    test_clear_alert_daily()
    test_time_encode_decode_roundtrip()

    print("=" * 60)
    print("所有测试通过!")
    print("=" * 60)


if __name__ == '__main__':
    run_tests()