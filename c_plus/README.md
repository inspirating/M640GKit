# M640GKit C++ 版本

M640G 胰岛素泵模拟器的 C++ 实现, 专为 ESP32 Arduino 开发环境设计。

## 文件结构

```
c_plus/
├── M640GKit.ino              # Arduino 入口文件 (setup/loop)
├── enums.h                   # 枚举和常量定义
├── pump_simulator.h          # 泵模拟器主类
├── encryption/
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
- **NimBLE-Arduino**: 轻量级 BLE 库
  - 在 Arduino IDE 中: 项目 -> 加载库 -> 管理库 -> 搜索 "NimBLE-Arduino" 并安装

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

### 3. 安装 NimBLE-Arduino 库

1. 项目 -> 加载库 -> 管理库
2. 搜索 "NimBLE-Arduino"
3. 点击安装

### 4. 上传程序

1. 用 USB 线连接 ESP32 到电脑
2. 在 Arduino IDE 中选择:
   - 工具 -> 开发板 -> ESP32 Arduino -> ESP32 Dev Module
   - 工具 -> 端口 -> 选择正确的 COM 端口
3. 打开 `M640GKit.ino` 文件
4. 点击上传按钮 (或按 Ctrl+U)

### 5. 查看日志

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
| `encryption/crc8.py` | `encryption/crc8.h` | CRC8 校验 |
| `encryption/crypto.py` | `encryption/crypto.h` | 加密解密 |
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
2. **BLE 稳定性**: NimBLE 比标准 BLE 库更稳定且内存占用更小
3. **电源**: 确保 ESP32 供电稳定, 避免 BLE 连接中断
4. **散热**: 长时间运行注意散热

## 故障排除

### 无法编译
- 确认已安装 ESP32 开发板支持
- 确认已安装 NimBLE-Arduino 库
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

## 许可证

MIT License
