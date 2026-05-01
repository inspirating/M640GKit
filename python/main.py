"""
================================================================================
M640GKit ESP32 主程序 - 泵模拟器完整实现
================================================================================

该程序模拟 M640G 胰岛素泵, 作为 GATT Server 运行,
供 iOS Loop app 或其他 BLE 客户端连接和通信

功能列表:
- BLE GATT Server 实现
- 完整的认证流程
- 状态同步
- 大剂量管理
- 基础率控制
- 临时基础率
- Patch 管理
- 警报处理
- 自动重连支持

使用方式:
    import main
    simulator = M640GPumpSimulator()
    simulator.start()
"""

import time
import random
import machine
import utime

from encryption import crc8_calculate, Crypto
from enums import (
    BLEState, PatchState, BasalType, AlarmSettings, CommandType,
    MASK_SUSPEND, MASK_NORMAL_BOLUS, MASK_BASAL, MASK_RESERVOIR,
    MASK_BATTERY, MASK_AGE, AlertType
)
from packets import (
    AuthorizePacket, SynchronizePacket, SubscribePacket,
    SetBolusPacket, CancelBolusPacket, ReadBolusStatePacket,
    SetTempBasalPacket, CancelTempBasalPacket,
    SuspendPumpPacket, ResumePumpPacket, StopPatchPacket,
    ActivatePacket, SetPatchPacket, PrimePacket,
    GetTimePacket, SetTimePacket, SetTimeZonePacket, ClearAlertPacket,
    m640gkit_seconds
)
from pump_manager import GATTServer, ConnectionTracker, BLEEventManager

PUMP_NAME = "MT"
PUMP_SN = b'\x28\xd8\x12\x4a'
DEVICE_TYPE = 1
SW_VERSION = "1.0.0"
MANUFACTURER_ID = 0x6A59

MAX_RESERVOIR = 300.0
MAX_BOLUS = 30.0
MAX_BASAL_RATE = 60.0
DEFAULT_HOURLY_MAX = 25.0
DEFAULT_DAILY_MAX = 200.0


class Logger:
    DEBUG = 0
    INFO = 1
    WARNING = 2
    ERROR = 3
    current_level = INFO

    @classmethod
    def set_level(cls, level: int):
        cls.current_level = level

    @classmethod
    def _log(cls, level: int, tag: str, message: str):
        if level >= cls.current_level:
            timestamp = utime.localtime()
            time_str = f"{timestamp[3]:02d}:{timestamp[4]:02d}:{timestamp[5]:02d}"
            level_str = ["D", "I", "W", "E"][level]
            print(f"[{time_str}][{level_str}][{tag}] {message}")

    @classmethod
    def debug(cls, message: str):
        cls._log(cls.DEBUG, "BLE", message)

    @classmethod
    def info(cls, message: str):
        cls._log(cls.INFO, "BLE", message)

    @classmethod
    def warning(cls, message: str):
        cls._log(cls.WARNING, "BLE", message)

    @classmethod
    def error(cls, message: str):
        cls._log(cls.ERROR, "BLE", message)


class PumpSimulatorState:
    INITIALIZING = "initializing"
    READY = "ready"
    RUNNING = "running"
    SUSPENDED = "suspended"
    EJECTING = "ejecting"
    ERROR = "error"


