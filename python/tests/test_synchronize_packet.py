"""
================================================================================
SynchronizePacket 单元测试
================================================================================

测试同步请求包的编码和响应解析

运行测试:
    import tests.test_synchronize_packet
    tests.test_synchronize_packet.run_tests()

作者: M640GKit Team
版本: 1.0.0
================================================================================
"""

import sys
sys.path.insert(0, '.')

from packets.synchronize_packet import SynchronizePacket, SynchronizeResponseParser
from enums import CommandType, PatchState
from encryption import crc8_calculate


def test_command_type():
    """测试命令类型"""
    print("测试 test_command_type...")

    packet = SynchronizePacket()

    assert packet.command_type == CommandType.SYNCHRONIZE, \
        f"命令类型错误: {packet.command_type}"

    print("  ✓ 通过")


def test_encode_request():
    """测试请求编码"""
    print("测试 test_encode_request...")

    packet = SynchronizePacket()
    packages = packet.encode(1)

    assert len(packages) == 1, f"期望1个包, 实际{len(packages)}"

    pkg = packages[0]

    assert pkg[0] == len(pkg), f"长度字节不匹配: {pkg[0]} vs {len(pkg)}"
    assert pkg[1] == CommandType.SYNCHRONIZE, f"命令类型不匹配: {pkg[1]}"
    assert pkg[2] == 1, f"序列号不匹配: {pkg[2]}"
    assert pkg[3] == 0, f"包索引不匹配: {pkg[3]}"

    content = pkg[6:-2]
    assert len(content) == 0, f"同步请求应无数据内容: {content.hex()}"

    calculated_crc = crc8_calculate(pkg[:-2])
    assert pkg[-2] == calculated_crc, f"CRC不匹配: {pkg[-2]:02X} vs {calculated_crc:02X}"

    print(f"  ✓ 通过 (编码长度: {len(pkg)} 字节)")


def test_get_request_bytes():
    """测试get_request_bytes方法"""
    print("测试 test_get_request_bytes...")

    packet = SynchronizePacket()

    result = packet.get_request_bytes()

    assert result == b'', f"同步请求应返回空字节: {result.hex()}"

    print("  ✓ 通过")


def test_parse_response():
    """测试响应解析"""
    print("测试 test_parse_response...")

    packet = SynchronizePacket()

    response_data = b'\x0f\x03\x01\x00\x00\x00\x01\x02\x00\x01\x02\x03\x04\x8a\x00'
    packet.decode(response_data)

    assert packet.response_code == 0, f"响应码错误: {packet.response_code}"

    parsed = packet.parse_response()

    assert parsed['state'] == PatchState.RUNNING, f"状态错误: {parsed['state']}"
    assert parsed['field_mask'] == 0x0002, f"字段掩码错误: {hex(parsed['field_mask'])}"
    assert parsed['sync_data'] == b'\x01\x02\x03\x04', f"同步数据错误: {parsed['sync_data'].hex()}"

    print(f"  ✓ 通过 (状态: {parsed['state'].name}, 掩码: {hex(parsed['field_mask'])})")


def test_minimum_data_size():
    """测试minimum_data_size属性"""
    print("测试 test_minimum_data_size...")

    packet = SynchronizePacket()

    assert packet.minimum_data_size == 3, \
        f"minimum_data_size错误: {packet.minimum_data_size}"

    print("  ✓ 通过")


def test_synchronize_response_parser_masks():
    """测试响应解析器的掩码定义"""
    print("测试 test_synchronize_response_parser_masks...")

    assert SynchronizeResponseParser.MASK_SUSPEND == 0x0001
    assert SynchronizeResponseParser.MASK_NORMAL_BOLUS == 0x0002
    assert SynchronizeResponseParser.MASK_EXTENDED_BOLUS == 0x0004
    assert SynchronizeResponseParser.MASK_BASAL == 0x0008
    assert SynchronizeResponseParser.MASK_SETUP == 0x0010
    assert SynchronizeResponseParser.MASK_RESERVOIR == 0x0020
    assert SynchronizeResponseParser.MASK_START_TIME == 0x0040
    assert SynchronizeResponseParser.MASK_BATTERY == 0x0080
    assert SynchronizeResponseParser.MASK_STORAGE == 0x0100
    assert SynchronizeResponseParser.MASK_ALARM == 0x0200
    assert SynchronizeResponseParser.MASK_AGE == 0x0400
    assert SynchronizeResponseParser.MASK_MAGNETO_PLACE == 0x0800

    print("  ✓ 通过")


def test_is_complete():
    """测试is_complete属性"""
    print("测试 test_is_complete...")

    packet = SynchronizePacket()
    packet.data_size = 10
    packet.total_data = b'\x00\x01\x02\x03\x04'

    assert not packet.is_complete, "数据不完整时is_complete应为False"

    packet.total_data = b'\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09'

    assert packet.is_complete, "数据完整时is_complete应为True"

    print("  ✓ 通过")


def run_tests():
    """运行所有测试"""
    print("=" * 60)
    print("SynchronizePacket 单元测试")
    print("=" * 60)

    test_command_type()
    test_encode_request()
    test_get_request_bytes()
    test_parse_response()
    test_minimum_data_size()
    test_synchronize_response_parser_masks()
    test_is_complete()

    print("=" * 60)
    print("所有测试通过!")
    print("=" * 60)


if __name__ == '__main__':
    run_tests()