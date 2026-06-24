/*
================================================================================
NimBLE-Arduino 外部配置 (nRF52840 / n-able-Arduino 平台)
================================================================================

NimBLE-Arduino 在非 ESP 平台编译时, nimconfig.h 会 #include "ext_nimble_config.h"。
本文件提供该配置, 覆盖默认值以适配 M640GKit 泵模拟器需求。

参考: NimBLE-Arduino/docs/Command_line_config.md
================================================================================
*/

#ifndef EXT_NIMBLE_CONFIG_H
#define EXT_NIMBLE_CONFIG_H

// ---------- MYNEWT_VAL 宏 ----------
// NimBLE 用 MYNEWT_VAL(x) 读取配置值, 展开为 MYNEWT_VAL_x。
// ESP32 平台由 esp_nimble_cfg.h 定义; 非 ESP 平台 syscfg.h 中该定义
// 被 #if 0 包裹未生效, 必须在此提供。
#ifndef MYNEWT_VAL
#define MYNEWT_VAL(_name)          MYNEWT_VAL_ ## _name
#endif
#ifndef MYNEWT_VAL_CHOICE
#define MYNEWT_VAL_CHOICE(_name, _val)  MYNEWT_VAL_ ## _name ## __ ## _val
#endif

// ---------- Controller ----------
// n-able-Arduino 核心不用 SoftDevice, NimBLE 自带完整 BLE 栈 (host + controller)。
// 启用 controller 让 NimBLE 直接驱动 nRF52840 无线电。
#define NIMBLE_CFG_CONTROLLER 1

// ---------- 角色配置 ----------
// M640GKit 只做 Peripheral (GATT Server), 不做 Central/Observer。
// Peripheral 和 Broadcaster 必须启用 (advertising 需要)。
#define CONFIG_BT_NIMBLE_ROLE_CENTRAL 0
#define CONFIG_BT_NIMBLE_ROLE_OBSERVER 0
#define CONFIG_BT_NIMBLE_ROLE_PERIPHERAL 1
#define CONFIG_BT_NIMBLE_ROLE_BROADCASTER 1
// NimBLE host 层角色
#define MYNEWT_VAL_BLE_ROLE_CENTRAL (0)
#define MYNEWT_VAL_BLE_ROLE_OBSERVER (0)

// ---------- 连接与 MTU ----------
// 最多 1 个 BLE 连接 (泵模拟器只连一台手机)
#define CONFIG_BT_NIMBLE_MAX_CONNECTIONS 1
// MTU = 247 (与 gatt_server.h 中 setMTU(247) 对应)
#define CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU 247

// ---------- 设备名 ----------
#define CONFIG_BT_NIMBLE_SVC_GAP_DEVICE_NAME "MT"

// ---------- Bonding / CCCD ----------
// 不做 BLE 配对/bonding (Medtrum 协议层用 AUTH_REQ 自己认证)
#define CONFIG_BT_NIMBLE_MAX_BONDS 0
// CCCD 订阅数: 2 个特征值各 1 个 = 2
#define CONFIG_BT_NIMBLE_MAX_CCCDS 2

// ---------- 调试 ----------
// 关闭 NimBLE 协议栈调试日志 (节省 ~32kB flash)
#define CONFIG_BT_NIMBLE_LOG_LEVEL 5
// 关闭 NimBLE CPP 封装层调试日志
#define CONFIG_NIMBLE_CPP_LOG_LEVEL 0

// ---------- 任务栈 ----------
// NimBLE host 任务栈大小 (nRF52840 RAM 充足, 4096 足够)
#define CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE 4096

// ---------- MSYS 缓冲 ----------
// msys_1 块数: 用于 prepare write/response, MTU=247 时 12 块足够
#define CONFIG_BT_NIMBLE_MSYS1_BLOCK_COUNT 12

#endif // EXT_NIMBLE_CONFIG_H
