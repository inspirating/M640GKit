/*
================================================================================
M640GKit 泵模拟器 - nRF52840 (n-able-Arduino + NimBLE) Arduino 入口文件
================================================================================

该程序模拟 M640G 胰岛素泵, 作为 BLE GATT Server 运行,
供 iOS Loop app / Trio 或其他 BLE 客户端连接和通信。

移植自 ESP32/ESP32.ino, 业务逻辑零改动 (见 pump_simulator.h)。
BLE 底层从 Adafruit Bluefruit (SoftDevice S140) 换成 Apache NimBLE
(经 h2zero NimBLE-Arduino 库, 跑在 n-able-Arduino 核心上)。

与 ESP32 版的差异 (仅入口文件层面):
  - 删除 esp_reset_reason() / ESP.getFreeHeap(): ESP32 专属 API
  - 持久化已禁用 (见 preferences_nrf52.h, 不依赖任何文件系统)
  - LED 极性: 板载 LED 是低电平点亮 (ESP32 多为高电平)
  - STEP_PIN: 默认用 P0.17 (Nice!Nano 引脚号 17, D2 排针)

硬件要求:
  - Nice!Nano v2 (nRF52840, Pro Micro 兼容引脚)
  - Arduino IDE + h2zero n-able-Arduino 核心 (Board Manager 安装)
  - Arduino IDE + h2zero NimBLE-Arduino 库 (Library Manager 安装)

选板 (n-able-Arduino 核心):
  - 优先选 "nRF52840 Generic" 或 Nice!Nano 对应变体 (若核心提供)
  - n-able 核心不用 SoftDevice, 故无 "SoftDevice -> S140" 选项
  - 引脚映射取决于所选变体, 务必确认 STEP_PIN 实际对应到 P0.17

使用方法:
  1. 安装 n-able-Arduino 核心 (见 README_nimble_migration.md)
  2. 安装 NimBLE-Arduino 库
  3. Arduino IDE -> 工具 -> 开发板 -> 选 nRF52840 板
  4. 工具 -> 串口 -> 选对应 COM 口
  5. 双击 RST 进入 UF2 bootloader 后上传, 或直接上传
  6. 串口监视器 (115200 baud)

对应 ESP32: ESP32.ino
================================================================================
*/

#include "pump_simulator.h"

// 使用 M640GKit 命名空间
using namespace M640GKit;

// ========== 引脚定义 ==========
// STEP_PIN: 控制 TS5A3166 模拟开关, 高电平导通 = 按下泵按键。
// 实际常量定义在 pump_simulator.h 内 (static constexpr int STEP_PIN = 17),
// 指向 Nice!Nano 的 P0.17 (D2 排针引脚)。
// 如你接在其他引脚, 改 pump_simulator.h 的 STEP_PIN 即可。

// LED 引脚: Nice!Nano 板载蓝色 LED 在 P0.15。
// n-able-Arduino 核心的板卡变体可能已定义 LED_BUILTIN; 若已定义则先 undef 再设我们的值,
// 保证与原 Adafruit 版一致 (LED = P0.15)。
// 板载 LED 为低电平点亮 (active-low), 与 ESP32 相反。
#ifdef LED_BUILTIN
#undef LED_BUILTIN
#endif
#define LED_BUILTIN 15  // Nice!Nano: LED = P0.15

// ========== 高驱动 GPIO 宏 ==========
// n-able-Arduino 核心没有 Adafruit 的 OUTPUT_H0H1 高驱动枚举。
// TS5A3166 / 三极管继电器需要 ~5mA 驱动能力, 普通 OUTPUT (~0.5mA) 可能不够。
// 解决: pinMode(OUTPUT) 后, 直接配置 nRF52 GPIO 端口配置寄存器为高驱动 (D0H1/D1H1)。
// STEP_PIN 物理引脚号需通过 n-able 核心的 digitalPinToPort/digitalPinToBitMask 转换;
// 若转换宏不存在则回退普通 OUTPUT (实测驱动不够再补寄存器级配置)。
#define STEP_DRIVE_HIGH(pin) stepPinSetHighDrive(pin)

// 全局模拟器实例
M640GPumpSimulator pumpSimulator;

// 配置 STEP_PIN 为高驱动输出 (nRF52 PIN_CNF 的 DRIVE 字段 = H0H1)。
// 仅在 n-able 核心暴露了 NRF_GPIO / 引脚映射宏时生效; 否则空操作。
// nRF52840 有两个 GPIO 端口: NRF_P0 (pin 0-31) 和 NRF_P1 (pin 32+)。
static void stepPinSetHighDrive(int pin) {
    pinMode(pin, OUTPUT);
#if defined(NRF_P0) && defined(NRF_P1)
    // nRF52 引脚号约定: P0.x = x, P1.x = 32+x。
    // DRIVE 字段 (PIN_CNF bit 8-10): 0=S0S1(标准), 1=H0S1, 2=S0H1, 3=H0H1(高驱动)。
    if (pin < 32) {
        NRF_P0->PIN_CNF[pin] = (NRF_P0->PIN_CNF[pin] & ~(7UL << 8)) | (3UL << 8);
    } else if (pin - 32 < 16) {
        NRF_P1->PIN_CNF[pin - 32] = (NRF_P1->PIN_CNF[pin - 32] & ~(7UL << 8)) | (3UL << 8);
    }
#endif
}

