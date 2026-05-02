"""
================================================================================
M640GKit ESP32 主循环 (Arduino loop 等价物)
================================================================================

该文件在 boot.py 之后执行, 负责运行泵模拟器的主循环

MicroPython 启动顺序:
1. 上电/复位
2. 执行 boot.py (setup)
3. 执行 main.py (loop) <- 此文件
================================================================================
"""

import utime
from pump_simulator import simulator

# Arduino loop() 等价物 - 无限循环
while True:
    try:
        simulator.loop()
    except Exception as e:
        print(f"[ERROR] 主循环异常: {e}")
    
    # 短暂休眠, 让出 CPU 给其他任务
    # 注意: simulator.loop() 内部已经做了时间检查,
    # 所以这里可以 sleep 较长时间, 不会错过更新
    utime.sleep_ms(10)
