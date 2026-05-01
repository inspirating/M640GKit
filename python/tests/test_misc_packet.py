"""
================================================================================
MiscPacket 单元测试
================================================================================

测试杂项数据包的编码和响应解析

包含:
- PollPatchPacket
- GetDeviceTypePacket
- GetRecordPacket

运行测试:
    import tests.test_misc_packet
    tests.test_misc_packet.run_tests()

作者: M640GKit Team
版本: 1.0.0
================================================================================
"""

import sys
sys.path.insert(0, '.')

from packets.misc_packet import PollPatchPacket, GetDeviceTypePacket, GetRecordPacket
from enums import CommandType, PatchState
from encryption import crc8_calculate


def test_poll_patch_command_type():
    """测试PollPatchPacket命令类型"""
    print("测试 test_poll_patch_command_type...")

    packet = PollPatchPacket()

    assert packet.command_type == CommandType.POLL_PATCH, \
        f"命令类型错误: {packet.command_type}"

    print("  ✓ 通过")


def test_poll_patch_encode():
    """测试PollPatchPacket编码"""
    print("测试 test_poll_patch_encode...")

    packet = PollPatchPacket(0)
    packages = packet.encode(1)

    assert len(packages) == 1, f"期望1个包, 实际{len(packages)}"

    pkg = packages[0]

    content = pkg[6:-2]
    assert len(content) == 2, f"轮询包数据长度应为2: {len(content)}"

    seq = int.from_bytes(content, 'little')
    assert seq == 0, f"序列号应为0: {seq}"

    print(f"  ✓ 通过 (编码: {pkg.hex()})")


def test_poll_patch_with_last_sequence():
    """测试PollPatchPacket带上次序列号"""
    print("测试 test_poll_patch_with_last_sequence...")

    packet = PollPatchPacket(100)
    packages = packet.encode(1)

    pkg = packages[0]
    content = pkg[6:-2]

    seq = int.from_bytes(content, 'little')
    assert seq == 100, f"序列号应为100: {seq}"

    print(f"  ✓ 通过 (上次序列号: {seq})")


def test_poll_patch_parse_response():
    """测试PollPatchPacket响应解析"""
    print("测试 test_poll_patch_parse_response...")

    packet = PollPatchPacket()
    response_data = b'\x0b\x03\x01\x00\x00\x00\x01\x00\x64\x02\x03\x01\x8a\x00'
    packet.decode(response_data)

    parsed = packet.parse_response()

    assert parsed['sequence'] == 1, f"序列号错误: {parsed['sequence']}"
    assert parsed['patch_state'] == PatchState.RUNNING, \
        f"Patch状态错误: {parsed['patch_state']}"
    assert len(parsed['active_alarms']) == 3, \
        f"警报数量错误: {len(parsed['active_alarms'])}"

    print(f"  ✓ 通过 (状态: {parsed['patch_state'].name})")


def test_get_device_type_command_type():
    """测试GetDeviceTypePacket命令类型"""
    print("测试 test_get_device_type_command_type...")

    packet = GetDeviceTypePacket()

    assert packet.command_type == CommandType.GET_DEVICE_TYPE, \
        f"命令类型错误: {packet.command_type}"

    print("  ✓ 通过")


def test_get_device_type_encode():
    """测试GetDeviceTypePacket编码"""
    print("测试 test_get_device_type_encode...")

    packet = GetDeviceTypePacket()
    packages = packet.encode(1)

    assert len(packages) == 1, f"期望1个包, 实际{len(packages)}"

    pkg = packages[0]

    content = pkg[6:-2]
    assert len(content) == 0, f"获取设备类型应无数据: {content.hex()}"

    print(f"  ✓ 通过 (编码: {pkg.hex()})")


