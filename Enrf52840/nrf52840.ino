/*
================================================================================
M640GKit nRF52840 泵模拟器 - Arduino 入口文件
================================================================================

该程序模拟 M640G 胰岛素泵, 作为 BLE GATT Server 运行,
供 iOS Loop app 或其他 BLE 客户端连接和通信

硬件要求:
- nRF52840 开发板 (如 Seeed XIAO nRF52840, Adafruit Feather nRF52840, Arduino Nano 33 BLE 等)
- Arduino nRF52 核心库 (需支持 ArduinoBLE)

使用方法:
1. 在 Arduino IDE 中打开此文件
2. 选择对应的 nRF52840 开发板
3. 选择正确的串口
4. 点击上传
5. 打开串口监视器查看日志 (115200 baud)
================================================================================
*/

#include "pump_simulator.h"

using namespace M640GKit;

// LED 引脚定义 (不同 nRF52840 开发板可能不同)
// Seeed XIAO nRF52840 Sense: LED_BUILTIN = LED_BUILTIN (GPIO 13)
// Adafruit Feather nRF52840: LED_BUILTIN = LED_BUILTIN
// Arduino Nano 33 BLE: LED_BUILTIN = LED_BUILTIN
#ifndef LED_BUILTIN
  #define LED_BUILTIN 13
#endif

// 全局模拟器实例
M640GPumpSimulator pumpSimulator;

// 全局指针定义 (用于回调)
M640GPumpSimulator* gSimulator = nullptr;

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("nRF52840 starting...");
    Serial.println("Version: 1.0.0");
    Serial.flush();

    Serial.println("\n========================================");
    Serial.println("  M640GKit nRF52840 泵模拟器");
    Serial.println("========================================");
    Serial.println("Initializing...");

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    gSimulator = &pumpSimulator;

    Serial.println("Starting pump simulator setup...");
    pumpSimulator.setup();

    Serial.println("初始化完成!");
    Serial.println("========================================\n");
}

void loop() {
    pumpSimulator.loop();

    static uint32_t lastLedToggle = 0;
    static bool ledState = false;
    uint32_t now = millis();
    uint32_t interval = pumpSimulator.getIsConnected() ? 1000 : 200;

    if (now - lastLedToggle >= interval) {
        lastLedToggle = now;
        ledState = !ledState;
        digitalWrite(LED_BUILTIN, ledState ? HIGH : LOW);
    }
}

/*
LED 行为指示:
- 广告中: 快速闪烁 (200ms)
- 已连接: 慢速闪烁 (1000ms)
- 未广告: 熄灭

注意: 如果 nRF52840 上的 LED_BUILTIN 定义不正确, 请修改上面的定义
常见 LED 引脚:
- Seeed XIAO nRF52840: GPIO 13 (蓝色 LED) / GPIO 14 (黄色 LED)
- Adafruit Feather nRF52840: GPIO 7 (蓝色 LED)
- Arduino Nano 33 BLE: GPIO 13
*/