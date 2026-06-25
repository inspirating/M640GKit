/*
================================================================================
M640GKit 泵模拟器 - nRF52840 (Adafruit nRF52 + NimBLE) Arduino 入口文件
================================================================================

该程序模拟 M640G 胰岛素泵, 作为 BLE GATT Server 运行,
供 iOS Loop app / Trio 或其他 BLE 客户端连接和通信。

移植自 ESP32/ESP32.ino, 业务逻辑零改动 (见 pump_simulator.h)。
BLE 底层从 ESP32 Arduino BLE 换成 NimBLE-Arduino。

硬件要求:
  - Nice!Nano v2 (nRF52840, Pro Micro 兼容引脚)
  - Arduino IDE + Adafruit nRF52 板卡包

选板 (Adafruit nRF52 核心):
  - 选 "PCA10056 nRF52840 DK" (引脚 1:1 映射, P0.x = x)

使用方法:
  1. Arduino IDE -> 工具 -> 开发板 -> Adafruit nRF52 -> PCA10056
  2. 工具 -> SoftDevice -> S140
  3. 工具 -> 串口 -> 选对应 COM 口
  4. 上传
  5. 串口监视器 (115200 baud)

对应 ESP32: ESP32.ino
================================================================================
*/

#include "pump_simulator.h"

// 使用 M640GKit 命名空间
using namespace M640GKit;

// LED 引脚: Nice!Nano 板载蓝色 LED 在 P0.15, 低电平点亮 (active-low)
#ifdef LED_BUILTIN
#undef LED_BUILTIN
#endif
#define LED_BUILTIN 15

// 全局模拟器实例
M640GPumpSimulator pumpSimulator;

void setup() {
    // 初始化串口
    Serial.begin(115200);
    delay(2000);  // 等 USB CDC 枚举

    Serial.println("nRF52840 starting...");
    Serial.println("Version: 1.0.0-nrf52840-nimble");
    Serial.flush();

    Serial.println("\n========================================");
    Serial.println("  M640GKit 泵模拟器 (nRF52840 + NimBLE)");
    Serial.println("========================================");
    Serial.println("正在初始化...");
    Serial.flush();

    // 持久化已禁用 (preferences_nrf52.h 为空操作 stub)
    Serial.println("Persistence disabled (no filesystem in use)");

    // 初始化 LED (active-low: HIGH=灭, LOW=亮)
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);  // 灭

    // 初始化 STEP_PIN (TS5A3166 模拟开关, 高电平导通)
    pinMode(STEP_PIN, OUTPUT);
    digitalWrite(STEP_PIN, LOW);  // LOW = 断开
    Serial.print("STEP_PIN=");
    Serial.print(STEP_PIN);
    Serial.println(" configured (active-high, default LOW)");

    // 设置全局实例指针 (用于回调)
    gSimulator = &pumpSimulator;

    // 初始化模拟器 (内部会调 gattServer.start() 启动 BLE)
    Serial.println("Starting pump simulator setup...");
    pumpSimulator.setup();

    Serial.println("初始化完成!");
    Serial.println("LED: fast pulse when advertising, off when connected");
    Serial.println("========================================\n");
    Serial.flush();
}

void loop() {
    // 运行模拟器主循环
    pumpSimulator.loop();

    // LED 指示
    static uint32_t lastLedToggle = 0;
    static bool ledState = false;
    uint32_t now = millis();

    if (pumpSimulator.getIsConnected()) {
        // 已连接: LED 常灭
        if (ledState) {
            ledState = false;
            digitalWrite(LED_BUILTIN, HIGH);  // 灭
        }
    } else {
        // 未连接: 短脉冲 (亮50ms/灭2s)
        uint32_t interval = ledState ? 50 : 2000;
        if (now - lastLedToggle >= interval) {
            lastLedToggle = now;
            ledState = !ledState;
            digitalWrite(LED_BUILTIN, ledState ? LOW : HIGH);  // LOW=亮, HIGH=灭
        }
    }

    delay(1);  // 让出 CPU
}
