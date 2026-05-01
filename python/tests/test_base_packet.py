"""
================================================================================
BasePacket 单元测试
================================================================================

测试数据包基类的编解码、分包处理和完整性校验

运行测试:
    import tests.test_base_packet
    tests.test_base_packet.run_tests()

作者: M640GKit Team
版本: 1.0.0
================================================================================
"""

import sys
sys.path.insert(0, '.')

from encryption import crc8_calculate
from packets.base_packet import BasePacket
from enums import CommandType


class TestPacket(BasePacket):
    """测试用数据包类"""
    command_type = CommandType.SYNCHRONIZE

    def __init__(self, data: bytes = b''):
        super().__init__()
        self._test_data = data

    def get_request_bytes(self) -> bytes:
        return self._test_data


def test_encode_single_package():
    """测试单包编码"""
    print("测试 test_encode_single_package...")

    packet = TestPacket(b'\x01\x02\x03')
    packages = packet.encode(1)

    assert len(packages) == 1, f"期望1个包, 实际{len(packages)}"

    pkg = packages[0]
    assert pkg[0] == len(pkg), f"长度字节不匹配: {pkg[0]} vs {len(pkg)}"
    assert pkg[1] == CommandType.SYNCHRONIZE, f"命令类型不匹配: {pkg[1]}"
    assert pkg[2] == 1, f"序列号不匹配: {pkg[2]}"
    assert pkg[3] == 0, f"包索引不匹配: {pkg[3]}"

    calculated_crc = crc8_calculate(pkg[:-2])
    assert pkg[-2] == calculated_crc, f"CRC不匹配: {pkg[-2]:02X} vs {calculated_crc:02X}"

    print("  ✓ 通过")


def test_encode_multiple_packages():
    """测试分包编码 (数据超过15字节)"""
    print("测试 test_encode_multiple_packages...")

    large_data = b'\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14'
    packet = TestPacket(large_data)
    packages = packet.encode(5)

    assert len(packages) > 1, "大数据应该分包"

    for i, pkg in enumerate(packages):
        assert pkg[0] == len(pkg), f"包{i+1}长度字节不匹配"
        assert pkg[2] == 5, f"包{i+1}序列号应为5"
        assert pkg[3] == i + 1, f"包{i+1}索引应为{i+1}"

        calculated_crc = crc8_calculate(pkg[:-2])
        assert pkg[-2] == calculated_crc, f"包{i+1} CRC不匹配"

    print(f"  ✓ 通过 (生成了 {len(packages)} 个分包)")


def test_decode_single_package():
    """测试单包解码"""
    print("测试 test_decode_single_package...")

    raw_data = b'\x08\x03\x01\x00\x00\x00\x01\x02\x03\x8a\x00'
    packet = TestPacket()
    packet.decode(raw_data)

    assert packet.data_size == 8, f"data_size不匹配: {packet.data_size}"
    assert packet.response_code == 0, f"response_code不匹配: {packet.response_code}"
    assert packet.sequence_number == 0, f"sequence_number不匹配: {packet.sequence_number}"
    assert packet.total_data == raw_data[:-1], f"total_data不匹配"
    assert not packet.failed, "packet不应该标记为失败"

    print("  ✓ 通过")


def test_decode_with_wrong_crc():
    """测试CRC校验失败"""
    print("测试 test_decode_with_wrong_crc...")

    raw_data = b'\x08\x03\x01\x00\x00\x00\x01\x02\x03\x00\x00'
    packet = TestPacket()
    packet.decode(raw_data)

    assert packet.failed, "CRC错误应该标记packet为失败"

    print("  ✓ 通过")


def test_decode_fragmented_package():
    """测试分包解码"""
    print("测试 test_decode_fragmented_package...")

    packet = TestPacket()

    pkg1 = b'\x14\x03\x01\x01\x00\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x00'
    packet.decode(pkg1)

    assert packet.data_size == 20, f"data_size不匹配: {packet.data_size}"
    assert packet.sequence_number == 1, f"sequence_number不匹配: {packet.sequence_number}"
    assert len(packet.total_data) == 16, f"第一包后total_data长度: {len(packet.total_data)}"

    pkg2 = b'\x0a\x03\x01\x02\x00\x00\x0f\x10\x11\x12\x13\x14\x8a\x00'
    packet.decode(pkg2)

    assert packet.sequence_number == 2, f"sequence_number不匹配: {packet.sequence_number}"
    assert len(packet.total_data) == 20, f"分包后total_data长度: {len(packet.total_data)}"

    print("  ✓ 通过")


def test_decode_fragmented_with_wrong_sequence():
    """测试分包序列号错误"""
    print("测试 test_decode_fragmented_with_wrong_sequence...")

    packet = TestPacket()

    pkg1 = b'\x14\x03\x01\x01\x00\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x00'
    packet.decode(pkg1)

    pkg2 = b'\x0a\x03\x01\x03\x00\x00\x0f\x10\x11\x12\x13\x14\x8a\x00'
    packet.decode(pkg2)

    assert packet.failed, "序列号跳跃应该标记packet为失败"

    print("  ✓ 通过")


def test_is_complete():
    """测试is_complete属性"""
    print("测试 test_is_complete...")

    packet = TestPacket()
    packet.data_size = 10
    packet.total_data = b'\x00\x01\x02\x03\x04'

    assert not packet.is_complete, "数据不完整时is_complete应为False"

    packet.total_data = b'\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09'

    assert packet.is_complete, "数据完整时is_complete应为True"

    print("  ✓ 通过")


def test_has_enough_data():
    """测试has_enough_data属性"""
    print("测试 test_has_enough_data...")

    packet = TestPacket()
    packet.minimum_data_size = 5
    packet.total_data = b'\x00\x01\x02'

    assert not packet.has_enough_data, "数据不足时has_enough_data应为False"

    packet.total_data = b'\x00\x01\x02\x03\x04\x05'

    assert packet.has_enough_data, "数据足够时has_enough_data应为True"

    print("  ✓ 通过")


def test_get_request_bytes():
    """测试get_request_bytes方法"""
    print("测试 test_get_request_bytes...")

    packet = TestPacket(b'\x01\x02\x03')

    result = packet.get_request_bytes()

    assert result == b'\x01\x02\x03', f"get_request_bytes返回错误: {result.hex()}"

    print("  ✓ 通过")


def run_tests():
    """运行所有测试"""
    print("=" * 60)
    print("BasePacket 单元测试")
    print("=" * 60)

    test_encode_single_package()
    test_encode_multiple_packages()
    test_decode_single_package()
    test_decode_with_wrong_crc()
    test_decode_fragmented_package()
    test_decode_fragmented_with_wrong_sequence()
    test_is_complete()
    test_has_enough_data()
    test_get_request_bytes()

    print("=" * 60)
    print("所有测试通过!")
    print("=" * 60)


if __name__ == '__main__':
    run_tests()