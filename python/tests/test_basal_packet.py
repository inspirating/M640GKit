"""
================================================================================
BasalPacket 单元测试
================================================================================

测试基础率相关数据包的编码和响应解析

包含:
- SetBasalProfilePacket
- SetTempBasalPacket
- CancelTempBasalPacket

运行测试:
    import tests.test_basal_packet
    tests.test_basal_packet.run_tests()

作者: M640GKit Team
版本: 1.0.0
================================================================================
"""

import sys
sys.path.insert(0, '.')

from packets.basal_packet import SetBasalProfilePacket, SetTempBasalPacket, CancelTempBasalPacket
from enums import CommandType, BasalType
from encryption import crc8_calculate


def test_set_basal_profile_command_type():
    """测试SetBasalProfilePacket命令类型"""
    print("测试 test_set_basal_profile_command_type...")

    packet = SetBasalProfilePacket(b'\x00' * 48)

    assert packet.command_type == CommandType.SET_BASAL_PROFILE, \
        f"命令类型错误: {packet.command_type}"

    print("  ✓ 通过")


def test_set_basal_profile_encode():
    """测试SetBasalProfilePacket编码"""
    print("测试 test_set_basal_profile_encode...")

    basal_data = bytes([30] * 48)
    packet = SetBasalProfilePacket(basal_data)
    packages = packet.encode(1)

    assert len(packages) > 0, f"应生成至少1个包"

    pkg = packages[0]

    content = pkg[6:-2]
    assert content[0] == 1, f"首字节应为1: {content[0]}"

    print(f"  ✓ 通过 (生成了 {len(packages)} 个分包)")


def test_set_basal_profile_parse_response():
    """测试SetBasalProfilePacket响应解析"""
    print("测试 test_set_basal_profile_parse_response...")

    packet = SetBasalProfilePacket(b'\x00' * 48)
    response_data = b'\x11\x03\x01\x00\x00\x00\x02\x64\x00\x01\x00\x01\x00\x02\x00\x00\x00\x8a\x00'
    packet.decode(response_data)

    parsed = packet.parse_response()

    assert parsed['basal_type'] == BasalType.TEMP_BASAL, \
        f"基础率类型错误: {parsed['basal_type']}"
    assert parsed['basal_value'] == 0.5, f"基础率值错误: {parsed['basal_value']}"
    assert parsed['basal_sequence'] == 256, f"序列号错误: {parsed['basal_sequence']}"
    assert parsed['basal_patch_id'] == 1, f"Patch ID错误: {parsed['basal_patch_id']}"

    print(f"  ✓ 通过 (类型: {parsed['basal_type'].name}, 值: {parsed['basal_value']}U/h)")


def test_set_temp_basal_command_type():
    """测试SetTempBasalPacket命令类型"""
    print("测试 test_set_temp_basal_command_type...")

    packet = SetTempBasalPacket(2.0, 30)

    assert packet.command_type == CommandType.SET_TEMP_BASAL, \
        f"命令类型错误: {packet.command_type}"

    print("  ✓ 通过")


def test_set_temp_basal_encode():
    """测试SetTempBasalPacket编码"""
    print("测试 test_set_temp_basal_encode...")

    packet = SetTempBasalPacket(2.0, 30)
    packages = packet.encode(1)

    assert len(packages) == 1, f"期望1个包, 实际{len(packages)}"

    pkg = packages[0]

    content = pkg[6:-2]
    assert content[0] == 6, f"类型应为6(临时基础率): {content[0]}"

    rate_raw = int.from_bytes(content[1:3], 'little')
    assert rate_raw == 40, f"速率应为40(2.0/0.05): {rate_raw}"

    duration_raw = int.from_bytes(content[3:5], 'little')
    assert duration_raw == 30, f"持续时间应为30: {duration_raw}"

    print(f"  ✓ 通过 (速率: {rate_raw}, 时长: {duration_raw}分钟)")


