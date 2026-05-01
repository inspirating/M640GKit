"""
================================================================================
BolusPacket 单元测试
================================================================================

测试大剂量相关数据包的编码和响应解析

包含:
- SetBolusPacket
- CancelBolusPacket
- ReadBolusStatePacket

运行测试:
    import tests.test_bolus_packet
    tests.test_bolus_packet.run_tests()

作者: M640GKit Team
版本: 1.0.0
================================================================================
"""

import sys
sys.path.insert(0, '.')

from packets.bolus_packet import SetBolusPacket, CancelBolusPacket, ReadBolusStatePacket
from enums import CommandType
from encryption import crc8_calculate


def test_set_bolus_command_type():
    """测试SetBolusPacket命令类型"""
    print("测试 test_set_bolus_command_type...")

    packet = SetBolusPacket(5.0, 1)

    assert packet.command_type == CommandType.SET_BOLUS, \
        f"命令类型错误: {packet.command_type}"

    print("  ✓ 通过")


def test_set_bolus_encode():
    """测试SetBolusPacket编码"""
    print("测试 test_set_bolus_encode...")

    packet = SetBolusPacket(5.0, 1)
    packages = packet.encode(1)

    assert len(packages) == 1, f"期望1个包, 实际{len(packages)}"

    pkg = packages[0]

    content = pkg[6:-2]
    assert content[0] == 1, f"大剂量类型应为1: {content[0]}"
    amount_raw = int.from_bytes(content[1:3], 'little')
    assert amount_raw == 100, f"剂量应为100(5.0/0.05): {amount_raw}"
    assert content[3] == 0, f"末尾应为0: {content[3]}"

    print(f"  ✓ 通过 (编码: {pkg.hex()})")


def test_set_bolus_amount_conversion():
    """测试大剂量金额转换"""
    print("测试 test_set_bolus_amount_conversion...")

    test_cases = [
        (0.05, 1),
        (0.1, 2),
        (0.5, 10),
        (1.0, 20),
        (5.0, 100),
        (10.0, 200),
    ]

    for amount, expected_raw in test_cases:
        packet = SetBolusPacket(amount, 1)
        content = packet.get_request_bytes()
        actual_raw = int.from_bytes(content[1:3], 'little')
        assert actual_raw == expected_raw, \
            f"金额{amount}应转换为{expected_raw}, 实际{actual_raw}"

    print("  ✓ 通过")


def test_set_bolus_types():
    """测试不同大剂量类型"""
    print("测试 test_set_bolus_types...")

    for bolus_type in [1, 2, 3]:
        packet = SetBolusPacket(5.0, bolus_type)
        content = packet.get_request_bytes()
        assert content[0] == bolus_type, \
            f"类型{bolus_type}不匹配: {content[0]}"

    print("  ✓ 通过")


def test_cancel_bolus_command_type():
    """测试CancelBolusPacket命令类型"""
    print("测试 test_cancel_bolus_command_type...")

    packet = CancelBolusPacket(1)

    assert packet.command_type == CommandType.CANCEL_BOLUS, \
        f"命令类型错误: {packet.command_type}"

    print("  ✓ 通过")


def test_cancel_bolus_encode():
    """测试CancelBolusPacket编码"""
    print("测试 test_cancel_bolus_encode...")

    packet = CancelBolusPacket(2)
    packages = packet.encode(5)

    assert len(packages) == 1, f"期望1个包, 实际{len(packages)}"

    pkg = packages[0]

    content = pkg[6:-2]
    assert content == b'\x02', f"取消类型应为2: {content.hex()}"

    print(f"  ✓ 通过 (编码: {pkg.hex()})")


def test_cancel_bolus_default_type():
    """测试CancelBolusPacket默认类型"""
    print("测试 test_cancel_bolus_default_type...")

    packet = CancelBolusPacket()
    content = packet.get_request_bytes()

    assert content == b'\x01', f"默认类型应为1: {content.hex()}"

    print("  ✓ 通过")


def test_read_bolus_state_command_type():
    """测试ReadBolusStatePacket命令类型"""
    print("测试 test_read_bolus_state_command_type...")

    packet = ReadBolusStatePacket()

    assert packet.command_type == CommandType.READ_BOLUS_STATE, \
        f"命令类型错误: {packet.command_type}"

    print("  ✓ 通过")


def test_read_bolus_state_encode():
    """测试ReadBolusStatePacket编码"""
    print("测试 test_read_bolus_state_encode...")

    packet = ReadBolusStatePacket(0)
    packages = packet.encode(1)

    assert len(packages) == 1, f"期望1个包, 实际{len(packages)}"

    pkg = packages[0]

    content = pkg[6:-2]
    assert content == b'\x00', f"大剂量ID应为0: {content.hex()}"

    print(f"  ✓ 通过 (编码: {pkg.hex()})")


def test_read_bolus_state_parse_response():
    """测试ReadBolusStatePacket响应解析"""
    print("测试 test_read_bolus_state_parse_response...")

    packet = ReadBolusStatePacket()
    response_data = b'\x10\x03\x01\x00\x00\x00\x01\x01\x20\x04\x20\x04\x00\x00\x8a\x00'
    packet.decode(response_data)

    parsed = packet.parse_response()

    assert parsed['bolus_id'] == 1, f"大剂量ID错误: {parsed['bolus_id']}"
    assert parsed['state'] == 'active', f"状态错误: {parsed['state']}"
    assert parsed['delivered'] == 1.0, f"已输送剂量错误: {parsed['delivered']}"
    assert parsed['remaining'] == 1.0, f"剩余剂量错误: {parsed['remaining']}"

    print(f"  ✓ 通过 (已输送: {parsed['delivered']}U, 剩余: {parsed['remaining']}U)")


def test_bolus_encode_decode_roundtrip():
    """测试大剂量包编码解码往返"""
    print("测试 test_bolus_encode_decode_roundtrip...")

    packet1 = SetBolusPacket(7.5, 2)
    packages = packet1.encode(42)

    packet2 = SetBolusPacket(0, 0)
    packet2.decode(packages[0])

    assert not packet2.failed, "解码过程不应该失败"

    content = packet2.get_request_bytes()
    amount_raw = int.from_bytes(content[1:3], 'little')
    assert amount_raw == 150, f"往返转换后金额应仍为150: {amount_raw}"

    print("  ✓ 通过")


def run_tests():
    """运行所有测试"""
    print("=" * 60)
    print("BolusPacket 单元测试")
    print("=" * 60)

    test_set_bolus_command_type()
    test_set_bolus_encode()
    test_set_bolus_amount_conversion()
    test_set_bolus_types()
    test_cancel_bolus_command_type()
    test_cancel_bolus_encode()
    test_cancel_bolus_default_type()
    test_read_bolus_state_command_type()
    test_read_bolus_state_encode()
    test_read_bolus_state_parse_response()
    test_bolus_encode_decode_roundtrip()

    print("=" * 60)
    print("所有测试通过!")
    print("=" * 60)


if __name__ == '__main__':
    run_tests()