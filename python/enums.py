"""
================================================================================
枚举类型定义
================================================================================

包含所有 BLE 通信相关的枚举类型:
- BLE 状态
- 命令类型
- Patch 状态
- 基础率类型
- 警报设置
- 同步响应掩码
- 连接错误类型

作者: M640GKit Team
版本: 1.0.0
================================================================================
"""

import ubluetooth


# =============================================================================
# BLE UUID 定义
# =============================================================================

# GATT 服务和特征 UUID
SERVICE_UUID = ubluetooth.UUID("669A9001-0008-968F-E311-6050405558B3")
READ_UUID = ubluetooth.UUID("669a9120-0008-968f-e311-6050405558b3")
WRITE_UUID = ubluetooth.UUID("669a9101-0008-968f-e311-6050405558B3")


# =============================================================================
# BLE 状态机
# 用于管理连接状态转换
# =============================================================================

class BLEState:
    """
    BLE 连接状态枚举

    状态转换图:
    IDLE -> SCANNING -> CONNECTING -> CONNECTED -> AUTHENTICATING -> AUTHENTICATED
                                    -> DISCONNECTED -> CONNECTING (重连)
    """
    IDLE = "idle"                  # 空闲, 未连接
    SCANNING = "scanning"          # 扫描中
    CONNECTING = "connecting"       # 连接中
    CONNECTED = "connected"         # 已连接
    AUTHENTICATING = "authenticating"  # 认证中
    AUTHENTICATED = "authenticated"    # 已认证
    DISCONNECTED = "disconnected"  # 断开连接


# =============================================================================
# 命令类型枚举
# 与泵通信的所有命令类型
# =============================================================================

class CommandType:
    """
    命令类型常量, 对应泵的各个功能

    命令类型说明:
    - SYNCHRONIZE: 轮询泵状态 (最常用的命令)
    - SUBSCRIBE: 订阅通知
    - AUTH_REQ: 认证请求 (连接后第一个命令)
    - GET/SET_TIME: 时间同步
    - SET_BOLUS: 大剂量
    - SET_TEMP_BASAL: 临时基础率
    - SUSPEND/RESUME: 暂停/恢复泵
    """
    SYNCHRONIZE = 3          # 同步泵状态 (轮询)
    SUBSCRIBE = 4            # 订阅通知
    AUTH_REQ = 5             # 认证请求
    GET_DEVICE_TYPE = 6      # 获取设备类型
    SET_TIME = 10            # 设置泵时间
    GET_TIME = 11            # 获取泵时间
    SET_TIME_ZONE = 12       # 设置时区
    PRIME = 16               # 灌注操作
    ACTIVATE = 18            # 激活 Patch
    SET_BOLUS = 19           # 设置大剂量
    CANCEL_BOLUS = 20        # 取消大剂量
    SET_BASAL_PROFILE = 21   # 设置基础率配置文件
    SET_TEMP_BASAL = 24      # 设置临时基础率
    CANCEL_TEMP_BASAL = 25   # 取消临时基础率
    SUSPEND_PUMP = 28        # 暂停泵
    RESUME_PUMP = 29         # 恢复泵
    POLL_PATCH = 30          # 轮询 Patch 状态
    STOP_PATCH = 31          # 停止 Patch
    READ_BOLUS_STATE = 34    # 读取大剂量状态
    SET_PATCH = 35           # 设置 Patch
    SET_BOLUS_MOTOR = 36     # 设置大剂量电机
    GET_RECORD = 99          # 获取记录
    CLEAR_ALARM = 115        # 清除警报


# =============================================================================
# Patch 状态枚举
# 表示泵的不同工作状态
# =============================================================================

class PatchState:
    """
    泵 Patch 状态枚举

    状态分类:
    - 初始化状态: NONE, IDLE, FILLED, PRIMING, PRIMED
    - 运行状态: ACTIVE, ACTIVE_ALT
    - 暂停状态: SUSPENDED, PAUSED, AUTO_SUSPENDED, LOW_BG_SUSPENDED
    - 错误状态: OCCLUSION, EXPIRED, RESERVOIR_EMPTY, PATCH_FAULT
    - 停止状态: STOPPED, EJECTED
    """
    NONE = 0                 # 无状态/未知
    IDLE = 1                 # 空闲
    FILLED = 2               # 已填充胰岛素
    PRIMING = 3              # 灌注中
    PRIMED = 4               # 已完成灌注
    EJECTING = 5             # 弹出中
    EJECTED = 6              # 已弹出
    ACTIVE = 32              # 运行中 (正常工作中)
    ACTIVE_ALT = 33          # 运行中 (备用)
    LOW_BG_SUSPENDED = 64    # 低血糖自动暂停
    AUTO_SUSPENDED = 66      # 自动暂停
    SUSPENDED = 69           # 手动暂停
    PAUSED = 70              # 暂停
    OCCLUSION = 96           # 堵管报警
    EXPIRED = 97             # Patch 已过期
    RESERVOIR_EMPTY = 98     # 储药器空
    PATCH_FAULT = 99         # Patch 故障
    STOPPED = 128            # 已停止


