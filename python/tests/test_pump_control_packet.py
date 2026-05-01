"""
================================================================================
PumpControlPacket 单元测试
================================================================================

测试泵控制相关数据包的编码和响应解析

包含:
- SuspendPumpPacket
- ResumePumpPacket
- StopPatchPacket
- ActivatePacket
- SetPatchPacket

运行测试:
    import tests.test_pump_control_packet
    tests.test_pump_control_packet.run_tests()

作者: M640GKit Team
版本: 1.0.0
================================================================================
"""

import sys
sys.path.insert(0, '.')

from packets.pump_control_packet import (
    SuspendPumpPacket, ResumePumpPacket, StopPatchPacket,
    ActivatePacket, SetPatchPacket
)
from enums import CommandType
from encryption import crc8_calculate


def test_suspend_pump_command_type():
    """测试SuspendPumpPacket命令类型"""
    print("测试 test_suspend_pump_command_type...")

    packet = SuspendPumpPacket(30)

    assert packet.command_type == CommandType.SUSPEND_PUMP, \
        f"命令类型错误: {packet.command_type}"

    print("  ✓ 通过")


def test_suspend_pump_encode():
    """测试SuspendPumpPacket编码"""
    print("测试 test_suspend_pump_encode...")

    packet = SuspendPumpPacket(60)
    packages = packet.encode(1)

    assert len(packages) == 1, f"期望1个包, 实际{len(packages)}"

    pkg = packages[0]

    content = pkg[6:-2]
    assert content[0] == 3, f"首字节应为3: {content[0]}"
    assert content[1] == 60, f"暂停时长应为60: {content[1]}"

    print(f"  ✓ 通过 (暂停时长: {content[1]} 分钟)")


def test_suspend_pump_duration():
    """测试SuspendPumpPacket不同暂停时长"""
    print("测试 test_suspend_pump_duration...")

    test_cases = [15, 30, 60, 120, 240]

    for duration in test_cases:
        packet = SuspendPumpPacket(duration)
        content = packet.get_request_bytes()
        assert content[1] == duration, \
            f"暂停时长{duration}不匹配: {content[1]}"

    print("  ✓ 通过")


def test_resume_pump_command_type():
    """测试ResumePumpPacket命令类型"""
    print("测试 test_resume_pump_command_type...")

    packet = ResumePumpPacket()

    assert packet.command_type == CommandType.RESUME_PUMP, \
        f"命令类型错误: {packet.command_type}"

    print("  ✓ 通过")


def test_resume_pump_encode():
    """测试ResumePumpPacket编码"""
    print("测试 test_resume_pump_encode...")

    packet = ResumePumpPacket()
    packages = packet.encode(1)

    assert len(packages) == 1, f"期望1个包, 实际{len(packages)}"

    pkg = packages[0]

    content = pkg[6:-2]
    assert len(content) == 0, f"恢复泵应无数据: {content.hex()}"

    print(f"  ✓ 通过 (编码: {pkg.hex()})")


def test_resume_pump_get_request_bytes():
    """测试ResumePumpPacket无请求数据"""
    print("测试 test_resume_pump_get_request_bytes...")

    packet = ResumePumpPacket()

    result = packet.get_request_bytes()

    assert result == b'', f"恢复泵应返回空字节: {result.hex()}"

    print("  ✓ 通过")


def test_stop_patch_command_type():
    """测试StopPatchPacket命令类型"""
    print("测试 test_stop_patch_command_type...")

    packet = StopPatchPacket()

    assert packet.command_type == CommandType.STOP_PATCH, \
        f"命令类型错误: {packet.command_type}"

    print("  ✓ 通过")


def test_stop_patch_encode():
    """测试StopPatchPacket编码"""
    print("测试 test_stop_patch_encode...")

    packet = StopPatchPacket()
    packages = packet.encode(1)

    assert len(packages) == 1, f"期望1个包, 实际{len(packages)}"

    pkg = packages[0]

    content = pkg[6:-2]
    assert len(content) == 0, f"停止Patch应无数据: {content.hex()}"

    print(f"  ✓ 通过 (编码: {pkg.hex()})")


