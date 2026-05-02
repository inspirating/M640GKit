import sys
sys.path.insert(0, '.')
from encryption.crypto import Crypto

sn = bytes([0x28, 0xD8, 0x12, 0x4A])
reversed_sn = bytes(reversed(sn))
print(f'pumpSN: {sn.hex()}')
print(f'pumpSN.reversed(): {reversed_sn.hex()}')

int_val = int.from_bytes(reversed_sn, 'little')
print(f'As int (LE): {int_val}')

result = Crypto.simple_decrypt(reversed_sn)
result_int = int.from_bytes(result, 'little')
print(f'simpleDecrypt result: {result.hex()}')
print(f'simpleDecrypt result as int: {result_int}')

if 126_000_000 <= result_int < 126_999_999:
    print('Model: MD0201')
elif 127_000_000 <= result_int < 127_999_999:
    print('Model: MD5201')
elif 128_000_000 <= result_int < 128_999_999:
    print('Model: MD8201')
elif 130_000_000 <= result_int < 130_999_999:
    print('Model: MD0202')
elif 131_000_000 <= result_int < 131_999_999:
    print('Model: MD5202')
elif 148_000_000 <= result_int < 148_999_999:
    print('Model: MD8301')
else:
    print(f'Model: UNKNOWN (falls back to MD8301)')
