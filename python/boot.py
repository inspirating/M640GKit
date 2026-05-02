"""
================================================================================
M640GKit ESP32 启动脚本 (Arduino setup 等价物)
================================================================================

该文件在 ESP32 启动时自动执行, 负责初始化泵模拟器

MicroPython 启动顺序:
1. 上电/复位
2. 执行 boot.py (此文件)
3. 执行 main.py (loop 循环)
================================================================================
"""

from pump_simulator import simulator

# Arduino setup() 等价物 - 只执行一次
try:
    simulator.setup()
except Exception as e:
    print(f"[ERROR] 初始化失败: {e}")
    # 初始化失败时, 让 LED 快速闪烁提示
    try:
        import machine
        led = machine.Pin(2, machine.Pin.OUT)
        import utime
        while True:
            led.value(1)
            utime.sleep_ms(100)
            led.value(0)
            utime.sleep_ms(100)
    except:
        pass

# 解决方案：三步走，确保连接成功
# 第一步：确认并刷入正确的 MicroPython 固件
# 这是最关键的一步。你需要为 ESP32-C3 刷入官方或社区维护的 MicroPython 固件。
# 下载固件：
# 访问 MicroPython 官方网站的下载页面。
# 找到 ESP32 板块，下载适用于 ESP32-C3 的 .bin 固件文件。文件名通常类似 esp32c3-xxxxxx.bin。
# 使用 esptool 刷入固件：
# 打开命令行工具（Windows 的 CMD 或 PowerShell）。
# 安装 esptool（如果还没安装）：
# bash

# pip install esptool

# # 1. 擦除整个芯片（可选，但推荐）
# esptool.py --port COM3 erase_flash

# # 2. 烧录固件到地址 0x0000
# esptool.py --port COM3 --baud 921600 write_flash 0x0000 你的固件文件名.bin