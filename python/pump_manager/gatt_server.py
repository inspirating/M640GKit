"""
================================================================================
GATT 服务器
================================================================================

作为 GATT Server 运行, 模拟 M640G 泵设备

功能:
- 注册 GATT 服务和特征
- 处理写入请求
- 发送通知
- 管理订阅状态

作者: M640GKit Team
版本: 1.0.0
================================================================================
"""

import ubluetooth
from enums import SERVICE_UUID, READ_UUID, WRITE_UUID
from encryption import crc8_calculate
import time


class GATTServer:
    """
    GATT 服务器类

    负责管理 GATT 服务、特征和描述符

    GATT 架构:
    - Service: 包含一个或多个 Characteristic
    - Characteristic: 包含一个 Value 和可选的 Descriptor
    - Descriptor: Characteristic 的元数据
    """

    def __init__(self, ble_instance):
        """
        初始化 GATT 服务器

        参数:
            ble_instance: ubluetooth.BLE 实例
        """
        self.ble = ble_instance
        self.is_running = False

        # 服务和特征句柄
        self._service_handle = None
        self._read_handle = None
        self._write_handle = None

        # 数据缓冲区
        self._read_buffer = bytearray()
        self._write_buffer = bytearray()

        # 回调函数
        self.on_write_request = None
        self.on_subscribe = None

    def start(self):
        """
        启动 GATT 服务器

        注册 GATT 服务并开始广播
        """
        if self.is_running:
            return

        # 注册 GATT 回调
        self.ble.gatt_register_server(self._gatt_server_irq)

        # 创建服务
        self._service_handle = self.ble.gatt_server_service(SERVICE_UUID)

        # 创建写入特征 (可写)
        self._write_handle = self.ble.gatt_server_characteristic(
            self._service_handle,
            WRITE_UUID,
            0x06
        )

        # 创建读取/通知特征 (可读, 可通知)
        self._read_handle = self.ble.gatt_server_characteristic(
            self._service_handle,
            READ_UUID,
            0x12
        )

        # 注册服务
        self.ble.gatt_server_register(self._service_handle)

        # 启动广播
        self._start_advertising()

        self.is_running = True

    def stop(self):
        """
        停止 GATT 服务器
        """
        if not self.is_running:
            return

        self.ble.gap_advertise(None)

        if self._service_handle:
            self.ble.gatt_server_unregister(self._service_handle)

        self.is_running = False

    def _start_advertising(self):
        """
        开始 BLE 广播

        广播数据包包含设备名称和 Service UUID
        """
        adv_data = self._build_advertising_data()
        scan_resp = bytearray()
        self.ble.gap_advertise(100000, adv_data, scan_resp)

    def _build_advertising_data(self) -> bytearray:
        """
        构建广播数据包

        返回:
            广播数据字节数组
        """
        adv_data = bytearray()

        # Flags
        adv_data.append(0x02)
        adv_data.append(0x01)
        adv_data.append(0x06)

        # 设备名称
        name_bytes = b"MT"
        adv_data.append(len(name_bytes) + 1)
        adv_data.append(0x09)
        adv_data.extend(name_bytes)

        # 制造商数据
        manufacturer_data = bytearray()
        manufacturer_data.append(0x59)
        manufacturer_data.append(0x6A)
        uuid_bytes = bytes([
            0x00, 0x08, 0x96, 0x8F, 0xE3, 0x11, 0x60, 0x50,
            0x55, 0x58, 0xB3
        ])
        manufacturer_data.extend(uuid_bytes)
        manufacturer_data.extend(b'\x28\xd8\x12\x4a')
        manufacturer_data.append(1)
        manufacturer_data.append(1)

        adv_data.append(len(manufacturer_data) + 1)
        adv_data.append(0xFF)
        adv_data.extend(manufacturer_data)

        return adv_data

    def send_notification(self, data: bytes, use_crc_hack: bool = True) -> bool:
        if self._read_handle is None:
            return False

        if use_crc_hack and len(data) > 0 and data[1] != 0x00:
            if len(data) >= 6:
                expected_crc = crc8_calculate(data[:-2])
                actual_data = list(data)
                if len(actual_data) >= 2 and actual_data[-2] != expected_crc:
                    actual_data[-1] = 0x00
                    data = bytes(actual_data)

        try:
            self.ble.gatt_server_notify(self._read_handle, data, True)
            return True
        except Exception as e:
            print(f"发送通知失败: {e}")
            return False

    def send_notification_with_crc_hack(self, data: bytes) -> bool:
        return self.send_notification(data, use_crc_hack=True)

    def _gatt_server_irq(self, event, data):
        """GATT 服务器中断处理函数"""
        if event == self.ble.GATT_SERVER_WRITE:
            self._handle_write(data)
        elif event == self.ble.GATT_SERVER_SUBSCRIBE:
            self._handle_subscribe(data)

    def _handle_write(self, data):
        """处理写入请求"""
        handle, offset, data = data

        if handle == self._write_handle:
            self._write_buffer.extend(data)

            if len(self._write_buffer) >= 6:
                packet_len = self._write_buffer[0]
                if len(self._write_buffer) >= packet_len:
                    complete_packet = bytes(self._write_buffer[:packet_len])
                    self._write_buffer = self._write_buffer[packet_len:]
                    if self.on_write_request:
                        self.on_write_request(complete_packet)

    def _handle_subscribe(self, data):
        """处理订阅状态变化"""
        handle, notify_enable, indicate_enable = data

        if handle == self._read_handle:
            if self.on_subscribe:
                self.on_subscribe(notify_enable)