# =============================================================================
# 基础率类型枚举
# 用于区分不同类型的基础率操作
# =============================================================================

class BasalType:
    """
    基础率类型枚举

    类型说明:
    - STANDARD: 标准基础率 (配置文件)
    - TEMP_*: 临时基础率 (绝对值/相对值)
    - SUSPEND_*: 各种暂停原因
    - AUTO_*: 自动模式相关
    """
    NONE = 0
    STANDARD = 1
    EXERCISE = 2
    HOLIDAY = 3
    PROGRAM_A = 4
    PROGRAM_B = 5
    ABSOLUTE_TEMP = 6       # 绝对值临时基础率
    RELATIVE_TEMP = 7       # 相对值临时基础率
    PROGRAM_C = 8
    PROGRAM_D = 9
    SICK = 10
    AUTO = 11
    NEW = 12
    SUSPEND_LOW_GLUCOSE = 13
    SUSPEND_PREDICT_LOW_GLUCOSE = 14
    SUSPEND_AUTO = 15
    SUSPEND_MORE_THAN_MAX_PER_HOUR = 16
    SUSPEND_MORE_THAN_MAX_PER_DAY = 17
    SUSPEND_MANUAL = 18
    SUSPEND_KEY_LOST = 19
    STOP_OCCLUSION = 20
    STOP_EXPIRED = 21
    STOP_EMPTY = 22
    STOP_PATCH_FAULT = 23
    STOP_BASE_FAULT = 24
    STOP_BATTERY_EMPTY = 25
    STOP = 26
    PAUSE_INTERRUPT = 27
    PRIME = 28
    AUTO_MODE_START = 29
    AUTO_MODE_EXIT = 30


# =============================================================================
# 警报设置枚举
# 控制泵的警报方式
# =============================================================================

class AlarmSettings:
    """
    警报设置枚举

    警报方式:
    - LIGHT_VIBRATE_BEEP: 灯光+震动+蜂鸣 (最大声)
    - VIBRATE_ONLY: 仅震动 (安静模式)
    - NONE: 完全静音
    """
    LIGHT_VIBRATE_BEEP = 0   # 灯光+震动+蜂鸣
    LIGHT_VIBRATE = 1        # 灯光+震动
    LIGHT_BEEP = 2           # 灯光+蜂鸣
    LIGHT_ONLY = 3           # 仅灯光
    VIBRATE_BEEP = 4         # 震动+蜂鸣
    VIBRATE_ONLY = 5         # 仅震动
    BEEP_ONLY = 6            # 仅蜂鸣
    NONE = 7                 # 静音


# =============================================================================
# 警报类型枚举
# =============================================================================

class AlertType:
    """
    警报类型枚举

    类型说明:
    - HOURLY: 每小时警报 (提醒)
    - DAILY: 每日警报 (提醒)
    """
    HOURLY = 4   # 每小时警报
    DAILY = 5    # 每日警报


# =============================================================================
# 连接错误类型
# 定义所有可能的连接错误
# =============================================================================

class ConnectError:
    """
    连接错误类型常量

    错误类型说明:
    - NO_SERIAL_NUMBER: 未找到设备序列号
    - TIMEOUT: 连接/操作超时
    - NOT_CONNECTED: 未连接
    - PUMP_BUSY: 泵忙 (正在执行其他操作)
    """
    NO_SERIAL_NUMBER = "noSerialNumberAvailable"
    INVALID_BLUETOOTH_STATE = "invalidBluetoothState"
    FAILED_TO_CONNECT = "failedToConnectToDevice"
    FAILED_TO_FIND_DEVICE = "failedToFindDevice"
    NO_MANAGER = "noManager"
    NO_WRITE_CHARACTERISTIC = "noWriteCharacteristic"
    ALREADY_RUNNING = "alreadyRunning"
    NO_DATA = "noData"
    TIMEOUT = "timeout"
    FAILED_DISCOVER_SERVICES = "failedToDiscoverServices"
    FAILED_DISCOVER_CHARACTERISTICS = "failedToDiscoverCharacteristics"