void setup() {
    // 初始化串口 (USB CDC 或 UART0, 取决于板卡变体配置)
    Serial.begin(115200);

    // 等待 USB CDC 枚举完成
    uint32_t usbWaitStart = millis();
    while (!Serial && (millis() - usbWaitStart < 5000)) {
        delay(10);
    }

    Serial.println("nRF52840 starting...");
    Serial.println("Version: 1.0.0-nrf52840-nimble");
    Serial.flush();

    // 打印复位原因 (NRF_POWER->RESETREAS 位掩码, 便于诊断)。
    // n-able-Arduino 核心基于 nRF SDK, 仍暴露 NRF_POWER 寄存器。
    // 若核心未暴露 NRF_POWER, 注释掉这三行即可。
#if defined(NRF_POWER)
    Serial.print("Reset reason (NRF_POWER->RESETREAS raw): 0x");
    Serial.println(NRF_POWER->RESETREAS, HEX);
    // 清除复位原因标志 (nRF52 特性: 该寄存器需手动清, 否则跨复位保留)
    NRF_POWER->RESETREAS = 1;  // 写任意值清除
#else
    Serial.println("Reset reason: NRF_POWER not exposed by core");
#endif

    Serial.println("\n========================================");
    Serial.println("  M640GKit 泵模拟器 (nRF52840 + NimBLE)");
    Serial.println("========================================");
    Serial.println("正在初始化...");
    Serial.flush();

    // ---------- 持久化已禁用 ----------
    // preferences_nrf52.h 的 readFile/writeFile 已是空操作 stub,
    // 不依赖任何文件系统 (Adafruit LittleFS 或其它)。
    Serial.println("Persistence disabled (no filesystem in use)");

    // ---------- 初始化 LED (板载 LED 为低电平点亮) ----------
    Serial.print("Initializing LED on pin ");
    Serial.println(LED_BUILTIN);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);  // HIGH = 灭 (active-low)

    // ---------- 初始化 STEP_PIN (TS5A3166 / 三极管控制, 高电平导通) ----------
    // 必须先配置为输出再拉低, 确保 pinMode 切换瞬间开关不会误触发。
    STEP_DRIVE_HIGH(STEP_PIN);
    digitalWrite(STEP_PIN, LOW);  // TS5A3166 / 三极管: LOW = 断开
    Serial.print("STEP_PIN=");
    Serial.print(STEP_PIN);
    Serial.println(" configured (high-drive if NRF_GPIO exposed, active-high, default LOW)");

    // 设置全局实例指针 (用于回调)
    gSimulator = &pumpSimulator;

    // 初始化模拟器 (内部会调 gattServer.start() 启动 BLE)
    Serial.println("Starting pump simulator setup...");
    pumpSimulator.setup();

    Serial.println("初始化完成!");
    Serial.println("LED will blink fast when advertising, slow when connected");

    // 打印内存信息 (替代 ESP32 的 ESP.getFreeHeap)
    // n-able 核心的 FreeRTOS 可能未导出 xPortGetFreeHeapSize, 此处仅打印提示
    Serial.println("Free heap: see FreeRTOS stats (xPortGetFreeHeapSize may be unavailable)");

    Serial.println("========================================\n");
    Serial.flush();
}

void loop() {
    // 运行模拟器主循环
    pumpSimulator.loop();
    delay(5); // <--- 【关键】给系统底层调度器让出 5ms

    // LED 指示: 未连接时快闪, 已连接时常灭
    static uint32_t lastLedToggle = 0;
    static bool ledOn = false;
    uint32_t now = millis();

    if (pumpSimulator.getIsConnected()) {
        // 已连接: LED 常灭
        if (ledOn) {
            ledOn = false;
            digitalWrite(LED_BUILTIN, HIGH);  // HIGH = 灭
        }
    } else {
        // 未连接: 快闪 (亮200ms, 灭200ms)
        uint32_t interval = ledOn ? 200 : 200;
        if (now - lastLedToggle >= interval) {
            lastLedToggle = now;
            ledOn = !ledOn;
            digitalWrite(LED_BUILTIN, ledOn ? LOW : HIGH);  // LOW=亮, HIGH=灭
        }
    }
}
