"""
================================================================================
BLE 管理器
================================================================================

负责 BLE 扫描、连接、数据收发和状态管理

功能:
- 扫描附近的泵设备
- 连接到泵
- 发送数据包并等待响应
- 处理通知
- 管理连接状态
- 自动重连

作者: M640GKit Team
版本: 1.0.0
================================================================================
"""

import ubluetooth
import time
from enums import BLEState, ConnectError, SERVICE_UUID, READ_UUID, WRITE_UUID
from encryption import crc8_calculate


class BLECallbacks:
    """BLE 回调接口类"""

    def on_scan_result(self, addr: tuple, rssi: int, name: str, adv_data: dict):
        pass

    def on_connect(self, conn_id: int, addr: tuple):
        pass

    def on_disconnect(self, conn_id: int, addr: tuple):
        pass

    def on_discover_services(self, conn_id: int, services: list):
        pass

    def on_discover_characteristics(self, conn_id: int, characteristics: list):
        pass

    def on_notify(self, conn_id: int, handle: int, data: bytes):
        pass

    def on_write(self, conn_id: int, handle: int, data: bytes):
        pass

    def on_state_change(self, state: BLEState):
        pass

    def on_error(self, error: str, details: str = ""):
        pass

    def on_response(self, packet, success: bool, error_code: int = 0):
        pass


class WriteContext:
    """写入上下文,用于管理单次写入操作的状态"""

    def __init__(self, packet, timeout_ms=30000):
        self.packet = packet
        self.timeout_ms = timeout_ms
        self.start_time = time.ticks_ms()
        self.completed = False
        self.success = False
        self.error_code = 0
        self.response_data = None


