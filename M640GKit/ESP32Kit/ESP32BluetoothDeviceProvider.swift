//
//  ESP32BluetoothDeviceProvider.swift
//  M640GKit
//
//  Created by ESP32 on 2026-05-02
//  Copyright © 2026 LoopKit Authors. All rights reserved.
//

import Foundation
import CoreBluetooth
import LoopKit

public enum ConnectionState: String {
    case disconnected
    case connecting
    case connected
}

public protocol ESP32BluetoothDeviceProviderDelegate: AnyObject {
    func bluetoothDeviceProvider(_ provider: ESP32BluetoothDeviceProvider, didDiscover device: ESP32Device)
    func bluetoothDeviceProvider(_ provider: ESP32BluetoothDeviceProvider, didConnect device: ESP32Device)
    func bluetoothDeviceProvider(_ provider: ESP32BluetoothDeviceProvider, didDisconnect device: ESP32Device)
    func bluetoothDeviceProviderDidUpdateState(_ provider: ESP32BluetoothDeviceProvider)
}

public class ESP32BluetoothDeviceProvider: NSObject {
    public weak var delegate: ESP32BluetoothDeviceProviderDelegate?

    public private(set) var state: CBManagerState = .unknown

    public var autoConnectIDs: Set<UUID>

    public private(set) var discoveredDevices: [ESP32Device] = []

    public private(set) var connectedDevices: [ESP32Device] = []

    public var idleListeningEnabled: Bool = false

    public var idleListeningState: IdleListeningState = .disabled

    public var timerTickEnabled: Bool = false

    public init(autoConnectIDs: Set<UUID> = []) {
        self.autoConnectIDs = autoConnectIDs
        super.init()
    }

    public func startScanning() {
    }

    public func stopScanning() {
    }

    public func connect(_ device: ESP32Device) {
    }

    public func disconnect(_ device: ESP32Device) {
    }

    public func getDevices(_ completion: @escaping ([ESP32Device]) -> Void) {
        completion(connectedDevices)
    }

    public func deprioritize(_ device: ESP32Device, completion: (() -> Void)?) {
        completion?()
    }

    public func assertIdleListening(forcingRestart: Bool) {
    }

    public var firstConnectedDevice: (@escaping (ESP32Device?) -> Void) -> Void {
        return { completion in
            completion(self.connectedDevices.first)
        }
    }
}

extension ESP32BluetoothDeviceProvider: CBCentralManagerDelegate {
    public func centralManagerDidUpdateState(_ central: CBCentralManager) {
        state = central.state
        delegate?.bluetoothDeviceProviderDidUpdateState(self)
    }

    public func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String : Any], rssi RSSI: NSNumber) {
    }

    public func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
    }

    public func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
    }
}
