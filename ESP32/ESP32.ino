/*
================================================================================
M640GKit ESP32 泵模拟器 - Arduino 入口文件
================================================================================

该程序模拟 M640G 胰岛素泵, 作为 BLE GATT Server 运行,
供 iOS Loop app 或其他 BLE 客户端连接和通信

硬件要求:
- ESP32 开发板 (推荐 ESP32-WROOM-32)
- Arduino ESP32 核心库 (内置 BLE, 见 pump_manager/gatt_server.h)

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
#include "esp_wifi.h"
#include "esp_pm.h"
#include <WiFi.h>

// 使用 M640GKit 命名空间
using namespace M640GKit;

// ESP32-C3 LED 引脚定义 (不同开发板可能不同)
// 常见: GPIO8, GPIO2, GPIO3
#ifndef LED_BUILTIN
  #define LED_BUILTIN 8
#endif

// 全局模拟器实例
M640GPumpSimulator pumpSimulator;

void power_manager_init() {
    // 1. 关闭 WiFi 射频并卸载驱动
    WiFi.mode(WIFI_OFF);

    // 强制关闭所有射频并卸载驱动
    esp_wifi_stop();
    esp_wifi_deinit();

    // 2. 动态调频 (DVFS): 空闲时自动降至 10MHz, BLE/中断时自动升至 80MHz
    //    固定频率不能低于 80MHz (BLE 不稳定), 但 DVFS 可以安全地动态降频
    esp_pm_config_t pm_config = {
        .max_freq_mhz = 80,
        .min_freq_mhz = 10,
        .light_sleep_enable = false  // light sleep 需要 menuconfig 启用 TICKLESS_IDLE
    };
    esp_pm_configure(&pm_config);

    // 4. 降低串口波特率减少 UART 模块功耗 (可选, 调试时注释掉)
    // Serial.updateBaudRate(9600);
}

void setup() {
    power_manager_init();

    // 初始化串口
    Serial.begin(115200);
    
    // 给串口一些时间初始化
    delay(1000);

    Serial.println("ESP32 starting...");
    Serial.println("Version: 1.0.0");
    Serial.flush();
    
    // 检查复位原因
    esp_reset_reason_t reset_reason = esp_reset_reason();
    Serial.print("Reset reason: ");
    Serial.println(reset_reason);

    Serial.println("\n========================================");
    Serial.println("  M640GKit ESP32 泵模拟器");
    Serial.println("========================================");
    Serial.println("正在初始化...");
    Serial.flush();

    // 初始化 LED (ESP32-C3 常见引脚: 8, 2, 3)
    Serial.print("Initializing LED on pin ");
    Serial.println(LED_BUILTIN);
    
    digitalWrite(LED_BUILTIN, LOW);
    pinMode(LED_BUILTIN, OUTPUT);

    // 初始化 GPIO6 (STEP_PIN) 作为 TS5A3166 模拟开关控制, 高电平导通
    // 必须先 pinMode(OUTPUT) 再 digitalWrite(LOW),
    // 确保 pinMode 切换瞬间输出寄存器默认 LOW, 开关不会误触发
    pinMode(STEP_PIN, OUTPUT);
    digitalWrite(STEP_PIN, LOW); // TS5A3166: LOW = 断开
    
    Serial.println("GPIO6(STEP_PIN) initialized as TS5A3166 control (active-high, default LOW)");

    // 设置全局实例指针 (用于回调)
    gSimulator = &pumpSimulator;

    // 初始化模拟器
    Serial.println("Starting pump simulator setup...");
    pumpSimulator.setup();

    Serial.println("初始化完成!");
    Serial.println("LED will blink fast when advertising, slow when connected");
    
    // 打印内存信息
    Serial.print("Free heap: ");
    Serial.print(ESP.getFreeHeap());
    Serial.println(" bytes");
    
    Serial.println("========================================\n");
    Serial.flush();
}

void loop() {
    // 运行模拟器主循环
    pumpSimulator.loop();

    // LED 心跳指示器
    static uint32_t lastLedToggle = 0;
    static bool ledState = false;
    uint32_t now = millis();
    
    if (pumpSimulator.getIsConnected()) {
        // 已连接: LED 常灭
        if (ledState) {
            ledState = false;
            digitalWrite(LED_BUILTIN, LOW);
        }
    } else {
        // 未连接: 短脉冲闪烁 (亮50ms/灭2s)
        uint32_t interval = ledState ? 50 : 1000;
        if (now - lastLedToggle >= interval) {
            lastLedToggle = now;
            ledState = !ledState;
            digitalWrite(LED_BUILTIN, ledState ? HIGH : LOW);
        }
    }

    // 让出 CPU 进入低功耗: FreeRTOS idle → light sleep (如果已启用)
    delay(2);
}

// 如果你的ESP32-C3 LED不亮，尝试修改 LED_BUILTIN 为以下值之一:
// #define LED_BUILTIN 2  // 某些开发板使用GPIO2
// #define LED_BUILTIN 3  // 某些开发板使用GPIO3
// #define LED_BUILTIN 8  // 某些开发板使用GPIO8 (默认)