def test_set_temp_basal_rate_conversion():
    """测试临时基础率速率转换"""
    print("测试 test_set_temp_basal_rate_conversion...")

    test_cases = [
        (0.05, 1),
        (0.1, 2),
        (0.5, 10),
        (1.0, 20),
        (2.0, 40),
        (5.0, 100),
    ]

    for rate, expected_raw in test_cases:
        packet = SetTempBasalPacket(rate, 30)
        content = packet.get_request_bytes()
        actual_raw = int.from_bytes(content[1:3], 'little')
        assert actual_raw == expected_raw, \
            f"速率{rate}应转换为{expected_raw}, 实际{actual_raw}"

    print("  ✓ 通过")


def test_set_temp_basal_parse_response():
    """测试SetTempBasalPacket响应解析"""
    print("测试 test_set_temp_basal_parse_response...")

    packet = SetTempBasalPacket(2.0, 30)
    response_data = b'\x11\x03\x01\x00\x00\x00\x02\x64\x00\x01\x00\x01\x00\x02\x00\x00\x00\x8a\x00'
    packet.decode(response_data)

    parsed = packet.parse_response()

    assert parsed['basal_type'] == BasalType.TEMP_BASAL, \
        f"基础率类型错误: {parsed['basal_type']}"
    assert parsed['basal_value'] == 0.5, f"基础率值错误: {parsed['basal_value']}"

    print(f"  ✓ 通过 (类型: {parsed['basal_type'].name}, 值: {parsed['basal_value']}U/h)")


def test_cancel_temp_basal_command_type():
    """测试CancelTempBasalPacket命令类型"""
    print("测试 test_cancel_temp_basal_command_type...")

    packet = CancelTempBasalPacket()

    assert packet.command_type == CommandType.CANCEL_TEMP_BASAL, \
        f"命令类型错误: {packet.command_type}"

    print("  ✓ 通过")


def test_cancel_temp_basal_encode():
    """测试CancelTempBasalPacket编码"""
    print("测试 test_cancel_temp_basal_encode...")

    packet = CancelTempBasalPacket()
    packages = packet.encode(1)

    assert len(packages) == 1, f"期望1个包, 实际{len(packages)}"

    pkg = packages[0]

    content = pkg[6:-2]
    assert len(content) == 0, f"取消临时基础率应无数据: {content.hex()}"

    print(f"  ✓ 通过 (编码: {pkg.hex()})")


def test_cancel_temp_basal_get_request_bytes():
    """测试CancelTempBasalPacket无请求数据"""
    print("测试 test_cancel_temp_basal_get_request_bytes...")

    packet = CancelTempBasalPacket()

    result = packet.get_request_bytes()

    assert result == b'', f"取消临时基础率应返回空字节: {result.hex()}"

    print("  ✓ 通过")


def test_basal_encode_decode_roundtrip():
    """测试基础率包编码解码往返"""
    print("测试 test_basal_encode_decode_roundtrip...")

    packet1 = SetTempBasalPacket(3.0, 45)
    packages = packet1.encode(42)

    packet2 = SetTempBasalPacket(0, 0)
    packet2.decode(packages[0])

    assert not packet2.failed, "解码过程不应该失败"

    content = packet2.get_request_bytes()
    rate_raw = int.from_bytes(content[1:3], 'little')
    assert rate_raw == 60, f"往返转换后速率应仍为60: {rate_raw}"

    print("  ✓ 通过")


def run_tests():
    """运行所有测试"""
    print("=" * 60)
    print("BasalPacket 单元测试")
    print("=" * 60)

    test_set_basal_profile_command_type()
    test_set_basal_profile_encode()
    test_set_basal_profile_parse_response()
    test_set_temp_basal_command_type()
    test_set_temp_basal_encode()
    test_set_temp_basal_rate_conversion()
    test_set_temp_basal_parse_response()
    test_cancel_temp_basal_command_type()
    test_cancel_temp_basal_encode()
    test_cancel_temp_basal_get_request_bytes()
    test_basal_encode_decode_roundtrip()

    print("=" * 60)
    print("所有测试通过!")
    print("=" * 60)


if __name__ == '__main__':
    run_tests()