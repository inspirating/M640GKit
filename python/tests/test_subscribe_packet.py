"""
================================================================================
SubscribePacket 单元测试
================================================================================

测试订阅请求包的编码

运行测试:
    import tests.test_subscribe_packet
    tests.test_subscribe_packet.run_tests()

作者: M640GKit Team
版本: 1.0.0
================================================================================
"""

import sys
sys.path.insert(0, '.')

from packets.subscribe_packet import SubscribePacket
from enums import CommandType
from encryption import crc8_calculate


def test_command_type():
    """测试命令类型"""
    print("测试 test_command_type...")

    packet = SubscribePacket()

    assert packet.command_type == CommandType.SUBSCRIBE, \
        f"命令类型错误: {packet.command_type}"

    print("  ✓ 通过")


def test_encode():
    """测试编码"""
    print("测试 test_encode...")

    packet = SubscribePacket()
    packages = packet.encode(1)

    assert len(packages) == 1, f"期望1个包, 实际{len(packages)}"

    pkg = packages[0]

    assert pkg[0] == len(pkg), f"长度字节不匹配: {pkg[0]} vs {len(pkg)}"
    assert pkg[1] == CommandType.SUBSCRIBE, f"命令类型不匹配: {pkg[1]}"
    assert pkg[2] == 1, f"序列号不匹配: {pkg[2]}"
    assert pkg[3] == 0, f"包索引不匹配: {pkg[3]}"

    content = pkg[6:-2]
    assert len(content) == 0, f"订阅请求应无数据: {content.hex()}"

    calculated_crc = crc8_calculate(pkg[:-2])
    assert pkg[-2] == calculated_crc, f"CRC不匹配: {pkg[-2]:02X} vs {calculated_crc:02X}"

    print(f"  ✓ 通过 (编码长度: {len(pkg)} 字节)")


def test_get_request_bytes():
    """测试get_request_bytes方法"""
    print("测试 test_get_request_bytes...")

    packet = SubscribePacket()

    result = packet.get_request_bytes()

    assert result == b'', f"订阅请求应返回空字节: {result.hex()}"

    print("  ✓ 通过")


def test_encode_decode_roundtrip():
    """测试编码解码往返"""
    print("测试 test_encode_decode_roundtrip...")

    packet1 = SubscribePacket()
    packages = packet1.encode(42)

    assert len(packages) == 1, f"编码后应有1个包"

    packet2 = SubscribePacket()
    packet2.decode(packages[0])

    assert not packet2.failed, "解码过程不应该失败"
    assert packet2.data_size == len(packages[0]), "数据长度不匹配"

    print("  ✓ 通过")


def test_different_sequence_numbers():
    """测试不同序列号"""
    print("测试 test_different_sequence_numbers...")

    for seq in [0, 1, 127, 255]:
        packet = SubscribePacket()
        packages = packet.encode(seq)

        pkg = packages[0]
        assert pkg[2] == seq, f"序列号应为{seq}: {pkg[2]}"

    print("  ✓ 通过")


def run_tests():
    """运行所有测试"""
    print("=" * 60)
    print("SubscribePacket 单元测试")
    print("=" * 60)

    test_command_type()
    test_encode()
    test_get_request_bytes()
    test_encode_decode_roundtrip()
    test_different_sequence_numbers()

    print("=" * 60)
    print("所有测试通过!")
    print("=" * 60)


if __name__ == '__main__':
    run_tests()