def test_stop_patch_parse_response():
    """测试StopPatchPacket响应解析"""
    print("测试 test_stop_patch_parse_response...")

    packet = StopPatchPacket()
    response_data = b'\x0a\x03\x01\x00\x00\x00\x01\x00\x02\x00\x8a\x00'
    packet.decode(response_data)

    parsed = packet.parse_response()

    assert parsed['sequence'] == 1, f"序列号错误: {parsed['sequence']}"
    assert parsed['patch_id'] == 2, f"Patch ID错误: {parsed['patch_id']}"

    print(f"  ✓ 通过 (序列号: {parsed['sequence']}, Patch ID: {parsed['patch_id']})")


def test_activate_packet_command_type():
    """测试ActivatePacket命令类型"""
    print("测试 test_activate_packet_command_type...")

    packet = ActivatePacket(
        expiration_timer=1,
        alarm_setting=30,
        hourly_max_insulin=50.0,
        daily_max_insulin=300.0,
        current_tdd=50.0,
        basal_profile=bytes([0] * 48)
    )

    assert packet.command_type == CommandType.ACTIVATE, \
        f"命令类型错误: {packet.command_type}"

    print("  ✓ 通过")


def test_activate_packet_encode():
    """测试ActivatePacket编码"""
    print("测试 test_activate_packet_encode...")

    basal = bytes([30] * 48)
    packet = ActivatePacket(
        expiration_timer=1,
        alarm_setting=30,
        hourly_max_insulin=50.0,
        daily_max_insulin=300.0,
        current_tdd=50.0,
        basal_profile=basal
    )
    packages = packet.encode(1)

    assert len(packages) > 0, f"应生成至少1个包"

    print(f"  ✓ 通过 (生成了 {len(packages)} 个分包)")


def test_activate_packet_parse_response():
    """测试ActivatePacket响应解析"""
    print("测试 test_activate_packet_parse_response...")

    basal = bytes([0] * 48)
    packet = ActivatePacket(
        expiration_timer=1,
        alarm_setting=30,
        hourly_max_insulin=50.0,
        daily_max_insulin=300.0,
        current_tdd=50.0,
        basal_profile=basal
    )
    response_data = b'\x0e\x03\x01\x00\x00\x00\x12\x34\x56\x78\x00\x00\x00\x00\x02\x00\x00\x00\x8a\x00'
    packet.decode(response_data)

    parsed = packet.parse_response()

    assert parsed['patch_id'] == b'\x12\x34\x56\x78', \
        f"Patch ID错误: {parsed['patch_id'].hex()}"
    assert parsed['time'] == 2, f"时间错误: {parsed['time']}"

    print(f"  ✓ 通过 (Patch ID: {parsed['patch_id'].hex()})")


def test_set_patch_command_type():
    """测试SetPatchPacket命令类型"""
    print("测试 test_set_patch_command_type...")

    packet = SetPatchPacket()

    assert packet.command_type == CommandType.SET_PATCH, \
        f"命令类型错误: {packet.command_type}"

    print("  ✓ 通过")


def test_pump_control_encode_decode_roundtrip():
    """测试泵控制包编码解码往返"""
    print("测试 test_pump_control_encode_decode_roundtrip...")

    packet1 = SuspendPumpPacket(90)
    packages = packet1.encode(42)

    packet2 = SuspendPumpPacket(0)
    packet2.decode(packages[0])

    assert not packet2.failed, "解码过程不应该失败"

    content = packet2.get_request_bytes()
    assert content[1] == 90, f"往返转换后暂停时长应仍为90: {content[1]}"

    print("  ✓ 通过")


def run_tests():
    """运行所有测试"""
    print("=" * 60)
    print("PumpControlPacket 单元测试")
    print("=" * 60)

    test_suspend_pump_command_type()
    test_suspend_pump_encode()
    test_suspend_pump_duration()
    test_resume_pump_command_type()
    test_resume_pump_encode()
    test_resume_pump_get_request_bytes()
    test_stop_patch_command_type()
    test_stop_patch_encode()
    test_stop_patch_parse_response()
    test_activate_packet_command_type()
    test_activate_packet_encode()
    test_activate_packet_parse_response()
    test_set_patch_command_type()
    test_pump_control_encode_decode_roundtrip()

    print("=" * 60)
    print("所有测试通过!")
    print("=" * 60)


if __name__ == '__main__':
    run_tests()