"""
================================================================================
Packet 测试脚本
================================================================================

验证 packet 编码/解码、CRC校验、分包处理是否正确
================================================================================
"""

import sys
sys.path.insert(0, '.')

from encryption import crc8_calculate, Crypto
from packets import AuthorizePacket, SynchronizePacket, SetBolusPacket


def test_crc8():
    """测试 CRC8 计算"""
    print("=" * 60)
    print("测试 CRC8")
    print("=" * 60)

    test_data = b'\x14\x03\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'
    expected_crc = 0x8A

    calculated_crc = crc8_calculate(test_data)
    print(f"  测试数据: {test_data.hex()}")
    print(f"  期望 CRC: {expected_crc:02X}")
    print(f"  计算 CRC: {calculated_crc:02X}")
    print(f"  结果: {'✓ 通过' if calculated_crc == expected_crc else '✗ 失败'}")
    print()


def test_authorize_request():
    """测试 AuthorizePacket 编码"""
    print("=" * 60)
    print("测试 AuthorizePacket 编码")
    print("=" * 60)

    pump_sn = b'\x28\xd8\x12\x4a'
    session_token = b'\x11\x22\x33\x44'

    packet = AuthorizePacket(pump_sn, session_token)
    packages = packet.encode(1)

    print(f"  序列号: {packet.sequence_number}")
    print(f"  包数量: {len(packages)}")

    for i, pkg in enumerate(packages):
        print(f"  包 {i + 1}: {pkg.hex()}")
        print(f"    长度: {pkg[0]}")
        print(f"    命令: {pkg[1]}")
        print(f"    序列: {pkg[2]}")
        print(f"    索引: {pkg[3]}")
        print(f"    CRC: {pkg[-2]:02X}")

    print()


def test_authorize_response_decode():
    """测试 AuthorizePacket 响应解码"""
    print("=" * 60)
    print("测试 AuthorizePacket 响应解码")
    print("=" * 60)

    response_data = b'\x10\x03\x01\x00\x00\x00\x02\x01\x01\x00\x00\x8a\x8a\x00\x00'

    print(f"  响应数据: {response_data.hex()}")
    print(f"  数据长度: {len(response_data)}")

    packet = SynchronizePacket()
    packet.decode(response_data)

    print(f"  命令类型: {packet.command_type}")
    print(f"  data_size: {packet.data_size}")
    print(f"  response_code: {packet.response_code}")
    print(f"  sequence_number: {packet.sequence_number}")
    print(f"  total_data: {packet.total_data.hex()}")
    print(f"  is_complete: {packet.is_complete}")
    print(f"  has_enough_data: {packet.has_enough_data}")
    print()

    print("  解析响应:")
    print(f"    total_data[6] (role/type): {packet.total_data[6]}")
    print(f"    total_data[7] (device_type): {packet.total_data[7]}")
    print(f"    total_data[8] (sw_major): {packet.total_data[8]}")
    print(f"    total_data[9] (sw_minor): {packet.total_data[9]}")
    print(f"    total_data[10] (sw_rev): {packet.total_data[10]}")
    print()


def test_fragmentation():
    """测试分包处理"""
    print("=" * 60)
    print("测试分包处理")
    print("=" * 60)

    packet = SetBolusPacket(10.0, 1)
    packages = packet.encode(1)

    print(f"  大剂量: 10.0U")
    print(f"  包数量: {len(packages)}")

    for i, pkg in enumerate(packages):
        print(f"  包 {i + 1}: {pkg.hex()}")
        print(f"    长度: {pkg[0]}")
        print(f"    命令: {pkg[1]}")
        print(f"    序列: {pkg[2]}")
        print(f"    索引: {pkg[3]}")

    print()


def test_response_build():
    """测试响应构建"""
    print("=" * 60)
    print("测试响应构建")
    print("=" * 60)

    response_data = bytearray()
    response_data.append(0x02)
    response_data.append(0x01)
    response_data.append(0x01)
    response_data.append(0x00)
    response_data.append(0x00)

    header = bytearray([len(response_data) + 6, 0x03, 0x01, 0x00])
    header += (0).to_bytes(2, 'little')
    tmp = bytes(header) + response_data
    crc = crc8_calculate(tmp)
    result = tmp + bytes([crc]) + bytes([0])

    print(f"  响应数据: {response_data.hex()}")
    print(f"  头+数据: {tmp.hex()}")
    print(f"  CRC: {crc:02X}")
    print(f"  最终: {result.hex()}")
    print(f"  长度: {result[0]}")
    print(f"  命令: {result[1]}")
    print(f"  序列: {result[2]}")
    print(f"  索引: {result[3]}")
    print(f"  响应码: {result[4] + (result[5] << 8)}")
    print(f"  数据起始: {result[6]}")
    print()


def test_key_generation():
    """测试密钥生成"""
    print("=" * 60)
    print("测试密钥生成")
    print("=" * 60)

    pump_sn = b'\x28\xd8\x12\x4a'
    key = Crypto.gen_key(pump_sn)

    print(f"  泵序列号: {pump_sn.hex()}")
    print(f"  生成密钥: {key.hex()}")
    print()


if __name__ == '__main__':
    test_crc8()
    test_authorize_request()
    test_authorize_response_decode()
    test_fragmentation()
    test_response_build()
    test_key_generation()

    print("=" * 60)
    print("测试完成")
    print("=" * 60)