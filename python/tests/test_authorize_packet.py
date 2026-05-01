"""
================================================================================
AuthorizePacket 单元测试
================================================================================

测试认证请求包的编码和响应解析

运行测试:
    import tests.test_authorize_packet
    tests.test_authorize_packet.run_tests()

作者: M640GKit Team
版本: 1.0.0
================================================================================
"""

import sys
sys.path.insert(0, '.')

from packets.authorize_packet import AuthorizePacket
from enums import CommandType
from encryption import Crypto, crc8_calculate


def test_command_type():
    """测试命令类型"""
    print("测试 test_command_type...")

    packet = AuthorizePacket(b'\x01\x02\x03\x04', b'\x11\x22\x33\x44')

    assert packet.command_type == CommandType.AUTH_REQ, \
        f"命令类型错误: {packet.command_type}"

    print("  ✓ 通过")


def test_encode_request():
    """测试请求编码"""
    print("测试 test_encode_request...")

    pump_sn = b'\x4a\x12\xd8\x28'
    session_token = b'\x11\x22\x33\x44'

    packet = AuthorizePacket(pump_sn, session_token)
    packages = packet.encode(1)

    assert len(packages) == 1, f"期望1个包, 实际{len(packages)}"

    pkg = packages[0]

    assert pkg[0] == len(pkg), f"长度字节不匹配: {pkg[0]} vs {len(pkg)}"
    assert pkg[1] == CommandType.AUTH_REQ, f"命令类型不匹配: {pkg[1]}"
    assert pkg[2] == 1, f"序列号不匹配: {pkg[2]}"
    assert pkg[3] == 0, f"包索引不匹配: {pkg[3]}"

    calculated_crc = crc8_calculate(pkg[:-2])
    assert pkg[-2] == calculated_crc, f"CRC不匹配: {pkg[-2]:02X} vs {calculated_crc:02X}"

    content = pkg[6:-2]
    assert content[0] == 2, f"角色应为2(控制器): {content[0]}"
    assert content[1:5] == session_token, f"会话令牌不匹配: {content[1:5].hex()}"

    print(f"  ✓ 通过 (编码长度: {len(pkg)} 字节)")


def test_pump_sn_reversed():
    """测试泵序列号字节反转"""
    print("测试 test_pump_sn_reversed...")

    pump_sn = b'\x4a\x12\xd8\x28'
    session_token = b'\x11\x22\x33\x44'

    packet = AuthorizePacket(pump_sn, session_token)

    expected_sn = bytes(reversed(pump_sn))
    assert packet.pump_sn == expected_sn, \
        f"泵序列号反转错误: {packet.pump_sn.hex()} vs {expected_sn.hex()}"

    print("  ✓ 通过")


def test_key_generation():
    """测试密钥生成"""
    print("测试 test_key_generation...")

    pump_sn = b'\x4a\x12\xd8\x28'
    session_token = b'\x11\x22\x33\x44'

    packet = AuthorizePacket(pump_sn, session_token)
    key = Crypto.gen_key(packet.pump_sn)

    assert len(key) == 4, f"密钥长度错误: {len(key)}"

    expected_key = Crypto.gen_key(bytes(reversed(pump_sn)))
    assert key == expected_key, f"密钥不匹配: {key.hex()} vs {expected_key.hex()}"

    print(f"  ✓ 通过 (生成的密钥: {key.hex()})")


def test_parse_response():
    """测试响应解析"""
    print("测试 test_parse_response...")

    pump_sn = b'\x4a\x12\xd8\x28'
    session_token = b'\x11\x22\x33\x44'

    packet = AuthorizePacket(pump_sn, session_token)

    response_data = b'\x10\x03\x01\x00\x00\x00\x02\x01\x01\x00\x00\x8a\x8a\x00\x00'
    packet.decode(response_data)

    assert packet.response_code == 0, f"响应码错误: {packet.response_code}"
    assert packet.data_size == 16, f"数据长度错误: {packet.data_size}"

    parsed = packet.parse_response()

    assert parsed['device_type'] == 1, f"设备类型错误: {parsed['device_type']}"
    assert parsed['sw_version'] == "1.0.0", f"软件版本错误: {parsed['sw_version']}"

    print(f"  ✓ 通过 (设备类型: {parsed['device_type']}, 版本: {parsed['sw_version']})")


def test_minimum_data_size():
    """测试minimum_data_size属性"""
    print("测试 test_minimum_data_size...")

    packet = AuthorizePacket(b'\x01\x02\x03\x04', b'\x11\x22\x33\x44')

    assert packet.minimum_data_size == 10, \
        f"minimum_data_size错误: {packet.minimum_data_size}"

    print("  ✓ 通过")


def test_encode_and_decode_roundtrip():
    """测试编码解码往返"""
    print("测试 test_encode_and_decode_roundtrip...")

    pump_sn = b'\x4a\x12\xd8\x28'
    session_token = b'\x11\x22\x33\x44'

    packet1 = AuthorizePacket(pump_sn, session_token)
    packages = packet1.encode(42)

    assert len(packages) == 1, f"编码后应有1个包"

    packet2 = AuthorizePacket(pump_sn, session_token)
    packet2.decode(packages[0])

    assert not packet2.failed, "解码过程不应该失败"
    assert packet2.data_size == len(packages[0]), "数据长度不匹配"

    print("  ✓ 通过")


def run_tests():
    """运行所有测试"""
    print("=" * 60)
    print("AuthorizePacket 单元测试")
    print("=" * 60)

    test_command_type()
    test_encode_request()
    test_pump_sn_reversed()
    test_key_generation()
    test_parse_response()
    test_minimum_data_size()
    test_encode_and_decode_roundtrip()

    print("=" * 60)
    print("所有测试通过!")
    print("=" * 60)


if __name__ == '__main__':
    run_tests()