class M640GKitBLE:
    """
    M640G BLE 管理类

    负责:
    - 扫描附近的泵设备
    - 连接到泵
    - 发送数据包并等待响应
    - 处理通知
    - 管理连接状态
    - 自动重连
    - 心跳保活
    """

    SCAN_TIMEOUT_MS = 15000
    CONNECT_TIMEOUT_MS = 15000
    WRITE_TIMEOUT_MS = 30000
    PING_INTERVAL_MS = 5000
    RECONNECT_MAX_ATTEMPTS = 5
    RECONNECT_INTERVAL_MS = 3000
    CONNECTION_IDLE_TIMEOUT_MS = 60000

    def __init__(self, name: str = "M640G"):
        self.name = name
        self.ble = ubluetooth.BLE()
        self.ble.active(True)
        self.ble.config(gap_name=name)

        self._state = BLEState.IDLE
        self._conn_id = None
        self._server_addr = None
        self._tx_handle = None
        self._rx_handle = None

        self._scanning = False
        self._scan_results = []

        self._write_sequence = 0

        self._pending_write = None
        self._current_packet = None

        self.callbacks = None

        self.auto_reconnect = True

        self._response_callback = None
        self._notify_callback = None

        self._reconnect_attempts = 0
        self._last_activity_time = 0
        self._ping_timer = 0
        self._rssi_history = []
        self._max_rssi_history = 20
        self._write_queue = []
        self._is_processing_queue = False

        self._register_callbacks()

    def _register_callbacks(self):
        self.ble.irq(self._ble_irq_handler)

    def _ble_irq_handler(self, event, data):
        if event == ubluetooth.EVENT_SCAN_RESULT:
            addr, rssi, adv_type, name, adv_data = data
            self._handle_scan_result(addr, rssi, adv_type, name, adv_data)
        elif event == ubluetooth.EVENT_SCAN_DONE:
            self._scanning = False
            self._set_state(BLEState.IDLE)
        elif event == ubluetooth.EVENT_PERIPHERAL_CONNECT:
            conn_id, addr = data
            self._handle_connect(conn_id, addr)
        elif event == ubluetooth.EVENT_PERIPHERAL_DISCONNECT:
            conn_id, addr = data
            self._handle_disconnect(conn_id, addr)
        elif event == ubluetooth.EVENT_GATTC_WRITE_DONE:
            conn_id, value, status = data
            self._handle_write_done(conn_id, value, status)
        elif event == ubluetooth.EVENT_GATTC_NOTIFY:
            conn_id, value_handle, data = data
            self._handle_notify(conn_id, value_handle, data)
        elif event == ubluetooth.EVENT_GATTC_DISCOVER_SERVICES_DONE:
            conn_id, services = data
            self._handle_discover_services(conn_id, services)
        elif event == ubluetooth.EVENT_GATTC_DISCOVER_CHARACTERISTICS_DONE:
            conn_id, characteristics = data
            self._handle_discover_characteristics(conn_id, characteristics)

    def _handle_scan_result(self, addr, rssi, adv_type, name, adv_data):
        if name and (name.startswith("MT") or name.startswith("DM")):
            self._scan_results.append({
                'addr': addr,
                'rssi': rssi,
                'name': name,
                'adv_data': adv_data
            })
            self.add_rssi(rssi)
            if self.callbacks:
                self.callbacks.on_scan_result(addr, rssi, name, adv_data)

    def _handle_connect(self, conn_id, addr):
        self._conn_id = conn_id
        self._server_addr = addr
        self._reconnect_attempts = 0
        self._last_activity_time = time.ticks_ms()
        self._ping_timer = time.ticks_ms()
        self._set_state(BLEState.CONNECTED)
        if self.callbacks:
            self.callbacks.on_connect(conn_id, addr)

    def _handle_disconnect(self, conn_id, addr):
        self._conn_id = None
        self._tx_handle = None
        self._rx_handle = None
        self._set_state(BLEState.DISCONNECTED)
        self._pending_write = None
        if self.callbacks:
            self.callbacks.on_disconnect(conn_id, addr)

        if self.auto_reconnect and self._server_addr:
            self._schedule_reconnect()

    def _handle_write_done(self, conn_id, value, status):
        self.update_activity()
        if self.callbacks:
            self.callbacks.on_write(conn_id, value)

    def _handle_notify(self, conn_id, value_handle, data):
        self.update_activity()
        if self._is_read_characteristic(value_handle):
            if self._notify_callback:
                self._notify_callback(conn_id, value_handle, data)
            if self.callbacks:
                self.callbacks.on_notify(conn_id, value_handle, data)
        else:
            self._handle_response_data(data)

    def _handle_discover_services(self, conn_id, services):
        if self.callbacks:
            self.callbacks.on_discover_services(conn_id, services)

    def _handle_discover_characteristics(self, conn_id, characteristics):
        if self.callbacks:
            self.callbacks.on_discover_characteristics(conn_id, characteristics)

    def _is_read_characteristic(self, handle) -> bool:
        return self._rx_handle and handle == self._rx_handle

    def _is_write_characteristic(self, handle) -> bool:
        return self._tx_handle and handle == self._tx_handle

    def _handle_response_data(self, data: bytes):
        """处理响应数据"""
        if not self._current_packet:
            return

        packet = self._current_packet

        if not packet.total_data:
            if data[1] != packet.command_type:
                packet.failed = True
                self._complete_write(False, 0x0100)
                return

            packet.total_data = data[:-1]
            packet.data_size = data[0]
            packet.sequence_number = data[3]
            packet.response_code = int.from_bytes(data[4:6], 'little')

            initial_crc = crc8_calculate(data[:-1])
            if initial_crc != data[-1]:
                packet.failed = True
                self._complete_write(False, 0x0101)
                return
        else:
            packet.total_data += data[4:-1]
            if data[3] != packet.sequence_number + 1:
                packet.failed = True
                self._complete_write(False, 0x0102)
                return
            packet.sequence_number = data[3]

            new_crc = crc8_calculate(data[:-1])
            if new_crc != data[-1]:
                packet.failed = True
                self._complete_write(False, 0x0103)
                return

        if packet.is_complete:
            if packet.response_code == 16384:
                packet.total_data = b''
                packet.data_size = 0
                return

            if packet.response_code != 0:
                self._complete_write(False, packet.response_code)
            elif packet.failed:
                self._complete_write(False, 0x0104)
            elif not packet.has_enough_data:
                self._complete_write(False, 0x0105)
            else:
                self._complete_write(True, 0)
        else:
            pass

    def _complete_write(self, success: bool, error_code: int):
        if self._pending_write:
            self._pending_write.completed = True
            self._pending_write.success = success
            self._pending_write.error_code = error_code

            if success:
                self._pending_write.response_data = self._current_packet.parse_response()

            if self.callbacks:
                self.callbacks.on_response(self._current_packet, success, error_code)

            self._pending_write = None
            self._current_packet = None

    def _check_write_timeout(self):
        """检查写入是否超时"""
        if not self._pending_write:
            return

        elapsed = time.ticks_diff(time.ticks_ms(), self._pending_write.start_time)
        if elapsed >= self._pending_write.timeout_ms:
            self._complete_write(False, 0x0102)
            if self.callbacks:
                self.callbacks.on_error("timeout", "Write operation timed out")

    def _set_state(self, new_state: BLEState):
        if self._state != new_state:
            self._state = new_state
            if self.callbacks:
                self.callbacks.on_state_change(new_state)

    @property
    def state(self) -> BLEState:
        return self._state

    @property
    def is_connected(self) -> bool:
        return self._state in (BLEState.CONNECTED, BLEState.AUTHENTICATED)

    def scan_start(self, duration_ms: int = None, passive: bool = True):
        if duration_ms is None:
            duration_ms = self.SCAN_TIMEOUT_MS
        self._scan_results = []
        self._scanning = True
        self._set_state(BLEState.SCANNING)
        self.ble.gap_scan(duration_ms, 30000, 30000, passive)

    def scan_stop(self):
        self.ble.gap_scan(None)
        self._scanning = False
        self._set_state(BLEState.IDLE)

    def get_scan_results(self) -> list:
        return self._scan_results

    def connect(self, addr: tuple):
        if self._state == BLEState.CONNECTING:
            return
        self._server_addr = addr
        self._set_state(BLEState.CONNECTING)
        self.ble.gap_connect(addr, self.CONNECT_TIMEOUT_MS)

    def disconnect(self):
        if self._conn_id is not None:
            self.ble.disconnect(self._conn_id)
        self._set_state(BLEState.DISCONNECTED)

    def discover_services(self):
        if self._conn_id is not None:
            self.ble.gattc_discover_services(self._conn_id)

    def discover_characteristics(self, start_handle: int = 1, end_handle: int = 0xFFFF):
        if self._conn_id is not None:
            self.ble.gattc_discover_services(self._conn_id)

    def set_handles(self, tx_handle: int, rx_handle: int):
        """设置写入和读取特征句柄"""
        self._tx_handle = tx_handle
        self._rx_handle = rx_handle

    def set_notify_callback(self, callback):
        """设置通知回调"""
        self._notify_callback = callback

    def write_packet(self, packet) -> tuple:
        """
        发送数据包并等待响应

        参数:
            packet: BasePacket 子类实例

        返回:
            (success: bool, response_data, error_code: int)
        """
        if not self.is_connected:
            return (False, None, 0x0103)

        if self._pending_write:
            return (False, None, 0x0101)

        self._current_packet = packet
        self._pending_write = WriteContext(packet, self.WRITE_TIMEOUT_MS)

        packages = packet.encode(self._write_sequence)
        self._write_sequence = (self._write_sequence + 1) % 254

        for pkg in packages:
            try:
                self.ble.gattc_write(self._conn_id, self._tx_handle, pkg)
            except Exception as e:
                self._complete_write(False, 0x0100)
                if self.callbacks:
                    self.callbacks.on_error("write_error", str(e))
                return (False, None, 0x0100)

        return self._wait_for_response()

    def _wait_for_response(self) -> tuple:
        """等待响应 (轮询方式)"""
        start = time.ticks_ms()

        while self._pending_write and not self._pending_write.completed:
            elapsed = time.ticks_diff(time.ticks_ms(), start)
            if elapsed >= self.WRITE_TIMEOUT_MS:
                self._complete_write(False, 0x0102)
                break
            time.sleep_ms(10)

        if self._pending_write:
            w = self._pending_write
            self._pending_write = None
            return (w.success, w.response_data, w.error_code)

        return (False, None, 0x0102)

    def send_notification_response(self, packet, sequence_number: int) -> list:
        """
        发送通知响应 (用于 GATT Server)

        参数:
            packet: BasePacket 子类实例
            sequence_number: 序列号

        返回:
            编码后的数据包列表
        """
        return packet.encode(sequence_number)

    def _schedule_reconnect(self):
        if self._reconnect_attempts >= self.RECONNECT_MAX_ATTEMPTS:
            if self.callbacks:
                self.callbacks.on_error("reconnect_failed",
                                        f"重连失败, 已达最大尝试次数 {self.RECONNECT_MAX_ATTEMPTS}")
            return

        self._reconnect_attempts += 1
        time.sleep_ms(self.RECONNECT_INTERVAL_MS)

        if self._server_addr:
            try:
                self.connect(self._server_addr)
            except Exception as e:
                if self.callbacks:
                    self.callbacks.on_error("reconnect_error", str(e))

    def check_connection_health(self):
        now = time.ticks_ms()

        if self.is_connected:
            elapsed_since_activity = time.ticks_diff(now, self._last_activity_time)
            if elapsed_since_activity > self.CONNECTION_IDLE_TIMEOUT_MS:
                if self.callbacks:
                    self.callbacks.on_error("connection_idle",
                                            "连接空闲超时, 可能已断开")
                self.disconnect()
                return False

            elapsed_since_ping = time.ticks_diff(now, self._ping_timer)
            if elapsed_since_ping >= self.PING_INTERVAL_MS:
                self._send_ping()
                self._ping_timer = now

        self._check_write_timeout()
        return True

    def _send_ping(self):
        if not self.is_connected or not self._tx_handle:
            return

        try:
            ping_data = bytes([0x01, 0x00, 0x00, 0x00])
            self.ble.gattc_write(self._conn_id, self._tx_handle, ping_data)
        except Exception:
            pass

    def update_activity(self):
        self._last_activity_time = time.ticks_ms()

    def add_rssi(self, rssi: int):
        self._rssi_history.append(rssi)
        if len(self._rssi_history) > self._max_rssi_history:
            self._rssi_history.pop(0)

    def get_average_rssi(self) -> int:
        if not self._rssi_history:
            return 0
        return sum(self._rssi_history) // len(self._rssi_history)

    def get_connection_stats(self) -> dict:
        return {
            'state': self._state,
            'reconnect_attempts': self._reconnect_attempts,
            'average_rssi': self.get_average_rssi(),
            'write_queue_size': len(self._write_queue)
        }

    def enqueue_write(self, packet):
        self._write_queue.append(packet)
        if not self._is_processing_queue:
            self._process_write_queue()

    def _process_write_queue(self):
        if not self._write_queue:
            self._is_processing_queue = False
            return

        self._is_processing_queue = True
        packet = self._write_queue.pop(0)

        success, response, error_code = self.write_packet(packet)

        if not success and error_code == 0x0101:
            self._write_queue.insert(0, packet)
            time.sleep_ms(100)

        if self._write_queue:
            self._process_write_queue()
        else:
            self._is_processing_queue = False