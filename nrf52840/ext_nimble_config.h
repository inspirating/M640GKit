/*
================================================================================
NimBLE-Arduino 配置文件 (nRF52840 平台)
================================================================================

此文件告诉 NimBLE-Arduino 库使用 Nordic nRF52840 平台的配置。
需要放在项目根目录 (与 .ino 文件同级)。
================================================================================
*/

#ifndef EXT_NIMBLE_CONFIG_H
#define EXT_NIMBLE_CONFIG_H

// 启用 BLE
#define CONFIG_BT_ENABLED 1
#define CONFIG_BT_NIMBLE_ENABLED 1
#define CONFIG_BT_CONTROLLER_ENABLED 1

// 启用 Peripheral 角色 (GATT Server 必需)
#define CONFIG_BT_NIMBLE_ROLE_PERIPHERAL 1
#define CONFIG_BT_NIMBLE_ROLE_BROADCASTER 1

// 禁用不需要的角色以节省空间
#define CONFIG_BT_NIMBLE_ROLE_CENTRAL_DISABLED 1
#define CONFIG_BT_NIMBLE_ROLE_OBSERVER_DISABLED 1

// 最大连接数 (Peripheral 模式用 1)
#define CONFIG_BT_NIMBLE_MAX_CONNECTIONS 1

// MTU 大小
#define CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU 247

// 调试日志级别 (0=NONE, 5=CRITICAL)
#define CONFIG_BT_NIMBLE_LOG_LEVEL 5

// 启用安全连接
#define CONFIG_BT_NIMBLE_SM_LEGACY 1
#define CONFIG_BT_NIMBLE_SM_SC 1

// 最大存储的 bond 数
#define CONFIG_BT_NIMBLE_MAX_BONDS 1

// 最大 CCCD 存储数
#define CONFIG_BT_NIMBLE_MAX_CCCDS 2

// 传输层缓冲区配置
#define CONFIG_BT_NIMBLE_TRANSPORT_ACL_FROM_LL_COUNT 4
#define CONFIG_BT_NIMBLE_TRANSPORT_ACL_SIZE 247
#define CONFIG_BT_NIMBLE_TRANSPORT_EVT_COUNT 15
#define CONFIG_BT_NIMBLE_TRANSPORT_EVT_DISCARD_COUNT 4
#define CONFIG_BT_NIMBLE_TRANSPORT_EVT_SIZE 247

// GATT 最大过程数
#define CONFIG_BT_NIMBLE_GATT_MAX_PROCS 1

// MSYS 缓冲区配置
#define CONFIG_BT_NIMBLE_MSYS1_BLOCK_COUNT 8
#define CONFIG_BT_NIMBLE_MSYS_1_BLOCK_COUNT 8
#define CONFIG_BT_NIMBLE_MSYS_1_BLOCK_SIZE 256

// 设备名最大长度
#define CONFIG_BT_NIMBLE_GAP_DEVICE_NAME_MAX_LEN 31

// 主机栈大小
#define CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE 4096

// 禁用不必要的功能以节省空间
#define CONFIG_BT_NIMBLE_L2CAP_COC_MAX_NUM 0

#endif // EXT_NIMBLE_CONFIG_H
