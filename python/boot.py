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
