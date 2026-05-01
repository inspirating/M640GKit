//
//  ESP32DeviceProvider.swift
//  M640GKit
//
//  Created by ESP32 on 2026-05-02
//  Copyright © 2026 LoopKit Authors. All rights reserved.
//

import Foundation
import LoopKit

public protocol ESP32DeviceProviderDelegate: AnyObject {
    func deviceProvider(_ provider: ESP32DeviceProvider, didConnect device: ESP32Device)
    func deviceProvider(_ provider: ESP32DeviceProvider, didDisconnect device: ESP32Device)
    func deviceProvider(_ provider: ESP32DeviceProvider, didUpdateState state: ESP32DeviceProviderState)
}

public enum ESP32DeviceProviderState {
    case disconnected
    case connecting
    case connected
}

public enum IdleListeningState {
    case disabled
    case enabled(timeout: TimeInterval, channel: Int)

    public static let disabled = IdleListeningState.disabled
}

public class ESP32DeviceProvider {
    public weak var delegate: ESP32DeviceProviderDelegate?

    public private(set) var state: ESP32DeviceProviderState = .disconnected

    public var idleListeningEnabled: Bool = false

    public var idleListeningState: IdleListeningState = .disabled

    public var timerTickEnabled: Bool = false

    public var autoConnectIDs: Set<UUID>

    public private(set) var devices: [ESP32Device] = []

    public init(autoConnectIDs: Set<UUID> = []) {
        self.autoConnectIDs = autoConnectIDs
    }

    public func getDevices(_ completion: @escaping ([ESP32Device]) -> Void) {
        completion(devices)
    }

    public func connect(_ device: ESP32Device) {
        delegate?.deviceProvider(self, didConnect: device)
    }

    public func disconnect(_ device: ESP32Device) {
        delegate?.deviceProvider(self, didDisconnect: device)
    }

    public func deprioritize(_ device: ESP32Device, completion: (() -> Void)?) {
        completion?()
    }

    public var firstConnectedDevice: (@escaping (ESP32Device?) -> Void) -> Void {
        return { completion in
            completion(self.devices.first)
        }
    }
}