# =============================================================================
# 同步响应掩码
# 用于解析同步响应数据中的各个字段
# =============================================================================

# 同步响应字段掩码
MASK_SUSPEND = 0x0001              # 暂停时间
MASK_NORMAL_BOLUS = 0x0002         # 普通大剂量
MASK_EXTENDED_BOLUS = 0x0004       # 延时大剂量
MASK_BASAL = 0x0008                # 基础率
MASK_SETUP = 0x0010               # 设置/灌注进度
MASK_RESERVOIR = 0x0020           # 储药器余量
MASK_START_TIME = 0x0040          # 开始时间
MASK_BATTERY = 0x0080             # 电池状态
MASK_STORAGE = 0x0100             # 存储数据
MASK_ALARM = 0x0200               # 警报状态
MASK_AGE = 0x0400                 # Patch 使用时长
MASK_MAGNETO_PLACE = 0x0800       # 磁铁位置


# =============================================================================
# BLE 错误码定义
# =============================================================================

class BLEErrorCode:
    """
    BLE 错误码常量

    错误码分类:
    - SUCCESS: 成功 (0x0000)
    - 通用错误: 0x0100-0x01FF
    - 认证错误: 0x0200-0x02FF
    - 泵错误: 0x0300-0x03FF
    - 状态错误: 0x0400-0x04FF
    """

    SUCCESS = 0x0000

    # 通用错误
    UNKNOWN_ERROR = 0x0100
    INVALID_PARAMETER = 0x0101
    TIMEOUT = 0x0102
    NOT_CONNECTED = 0x0103
    ALREADY_CONNECTED = 0x0104

    # 认证错误
    AUTH_REQUIRED = 0x0200
    AUTH_FAILED = 0x0201
    INVALID_KEY = 0x0202
    INVALID_TOKEN = 0x0203

    # 泵错误
    PUMP_BUSY = 0x0300
    PUMP_SUSPENDED = 0x0301
    RESERVOIR_LOW = 0x0302
    RESERVOIR_EMPTY = 0x0303
    BATTERY_LOW = 0x0304
    BATTERY_EMPTY = 0x0305
    OCCLUSION = 0x0306
    PATCH_EXPIRED = 0x0307
    MAX_BOLUS_EXCEEDED = 0x0308
    MAX_BASAL_EXCEEDED = 0x0309

    # 状态错误
    INVALID_STATE = 0x0400
    NOT_INITIALIZED = 0x0401
    ALREADY_INITIALIZED = 0x0402

    @staticmethod
    def to_string(error_code: int) -> str:
        """
        将错误码转换为字符串

        参数:
            error_code: 错误码

        返回:
            错误描述
        """
        error_map = {
            BLEErrorCode.SUCCESS: "成功",
            BLEErrorCode.UNKNOWN_ERROR: "未知错误",
            BLEErrorCode.INVALID_PARAMETER: "参数无效",
            BLEErrorCode.TIMEOUT: "超时",
            BLEErrorCode.NOT_CONNECTED: "未连接",
            BLEErrorCode.ALREADY_CONNECTED: "已连接",
            BLEErrorCode.AUTH_REQUIRED: "需要认证",
            BLEErrorCode.AUTH_FAILED: "认证失败",
            BLEErrorCode.INVALID_KEY: "密钥无效",
            BLEErrorCode.INVALID_TOKEN: "令牌无效",
            BLEErrorCode.PUMP_BUSY: "泵忙",
            BLEErrorCode.PUMP_SUSPENDED: "泵已暂停",
            BLEErrorCode.RESERVOIR_LOW: "储药器低",
            BLEErrorCode.RESERVOIR_EMPTY: "储药器空",
            BLEErrorCode.BATTERY_LOW: "电池低",
            BLEErrorCode.BATTERY_EMPTY: "电池空",
            BLEErrorCode.OCCLUSION: "堵管",
            BLEErrorCode.PATCH_EXPIRED: "Patch已过期",
            BLEErrorCode.MAX_BOLUS_EXCEEDED: "超过最大大剂量",
            BLEErrorCode.MAX_BASAL_EXCEEDED: "超过最大基础率",
            BLEErrorCode.INVALID_STATE: "状态无效",
            BLEErrorCode.NOT_INITIALIZED: "未初始化",
            BLEErrorCode.ALREADY_INITIALIZED: "已初始化"
        }
        return error_map.get(error_code, f"未知错误 (0x{error_code:04X})")