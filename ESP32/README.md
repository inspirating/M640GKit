# M640GKit C++ 版本

M640G 胰岛素泵模拟器的 C++ 实现, 专为 ESP32 Arduino 开发环境设计。

## 文件结构

```
c_plus/
├── M640GKit.ino              # Arduino 入口文件 (setup/loop)
├── enums.h                   # 枚举和常量定义
├── pump_simulator.h          # 泵模拟器主类
├── 
│   ├── crc8.h               # CRC8 校验模块
│   └── crypto.h             # 加密解密模块
├── packets/
│   ├── base_packet.h        # 数据包基类
│   ├── authorize_packet.h   # 认证数据包
│   ├── synchronize_packet.h # 同步数据包
│   ├── bolus_packet.h       # 大剂量数据包
│   ├── basal_packet.h       # 基础率数据包
│   ├── time_packet.h        # 时间数据包
│   ├── pump_control_packet.h# 泵控制数据包
│   ├── subscribe_packet.h   # 订阅数据包
│   └── misc_packet.h        # 杂项数据包
├── pump_manager/
│   ├── gatt_server.h        # GATT 服务器
│   └── connection_tracker.h # 连接追踪器
└── tests/
    ├── test_main.h          # 测试主入口
    ├── test_crypto.h        # 加密模块测试
    └── test_packets.h       # 数据包测试
```

## 硬件要求

- ESP32 开发板 (推荐 ESP32-WROOM-32)
- USB 数据线

## 依赖库

- **Arduino ESP32 核心库**: 在 Arduino IDE 中通过 "开发板管理器" 添加 ESP32 支持
- **ESP32 Arduino BLE** (`BLEDevice.h` / `gatt_server.h` 当前实现): 使用开发板自带的 BLE 支持即可。
  - 若你自行改用 **NimBLE-Arduino**, 需同步修改 `pump_manager/gatt_server.h` 中的包含与 API, README 以仓库内实际代码为准。

## 使用方法

### 1. 安装 Arduino IDE