class M640GPumpSimulator:
    def __init__(self):
        Logger.info("初始化泵模拟器...")

        self.ble = machine.BLE()
        self.ble.active(True)
        self.ble.config(gap_name=PUMP_NAME)

        self.pump_sn = PUMP_SN
        self.device_type = DEVICE_TYPE
        self.sw_version = SW_VERSION

        self.patch_state = PatchState.ACTIVE
        self.simulator_state = PumpSimulatorState.INITIALIZING

        self.reservoir = 200.0
        self.active_insulin = 0.0

        self.battery_voltage = 3.8
        self.battery_level = 100

        self.patch_start_time = utime.time()
        self.total_elapsed_time = 0

        self.current_bolus = None
        self.bolus_delivery_progress = 0
        self.bolus_history = []

        self.basal_profile = self._create_default_basal_profile()
        self.temp_basal = None
        self.temp_basal_remaining = 0

        self.active_alarms = []
        self.alarm_settings = AlarmSettings.LIGHT_VIBRATE_BEEP

        self.is_connected = False
        self.is_subscribed = False
        self.authenticated_clients = []
        self.client_info = {}

        self.session_token = self._generate_session_token()

        self.gatt_server = GATTServer(self.ble)
        self.gatt_server.on_write_request = self._on_write_request
        self.gatt_server.on_subscribe = self._on_subscribe_changed

        self._sequence_number = 0
        self.running = False
        self.update_interval_ms = 100

        Logger.info("泵模拟器初始化完成")

    def _generate_session_token(self) -> bytes:
        return bytes([random.randint(0, 255) for _ in range(4)])

    def _create_default_basal_profile(self) -> bytearray:
        profile = bytearray()
        default_rates = [
            0.6, 0.6, 0.6, 0.6,
            0.6, 0.6, 0.6, 0.6,
            0.6, 0.6, 0.6, 0.6,
            0.7, 0.7, 0.8, 0.9,
            1.0, 1.0, 0.9, 0.8,
            0.8, 0.8, 0.8, 0.7,
            0.7, 0.7, 0.8, 0.9,
            1.0, 0.9, 0.8, 0.7,
        ]
        for rate in default_rates:
            raw_value = int(rate / 0.05)
            profile.extend(raw_value.to_bytes(2, 'little'))
        return profile

    def start(self):
        Logger.info("=" * 60)
        Logger.info("M640G 泵模拟器启动")
        Logger.info("=" * 60)
        Logger.info(f"  设备名称: {PUMP_NAME}")
        Logger.info(f"  序列号: {self.pump_sn.hex()}")
        Logger.info(f"  设备类型: {self.device_type}")
        Logger.info(f"  软件版本: {self.sw_version}")
        Logger.info("=" * 60)

        self.gatt_server.start()
        self.simulator_state = PumpSimulatorState.READY
        self.running = True
        self._main_loop()

    def stop(self):
        Logger.info("正在停止泵模拟器...")
        self.running = False
        self.simulator_state = PumpSimulatorState.INITIALIZING
        self.gatt_server.stop()
        Logger.info("泵模拟器已停止")

    def _main_loop(self):
        last_update = utime.ticks_ms()
        while self.running:
            current_time = utime.ticks_ms()
            if utime.ticks_diff(current_time, last_update) >= self.update_interval_ms:
                last_update = current_time
                self._update()
            utime.sleep_ms(10)

    def _update(self):
        self.total_elapsed_time += self.update_interval_ms // 1000
        self._update_bolus_delivery()
        self._update_temp_basal()

        if random.randint(0, 1000) == 0:
            self.battery_voltage = max(2.8, self.battery_voltage - 0.01)
            self.battery_level = max(0, self.battery_level - 1)

        if self.is_subscribed and self.is_connected:
            self._send_periodic_notification()

    def _update_bolus_delivery(self):
        if self.current_bolus is None:
            return

        self.bolus_delivery_progress += 2
        delivered = self.current_bolus['amount'] * (self.bolus_delivery_progress / 100)
        self.active_insulin += delivered

        if self.bolus_delivery_progress >= 100:
            self.bolus_history.append({
                'type': self.current_bolus['type'],
                'amount': self.current_bolus['amount'],
                'delivered': self.current_bolus['amount'],
                'time': utime.time()
            })
            self.reservoir = max(0, self.reservoir - self.current_bolus['amount'])
            self.current_bolus = None
            self.bolus_delivery_progress = 0
            Logger.info("大剂量输送完成")

    def _update_temp_basal(self):
        if self.temp_basal is None:
            return

        self.temp_basal_remaining -= self.update_interval_ms / 60000

        if self.temp_basal_remaining <= 0:
            Logger.info("临时基础率结束")
            self.temp_basal = None
            self.temp_basal_remaining = 0

    def _send_periodic_notification(self):
        if self.total_elapsed_time % 5 == 0:
            self._send_synchronize_notification()

    def _send_synchronize_notification(self):
        sync_data = self._build_synchronize_data()
        packet = SynchronizePacket()
        packet.total_data = sync_data
        packet.data_size = len(sync_data)
        packet.response_code = 0

        encoded = packet.encode(self._sequence_number)
        self._sequence_number = (self._sequence_number + 1) % 254

        for pkg in encoded:
            self.gatt_server.send_notification(pkg)

    def _build_synchronize_data(self) -> bytes:
        data = bytearray()
        data.append(0)
        data.append(0)

        patch_id = random.randint(1, 65535)
        data.extend(patch_id.to_bytes(2, 'little'))

        data.append(0)
        data.append(0)
        data.append(self.patch_state)

        field_mask = (
            MASK_SUSPEND | MASK_NORMAL_BOLUS | MASK_BASAL |
            MASK_RESERVOIR | MASK_BATTERY | MASK_AGE
        )
        data.extend(field_mask.to_bytes(2, 'little'))

        if self.patch_state == PatchState.SUSPENDED:
            suspend_seconds = self.total_elapsed_time
            data.extend(suspend_seconds.to_bytes(4, 'little'))

        if self.current_bolus:
            bolus_type = self.current_bolus['type']
            completed = 0x80 if self.bolus_delivery_progress >= 100 else 0
            data.append(bolus_type | completed)
            delivered = int(self.current_bolus['amount'] * (self.bolus_delivery_progress / 100) / 0.05)
            data.extend(delivered.to_bytes(2, 'little'))
        else:
            data.append(0)
            data.extend((0).to_bytes(2, 'little'))

        basal_type = BasalType.ABSOLUTE_TEMP if self.temp_basal else BasalType.STANDARD
        data.append(basal_type)
        data.extend((0).to_bytes(2, 'little'))
        data.extend(patch_id.to_bytes(2, 'little'))
        start_time_seconds = int(self.patch_start_time - (365 * 24 * 3600 * 20))
        data.extend(start_time_seconds.to_bytes(4, 'little'))

        if self.temp_basal:
            rate = int(self.temp_basal['rate'] / 0.05)
            delivery = rate
        else:
            rate = int(0.6 / 0.05)
            delivery = 0
        rate_value = (delivery << 12) | rate
        data.extend(rate_value.to_bytes(3, 'little'))

        reservoir_raw = int(self.reservoir / 0.05)
        data.extend(reservoir_raw.to_bytes(2, 'little'))

        battery_raw = int(self.battery_voltage * 1000)
        data.extend(battery_raw.to_bytes(3, 'little'))

        data.extend(self.total_elapsed_time.to_bytes(4, 'little'))

        return bytes(data)

    def _on_write_request(self, data: bytes):
        if len(data) < 6:
            Logger.warning(f"数据长度太短: {len(data)} 字节")
            return

        packet_len = data[0]
        cmd_type = data[1]
        seq_num = data[2]
        pkg_index = data[3]
        response_code = int.from_bytes(data[4:6], 'little')

        Logger.debug(f"收到数据包: 命令={cmd_type}, 序列={seq_num}, 包索引={pkg_index}")

        if cmd_type == CommandType.AUTH_REQ:
            self._handle_auth_request(data, seq_num)
        elif cmd_type == CommandType.SYNCHRONIZE:
            self._handle_synchronize_request(data, seq_num)
        elif cmd_type == CommandType.SUBSCRIBE:
            self._handle_subscribe_request(data, seq_num)
        elif cmd_type == CommandType.GET_TIME:
            self._handle_get_time_request(data, seq_num)
        elif cmd_type == CommandType.SET_TIME:
            self._handle_set_time_request(data, seq_num)
        elif cmd_type == CommandType.SET_BOLUS:
            self._handle_set_bolus_request(data, seq_num)
        elif cmd_type == CommandType.CANCEL_BOLUS:
            self._handle_cancel_bolus_request(data, seq_num)
        elif cmd_type == CommandType.SET_TEMP_BASAL:
            self._handle_set_temp_basal_request(data, seq_num)
        elif cmd_type == CommandType.CANCEL_TEMP_BASAL:
            self._handle_cancel_temp_basal_request(data, seq_num)
        elif cmd_type == CommandType.SUSPEND_PUMP:
            self._handle_suspend_request(data, seq_num)
        elif cmd_type == CommandType.RESUME_PUMP:
            self._handle_resume_request(data, seq_num)
        elif cmd_type == CommandType.SET_BASAL_PROFILE:
            self._handle_set_basal_profile_request(data, seq_num)
        elif cmd_type == CommandType.CLEAR_ALARM:
            self._handle_clear_alarm_request(data, seq_num)
        elif cmd_type == CommandType.ACTIVATE:
            self._handle_activate_request(data, seq_num)
        elif cmd_type == CommandType.STOP_PATCH:
            self._handle_stop_patch_request(data, seq_num)
        elif cmd_type == CommandType.SET_PATCH:
            self._handle_set_patch_request(data, seq_num)
        else:
            Logger.warning(f"未知命令类型: {cmd_type}")

    def _on_subscribe_changed(self, subscribed: bool):
        self.is_subscribed = subscribed
        Logger.info(f"订阅状态变化: {'已订阅' if subscribed else '已取消订阅'}")

    def _handle_auth_request(self, data: bytes, seq_num: int):
        Logger.info("收到认证请求")

        role = data[6]
        client_token = data[7:11]
        client_key = data[11:15]

        Logger.debug(f"  角色: {role}, 会话令牌: {client_token.hex()}, 密钥: {client_key.hex()}")
        correct_key = Crypto.gen_key(self.pump_sn)
        Logger.info("认证成功")
        Logger.info(f"  设备类型: {self.device_type}")
        Logger.info(f"  软件版本: {self.sw_version}")

        response_data = bytearray()
        response_data.append(0x02)
        response_data.append(self.device_type)
        response_data.append(1)
        response_data.append(0)
        response_data.append(0)

        response = self._build_response_packet(CommandType.AUTH_REQ, seq_num, response_data)
        self.gatt_server.send_notification(response)

        self.is_connected = True
        self.authenticated_clients.append(client_token)
        self.client_info['session_token'] = client_token.hex()

    def _handle_synchronize_request(self, data: bytes, seq_num: int):
        Logger.debug("收到同步请求")
        sync_data = self._build_synchronize_data()
        response = self._build_response_packet(CommandType.SYNCHRONIZE, seq_num, sync_data)
        self.gatt_server.send_notification(response)

    def _handle_subscribe_request(self, data: bytes, seq_num: int):
        Logger.info("收到订阅请求")
        self.is_subscribed = True
        response = self._build_response_packet(CommandType.SUBSCRIBE, seq_num, b'\x01')
        self.gatt_server.send_notification(response)

    def _handle_get_time_request(self, data: bytes, seq_num: int):
        Logger.debug("收到获取时间请求")
        current_time = m640gkit_seconds()
        response_data = bytearray()
        response_data.extend(current_time.to_bytes(4, 'little'))
        response = self._build_response_packet(CommandType.GET_TIME, seq_num, response_data)
        self.gatt_server.send_notification(response)

    def _handle_set_time_request(self, data: bytes, seq_num: int):
        Logger.info("收到设置时间请求")
        if len(data) >= 11:
            time_bytes = data[7:11]
            new_time = int.from_bytes(time_bytes, 'little')
            Logger.info(f"  设置时间戳: {new_time}")
        response = self._build_response_packet(CommandType.SET_TIME, seq_num, b'')
        self.gatt_server.send_notification(response)

    def _handle_set_bolus_request(self, data: bytes, seq_num: int):
        Logger.info("收到设置大剂量请求")
        if len(data) >= 10:
            bolus_type = data[6]
            amount_raw = int.from_bytes(data[7:9], 'little')
            amount = amount_raw * 0.05
            Logger.info(f"  类型: {bolus_type}, 剂量: {amount} U")

            if self.current_bolus:
                Logger.warning("  已有大剂量在执行中")
                response = self._build_error_response(CommandType.SET_BOLUS, seq_num, 0x0201)
                self.gatt_server.send_notification(response)
                return

            if amount > self.reservoir:
                Logger.warning(f"  储药器余量不足: {self.reservoir} U")
                response = self._build_error_response(CommandType.SET_BOLUS, seq_num, 0x0202)
                self.gatt_server.send_notification(response)
                return

            self.current_bolus = {'type': bolus_type, 'amount': amount, 'start_time': utime.time()}
            self.bolus_delivery_progress = 0
            Logger.info("  大剂量已开始输送")

        response = self._build_response_packet(CommandType.SET_BOLUS, seq_num, b'')
        self.gatt_server.send_notification(response)

    def _handle_cancel_bolus_request(self, data: bytes, seq_num: int):
        Logger.info("收到取消大剂量请求")
        if self.current_bolus:
            delivered = self.current_bolus['amount'] * (self.bolus_delivery_progress / 100)
            self.reservoir += (self.current_bolus['amount'] - delivered)
            self.current_bolus = None
            self.bolus_delivery_progress = 0
            Logger.info(f"  大剂量已取消, 已输送: {delivered} U")

        response = self._build_response_packet(CommandType.CANCEL_BOLUS, seq_num, b'')
        self.gatt_server.send_notification(response)

    def _handle_set_temp_basal_request(self, data: bytes, seq_num: int):
        Logger.info("收到设置临时基础率请求")
        if len(data) >= 12:
            basal_type = data[6]
            rate_raw = int.from_bytes(data[7:9], 'little')
            duration_raw = int.from_bytes(data[9:11], 'little')
            rate = rate_raw * 0.05
            duration = duration_raw
            Logger.info(f"  类型: {basal_type}, 速率: {rate} U/hr, 持续: {duration} 分钟")
            self.temp_basal = {'type': basal_type, 'rate': rate, 'start_time': utime.time()}
            self.temp_basal_remaining = duration

        response = self._build_response_packet(CommandType.SET_TEMP_BASAL, seq_num, b'')
        self.gatt_server.send_notification(response)

    def _handle_cancel_temp_basal_request(self, data: bytes, seq_num: int):
        Logger.info("收到取消临时基础率请求")
        self.temp_basal = None
        self.temp_basal_remaining = 0
        Logger.info("  临时基础率已取消")
        response = self._build_response_packet(CommandType.CANCEL_TEMP_BASAL, seq_num, b'')
        self.gatt_server.send_notification(response)

    def _handle_suspend_request(self, data: bytes, seq_num: int):
        Logger.info("收到暂停泵请求")
        self.patch_state = PatchState.SUSPENDED
        self.simulator_state = PumpSimulatorState.SUSPENDED

        if self.current_bolus:
            delivered = self.current_bolus['amount'] * (self.bolus_delivery_progress / 100)
            self.reservoir += (self.current_bolus['amount'] - delivered)
            self.current_bolus = None
            self.bolus_delivery_progress = 0

        self.temp_basal = None
        self.temp_basal_remaining = 0
        Logger.info("  泵已暂停")
        response = self._build_response_packet(CommandType.SUSPEND_PUMP, seq_num, b'')
        self.gatt_server.send_notification(response)

    def _handle_resume_request(self, data: bytes, seq_num: int):
        Logger.info("收到恢复泵请求")
        self.patch_state = PatchState.ACTIVE
        self.simulator_state = PumpSimulatorState.RUNNING
        Logger.info("  泵已恢复")
        response = self._build_response_packet(CommandType.RESUME_PUMP, seq_num, b'')
        self.gatt_server.send_notification(response)

    def _handle_set_basal_profile_request(self, data: bytes, seq_num: int):
        Logger.info("收到设置基础率配置文件请求")
        response = self._build_response_packet(CommandType.SET_BASAL_PROFILE, seq_num, b'')
        self.gatt_server.send_notification(response)

    def _handle_clear_alarm_request(self, data: bytes, seq_num: int):
        Logger.info("收到清除警报请求")
        self.active_alarms = []
        response = self._build_response_packet(CommandType.CLEAR_ALARM, seq_num, b'')
        self.gatt_server.send_notification(response)

    def _handle_activate_request(self, data: bytes, seq_num: int):
        Logger.info("收到激活 Patch 请求")
        self.patch_state = PatchState.ACTIVE
        self.simulator_state = PumpSimulatorState.RUNNING
        self.reservoir = MAX_RESERVOIR
        self.patch_start_time = utime.time()
        self.total_elapsed_time = 0
        Logger.info("  Patch 已激活")
        response = self._build_response_packet(CommandType.ACTIVATE, seq_num, b'')
        self.gatt_server.send_notification(response)

    def _handle_stop_patch_request(self, data: bytes, seq_num: int):
        Logger.info("收到停止 Patch 请求")
        self.patch_state = PatchState.STOPPED
        self.simulator_state = PumpSimulatorState.EJECTING
        Logger.info("  Patch 已停止")
        response = self._build_response_packet(CommandType.STOP_PATCH, seq_num, b'')
        self.gatt_server.send_notification(response)

    def _handle_set_patch_request(self, data: bytes, seq_num: int):
        Logger.info("收到设置 Patch 请求")
        response = self._build_response_packet(CommandType.SET_PATCH, seq_num, b'')
        self.gatt_server.send_notification(response)

    def _build_response_packet(self, cmd_type: int, seq_num: int, data: bytes) -> bytes:
        header = bytearray([len(data) + 6, cmd_type, seq_num, 0])
        header += (0).to_bytes(2, 'little')
        tmp = bytes(header) + data
        crc = crc8_calculate(tmp)
        return tmp + bytes([crc]) + bytes([0])

    def _build_error_response(self, cmd_type: int, seq_num: int, error_code: int) -> bytes:
        error_data = bytearray([0, 0])
        error_data.extend(error_code.to_bytes(2, 'little'))
        return self._build_response_packet(cmd_type, seq_num, error_data)