def test_get_device_type_parse_response():
    """测试GetDeviceTypePacket响应解析"""
    print("测试 test_get_device_type_parse_response...")

    packet = GetDeviceTypePacket()
    response_data = b'\x10\x03\x01\x00\x00\x00\x01\x03\x05\x02\x01\x00\x12\x34\x56\x78\x8a\x00'
    packet.decode(response_data)

    parsed = packet.parse_response()

    assert parsed['device_type'] == 1, f"设备类型错误: {parsed['device_type']}"
    assert parsed['hw_version'] == "3.5", f"硬件版本错误: {parsed['hw_version']}"
    assert parsed['sw_version'] == "2.1.0", f"软件版本错误: {parsed['sw_version']}"
    assert parsed['serial'] == "12345678", f"序列号错误: {parsed['serial']}"

    print(f"  ✓ 通过 (设备: {parsed['device_type']}, SW: {parsed['sw_version']})")


def test_get_record_command_type():
    """测试GetRecordPacket命令类型"""
    print("测试 test_get_record_command_type...")

    packet = GetRecordPacket(1)

    assert packet.command_type == CommandType.GET_RECORD, \
        f"命令类型错误: {packet.command_type}"

    print("  ✓ 通过")


def test_get_record_encode():
    """测试GetRecordPacket编码"""
    print("测试 test_get_record_encode...")

    packet = GetRecordPacket(1, 0, 10)
    packages = packet.encode(1)

    assert len(packages) == 1, f"期望1个包, 实际{len(packages)}"

    pkg = packages[0]

    content = pkg[6:-2]
    assert content[0] == 1, f"记录类型应为1: {content[0]}"
    assert int.from_bytes(content[1:3], 'little') == 0, f"起始索引应为0"
    assert content[3] == 10, f"数量应为10: {content[3]}"

    print(f"  ✓ 通过 (编码: {pkg.hex()})")


def test_get_record_types():
    """测试不同记录类型"""
    print("测试 test_get_record_types...")

    record_types = [1, 2, 3, 4]

    for record_type in record_types:
        packet = GetRecordPacket(record_type)
        content = packet.get_request_bytes()
        assert content[0] == record_type, \
            f"记录类型{record_type}不匹配: {content[0]}"

    print("  ✓ 通过")


def test_get_record_parse_response():
    """测试GetRecordPacket响应解析"""
    print("测试 test_get_record_parse_response...")

    packet = GetRecordPacket(1)
    response_data = b'\x0f\x03\x01\x00\x00\x00\x02\x00\x00\x00\x01\x5a\x00\x00\x01\x02\x00\x00\x00\x8a\x00'
    packet.decode(response_data)

    parsed = packet.parse_response()

    assert parsed['record_count'] == 2, f"记录数量错误: {parsed['record_count']}"
    assert len(parsed['records']) == 2, f"解析记录数错误: {len(parsed['records'])}"
    assert parsed['records'][0]['timestamp'] == 0x5A000000, \
        f"第一条记录时间戳错误: {hex(parsed['records'][0]['timestamp'])}"

    print(f"  ✓ 通过 (记录数: {parsed['record_count']})")


def test_misc_encode_decode_roundtrip():
    """测试杂项包编码解码往返"""
    print("测试 test_misc_encode_decode_roundtrip...")

    packet1 = GetRecordPacket(2, 5, 20)
    packages = packet1.encode(42)

    packet2 = GetRecordPacket(0)
    packet2.decode(packages[0])

    assert not packet2.failed, "解码过程不应该失败"

    content = packet2.get_request_bytes()
    assert content[0] == 2, f"往返后记录类型应仍为2: {content[0]}"

    print("  ✓ 通过")


def run_tests():
    """运行所有测试"""
    print("=" * 60)
    print("MiscPacket 单元测试")
    print("=" * 60)

    test_poll_patch_command_type()
    test_poll_patch_encode()
    test_poll_patch_with_last_sequence()
    test_poll_patch_parse_response()
    test_get_device_type_command_type()
    test_get_device_type_encode()
    test_get_device_type_parse_response()
    test_get_record_command_type()
    test_get_record_encode()
    test_get_record_types()
    test_get_record_parse_response()
    test_misc_encode_decode_roundtrip()

    print("=" * 60)
    print("所有测试通过!")
    print("=" * 60)


if __name__ == '__main__':
    run_tests()