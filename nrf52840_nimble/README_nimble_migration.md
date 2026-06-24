# M640GKit 泵模拟器 — nRF52840 NimBLE 迁移版

本目录把原 Adafruit Bluefruit (SoftDevice S140) 版本迁移到 **Apache NimBLE**
(经 [h2zero NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) 库,
跑在 [h2zero n-able-Arduino](https://github.com/h2zero/n-able-Arduino) 核心上)。

底层 BLE 栈从 **Nordic 闭源 SoftDevice** 换成 **开源 Apache NimBLE**,
完全不依赖 SoftDevice, 所有 `sd_ble_gap_*` / `ble_evt_t` / `BLE_GAP_EVT_*` 调用消失。

业务逻辑 (`pump_simulator.h` 的状态机/队列/carryOver/时间模型) **零改动**。

---

## 1. 为什么迁移到 NimBLE

| | Adafruit Bluefruit (旧) | n-able-Arduino + NimBLE (新) |
|---|---|---|
| BLE 协议栈 | Nordic SoftDevice S140 (**闭源**) | Apache NimBLE (**开源**) |
| Arduino 核心 | Adafruit nRF52 | h2zero n-able-Arduino |
| 配对/安全 | 需手动拒绝 SEC_REQUEST 防 0x8 断开 | 默认不配对, iOS 旧 bond 问题天然不存在 |
| MTU 协商 | onConnect 里 requestMtuExchange | init 时 setMTU, 自动协商 |
| 回调模型 | 全局静态函数 + setEventCallback | 回调类继承 (Callbacks) |

---

## 2. 环境搭建

### 2.1 安装 n-able-Arduino 核心
1. Arduino IDE → 文件 → 首选项 → 附加开发板管理器网址, 加:
   ```
   https://github.com/h2zero/n-able-Arduino/raw/master/package_n-able-Arduino_index.json
   ```
   (若该 URL 失效, 到 https://github.com/h2zero/n-able-Arduino 查 README 获取最新 JSON 地址)
2. 工具 → 开发板 → 开发板管理器 → 搜索 `n-able` → 安装。
3. 选板: **"nRF52840 Generic"** 或 Nice!Nano 对应变体 (若核心提供)。

### 2.2 安装 NimBLE-Arduino 库
1. Arduino IDE → 工具 → 管理库 → 搜索 `NimBLE-Arduino` (作者 h2zero) → 安装最新版。
2. 该库随 n-able-Arduino 核心一起在 nRF52840 上提供 NimBLE 协议栈。

### 2.3 板卡配置
- 开发板: nRF52840 板 (Generic / Nice!Nano 变体 / PCA10056)
- 串口: 选对应 COM 口 (USB CDC)
- **注意**: n-able 核心不用 SoftDevice, 故没有 "SoftDevice -> S140" 选项。

---

## 3. 改动文件清单

| 文件 | 改动 | 说明 |
|------|------|------|
| `gatt_server.h` | **完全重写** | Adafruit Bluefruit → NimBLE, 公共 API 不变 |
| `preferences_nrf52.h` | 小改 | 删 `Adafruit_LittleFS`/`InternalFileSystem` 头依赖 (持久化本就是空操作 stub) |
| `nrf52840.ino` | 中改 | `OUTPUT_H0H1` → 寄存器级高驱动; `LED_BUILTIN` undef 重定义; `NRF_POWER` 条件编译 |
| `pump_simulator.h` | 极小改 | `<rtos.h>` → `<FreeRTOS.h>`+`<task.h>`+`<semphr.h>`; 1 处 `OUTPUT_H0H1` → `OUTPUT` |
| `README_nimble_migration.md` | 新建 | 本文件 |

**未改动 (纯软件, 无平台依赖):**
`*_packet.h` (9 个) / `crypto.h` / `crc8.h` / `enums.h` / `connection_tracker.h`

---

## 4. 关键技术映射

| Adafruit Bluefruit | NimBLE-Arduino |
|---|---|
| `Bluefruit.begin(1,0)` | `NimBLEDevice::init("MT")` |
| `Bluefruit.configPrphConn(247,...)` | `NimBLEDevice::setMTU(247)` |
| `Bluefruit.setTxPower(8)` | `NimBLEDevice::setPower(8)` |
| `Bluefruit.setName("MT")` | `NimBLEDevice::init("MT")` |
| `BLEService` / `BLECharacteristic` | `NimBLEService` / `NimBLECharacteristic` |
| `CHR_PROPS_WRITE\|WRITE_WO_RESP\|NOTIFY` | `NIMBLE_PROPERTY::WRITE\|WRITE_NR\|NOTIFY` |
| `setPermission(SECMODE_OPEN,...)` | 属性 flags 自带 (默认 OPEN) |
| `chr->notify(data,len)` | `chr->setValue(data,len)` + `chr->notify()` (v2.x) |
| `Bluefruit.Advertising.addData(...)` | `NimBLEAdvertisementData::setManufacturerData` |
| `Bluefruit.ScanResponse.addUuid(...)` | `advData.addServiceUUID(...)` (NimBLE 自动溢出到 scan response) |
| `Bluefruit.setEventCallback(ble_evt_t*)` | `NimBLEServerCallbacks::onConnect/onDisconnect` |
| `sd_ble_gap_sec_params_reply(...PAIRING_NOT_SUPP...)` | NimBLE 默认不配对, 无需处理 |
| `NRF_FICR->DEVICEADDR` 读 MAC | 仍用 `NRF_FICR` (n-able 核心暴露) + `setOwnAddrType` |

---

## 5. 已知风险与对策 (上板验证重点)

### 风险 1: n-able-Arduino nRF52840 板卡变体不完整 【高】
- Nice!Nano 可能无专用 variant → 用 "nRF52840 Generic"。
- 若烧录不上, 退而用 PCA10056 DK 的变体。

### 风险 2: GPIO 高驱动 (OUTPUT_H0H1 消失) 【中】
- n-able 核心无 `OUTPUT_H0H1` 枚举。
- `nrf52840.ino` 里 `stepPinSetHighDrive()` 直接写 `NRF_P0/P1->PIN_CNF[].DRIVE = 3` (H0H1)。
- **验证点**: set bolus 时继电器是否吸合。若不吸合, 检查 `STEP_PIN` 引脚号在 n-able
  核心的映射是否真的对应到 P0.17 (Generic 变体可能不是 1:1)。

### 风险 3: NimBLE MAC 设置 【中】
- 代码先 `setOwnAddrType(BLE_OWN_ADDR_RANDOM)`, 再尝试用 FICR 派生地址。
- 若 `NimBLEDevice::setAddr` 在当前库版本签名不同, 会回退到 NimBLE 默认地址。
- **后果**: Trio 首次需重新配对一次 (与原 nRF52840 版体验一致)。

### 风险 4: NimBLE 回调签名版本差异 【低】
- NimBLE 不同版本 `onConnect` 签名不同 (有的带 `NimBLEConnInfo&`, 有的不带)。
- 代码用 `#if defined(NIMBLE_ARDUINO_HAS_ONCONNECT_CONNINFO)` 兼容两种。
- 若你的库版本两个签名都没有, 编译报错时按错误信息调整 `GattServerCallbacks` 的 override。

### 风险 5: `std::__throw_length_error` 重复定义 【低】
- `nrf52840.ino` 定义了 `std::__throw_length_error` stub (因 `-fno-exceptions`)。
- 若 n-able 核心已提供该符号, 会重复定义报错 → 删除 `nrf52840.ino` 里那段 `namespace std {...}`。

---

## 6. 验证阶段

1. **编译通过** — n-able 核心 + NimBLE 库全部 .h/.ino 无错误
2. **BLE 广播** — nRF Connect 扫到 `MT`, 厂商数据 = `59 6A 65 D1 79 98 01 01`
3. **Trio 配对** — 首次重新配对 (MAC 变了), 串口见 `[BLE] CLIENT CONNECTED!`
4. **协议流程** — AUTH/SYNCHRONIZE/SUBSCRIBE/PRIME/ACTIVATE 走通
5. **GPIO 输注** — set bolus 驱动继电器 (验证风险 2)

---

## 7. 与旧 SoftDevice 版对照

代码 diff 时, 业务逻辑 (`pump_simulator.h` 的状态机/队列/时间模型) 应**零差异**。
平台相关改动仅:
- `gatt_server.h`: 整文件重写 (NimBLE)
- `preferences_nrf52.h`: 删 2 个 Adafruit 头 + namespace
- `pump_simulator.h`: FreeRTOS include 头 + 1 处 OUTPUT_H0H1
- `nrf52840.ino`: LED/STEP/NRF_POWER 适配