下载并安装 [Arduino IDE](https://www.arduino.cc/en/software)

### 2. 添加 ESP32 支持

1. 打开 Arduino IDE
2. 文件 -> 首选项
3. 在 "附加开发板管理器网址" 中添加:
   ```
   https://dl.espressif.com/dl/package_esp32_index.json
   ```
4. 工具 -> 开发板 -> 开发板管理器
5. 搜索 "ESP32" 并安装

### 3. 上传程序

1. 用 USB 线连接 ESP32 到电脑
2. 在 Arduino IDE 中选择:
   - 工具 -> 开发板 -> ESP32 Arduino -> ESP32 Dev Module
   - 工具 -> 端口 -> 选择正确的 COM 端口
3. 打开 `M640GKit.ino` 文件
4. 点击上传按钮 (或按 Ctrl+U)

### 4. 查看日志

1. 工具 -> 串口监视器
2. 设置波特率为 115200
3. 查看模拟器运行日志

## 功能特性

- **BLE GATT Server**: 模拟 M640G 泵设备, 支持 iOS Loop app 连接
- **完整命令支持**: 支持所有泵操作命令 (大剂量、基础率、暂停/恢复等)
- **状态同步**: 定期发送泵状态通知
- **连接管理**: 自动重连、连接超时检测
- **数据包处理**: 支持分包传输和重组
- **CRC8 校验**: 确保数据完整性
- **加密认证**: 支持泵序列号认证

## 与 Python 版本的对应关系

| Python 文件 | C++ 文件 | 说明 |
|------------|---------|------|
| `enums.py` | `enums.h` | 枚举和常量 |
| `crc8.py` | `crc8.h` | CRC8 校验 |
| `crypto.py` | `crypto.h` | 加密解密 |
| `packets/base_packet.py` | `packets/base_packet.h` | 数据包基类 |
| `packets/*.py` | `packets/*.h` | 各类数据包 |
| `pump_manager/gatt_server.py` | `pump_manager/gatt_server.h` | GATT 服务器 |
| `pump_manager/ble_manager.py` | `pump_manager/connection_tracker.h` | 连接管理 |
| `pump_simulator.py` | `pump_simulator.h` | 泵模拟器 |
| `main.py` + `boot.py` | `M640GKit.ino` | 入口文件 |

## 测试

在 `setup()` 函数中添加以下代码运行测试:

```cpp
#include "tests/test_main.h"

void setup() {
    // ... 原有代码 ...
    M640GKit::TestMain::runAll();
}
```

## 注意事项

1. **内存使用**: ESP32 有 520KB SRAM, 注意避免内存泄漏
2. **BLE 稳定性**: 若连接不稳, 可尝试降低串口日志频率、检查电源; 改用 NimBLE 时需同步改写 `gatt_server.h`。
3. **电源**: 确保 ESP32 供电稳定, 避免 BLE 连接中断
4. **散热**: 长时间运行注意散热

## 故障排除

### 无法编译
- 确认已安装 ESP32 开发板支持（当前工程使用自带 BLE 栈, 无需单独安装 NimBLE）
- 检查所有文件是否在正确的目录中

### BLE 无法连接
- 确认 ESP32 已正确供电
- 检查串口日志中的错误信息
- 确认手机蓝牙已开启
- 尝试重启 ESP32

### 连接不稳定
- 检查电源是否稳定
- 减少日志输出频率
- 调整连接超时时间

## 已知问题与修复

### 1. Delete Pump 后显示 "patch error" 而非 "新增泵" 图标

**现象**: 点击 "Delete Pump" 后, Trio 界面左上角显示 "patch error" 图标, 而非预期的 "新增泵" 图标。

**根因**: "Delete Pump" 流程调用 `notifyDelegateOfDeactivation` 移除 pump manager, 但 BLE 连接未断开。Swift 端 `didDisconnectPeripheral` 回调中自动调用 `ensureConnected` 尝试重连, ESP32 断开后立即重新广播, Swift 自动重连后发现 ESP32 仍处于 ACTIVE/STOPPED 状态, 但 pump manager 已被删除, 状态不一致导致 Trio 显示 "patch error"。

**修复**: 在 `handleStopPatchRequest` 中:
- 重置所有运行时状态 (reservoir, activeInsulin, hourlyDelivered, dailyDelivered 等)
- 清除 NVS 持久化数据
- 设置 `pendingBleDisconnect = true`, 在主循环 `update()` 中延迟执行:
  - 将 `patchState` 重置为 `FILLED` (初始状态)
  - 将 `simulatorState` 重置为 `INITIALIZING`
  - 调用 `gattServer.disconnectAll()` 主动断开 BLE 连接
- BLE 断开后 ESP32 重新广播, Swift 重连时看到一个全新的未激活设备
- 重置 `patchStartTime = 0` 和 `totalElapsedTime = 0`, 确保内存中的值也被清空

### 2. ESP32 重启后 "检测到时间变更"

**现象**: ESP32 重启后, 点击 "Sync Patch Data" 触发 "Time Change Detected" 警告。先手动同步时间后, 再 Sync Patch Data 则不会触发。

**根因**: Swift 端时间变更检测逻辑为 `abs(pumpTimeSyncedAt - pumpTime) > 15秒`。`pumpTime` 来自 ESP32 的 `GetTimePacket` 响应 = `patchStartTime + totalElapsedTime`。ESP32 重启后 `totalElapsedTime` 重置为 0 (未持久化), 导致泵时间 = `patchStartTime` (原始激活时间), 与当前时间差距巨大, 触发时间变更检测。

**修复**: 将 `totalElapsedTime` 持久化到 NVS:
- `setup()` 中从 NVS 恢复 `totalElapsedTime`
- `update()` 中每 60 秒定期保存 `totalElapsedTime` 到 NVS
- 状态变更时 (`patchStateDirty`) 同时保存 `elapsedTime`
- `handleStopPatchRequest` 中清除 NVS 中的 `elapsedTime`

### 3. 蓝牙连接-断开循环

**现象**: ESP32 与 Swift 之间蓝牙反复连接-断开, 无法稳定通信。

**根因**: 之前在 BLE 回调中直接执行 `Preferences` NVS 写入操作 (flash 写入), 会阻塞 BLE 栈的任务, 可能导致 BLE 栈时序超时或栈溢出, 引发 ESP32 崩溃重启, 造成连接-断开循环。

**修复**: 将所有 NVS 写入操作从 BLE 回调移到主循环 `update()` 中延迟执行:
- `setPatchState()` 不再直接写 NVS, 改为设置 `patchStateDirty = true` 标志
- `update()` 每次循环检查 `patchStateDirty`, 为 true 时执行 NVS 写入
- `handleSetTimeRequest()` 和 `handleActivateRequest()` 同样改为延迟写入

### 4. 小剂量进度条不更新

**现象**: 输入 0.1U 或 0.2U 等小剂量 bolus 时, 进度条来不及反应, 始终为 0 后很快消失。

**根因**: Swift 端进度条使用 2 秒定时器更新, 而小剂量在 1-2 秒内就完成了, 进度条来不及显示任何进度。

**修复**: 在 `updateBolusDelivery()` 中:
- 小剂量 (≤0.2U) 时, 将输送速度降低为 5 秒内完成
- 每 10ms 发送一次进度通知, 确保 Swift 端能及时更新进度条

## 许可证

MIT License