class ConnectionTracker:
    """
    连接状态追踪器

    用于追踪连接质量、断开次数和重连策略
    """

    def __init__(self):
        """初始化连接追踪器"""
        self._connection_start_time = None
        self._total_connection_time = 0
        self._disconnect_count = 0
        self._last_disconnect_reason = None
        self._rssi_history = []
        self._max_rssi_history = 20

    def on_connect(self):
        """连接建立时调用"""
        self._connection_start_time = time.time()
        self._rssi_history = []

    def on_disconnect(self, reason: str = ""):
        """连接断开时调用"""
        if self._connection_start_time:
            self._total_connection_time += time.time() - self._connection_start_time
            self._connection_start_time = None
        self._disconnect_count += 1
        self._last_disconnect_reason = reason

    def add_rssi(self, rssi: int):
        """添加 RSSI 读数"""
        self._rssi_history.append(rssi)
        if len(self._rssi_history) > self._max_rssi_history:
            self._rssi_history.pop(0)

    def get_average_rssi(self) -> int:
        """获取平均 RSSI"""
        if not self._rssi_history:
            return 0
        return sum(self._rssi_history) // len(self._rssi_history)

    def get_connection_time(self) -> float:
        """获取当前连接时长"""
        if self._connection_start_time:
            return time.time() - self._connection_start_time
        return 0

    def get_total_connection_time(self) -> float:
        """获取累计连接时长"""
        return self._total_connection_time

    def get_disconnect_count(self) -> int:
        """获取断开次数"""
        return self._disconnect_count

    def get_stats(self) -> dict:
        """获取连接统计信息"""
        return {
            'current_connection_time': self.get_connection_time(),
            'total_connection_time': self.get_total_connection_time(),
            'disconnect_count': self.get_disconnect_count(),
            'average_rssi': self.get_average_rssi(),
            'last_disconnect_reason': self._last_disconnect_reason
        }


class BLEEventManager:
    """
    BLE 事件管理器

    提供事件订阅/发布机制
    """

    def __init__(self):
        """初始化事件管理器"""
        self._subscribers = {
            'on_connect': [],
            'on_disconnect': [],
            'on_auth_success': [],
            'on_auth_failure': [],
            'on_sync_data': [],
            'on_notification': [],
            'on_error': [],
            'on_state_change': []
        }

    def subscribe(self, event: str, callback):
        """订阅事件"""
        if event in self._subscribers:
            self._subscribers[event].append(callback)

    def unsubscribe(self, event: str, callback):
        """取消订阅"""
        if event in self._subscribers and callback in self._subscribers[event]:
            self._subscribers[event].remove(callback)

    def publish(self, event: str, *args, **kwargs):
        """发布事件"""
        if event in self._subscribers:
            for callback in self._subscribers[event]:
                try:
                    callback(*args, **kwargs)
                except Exception as e:
                    print(f"事件回调错误 [{event}]: {e}")

    def clear(self):
        """清除所有订阅"""
        for event in self._subscribers:
            self._subscribers[event] = []