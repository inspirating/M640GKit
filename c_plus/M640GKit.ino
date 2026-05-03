/*
================================================================================
M640GKit ESP32 泵模拟器 - Arduino 入口文件
================================================================================

该程序模拟 M640G 胰岛素泵, 作为 BLE GATT Server 运行,
供 iOS Loop app 或其他 BLE 客户端连接和通信

硬件要求:
- ESP32 开发板 (推荐 ESP32-WROOM-32)
- Arduino ESP32 核心库
- NimBLE-Arduino 库 (轻量级 BLE 库)

使用方法:
1. 在 Arduino IDE 中打开此文件
2. 选择 "工具" -> "开发板" -> "ESP32 Dev Module"
3. 选择正确的串口
4. 点击上传
5. 打开串口监视器查看日志 (115200 baud)

对应 Python: main.py + boot.py
================================================================================
*/

#include "pump_simulator.h"

// 使用 M640GKit 命名空间
using namespace M640GKit;

// 全局模拟器实例
M640GPumpSimulator pumpSimulator;

void setup() {
    // 初始化串口
    Serial.begin(115200);
    while (!Serial) {
        ; // 等待串口连接 (仅用于 Leonardo 等板子)
    }

    Serial.println("\n========================================");
    Serial.println("  M640GKit ESP32 泵模拟器");
    Serial.println("========================================");
    Serial.println("正在初始化...");

    // 设置全局实例指针 (用于回调)
    gSimulator = &pumpSimulator;

    // 初始化模拟器
    pumpSimulator.setup();

    Serial.println("初始化完成!");
    Serial.println("========================================\n");
}

void loop() {
    // 运行模拟器主循环
    pumpSimulator.loop();

    // 其他 Arduino 任务可以在这里添加
    // 例如: 读取传感器、控制 LED 等
}
