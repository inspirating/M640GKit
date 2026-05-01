//
//  ESP32Device.swift
//  M640GKit
//
//  Created by ESP32 on 2026-05-02
//  Copyright © 2026 LoopKit Authors. All rights reserved.
//

import Foundation
import LoopKit

public protocol ESP32DeviceDelegate: AnyObject {
    func device(_ device: ESP32Device, didUpdateBattery level: Int)
    func deviceTimerDidTick(_ device: ESP32Device)
}

public class ESP32Device {
    public let peripheralIdentifier: UUID
    public var name: String?
    public var version: String = "Unknown"
    public weak var delegate: ESP32DeviceDelegate?

    private let sessionQueue = DispatchQueue(label: "com.M640GKit.esp32device.sessionQueue")

    public init(peripheralIdentifier: UUID, name: String?) {
        self.peripheralIdentifier = peripheralIdentifier
        self.name = name
    }

    public func assertOnSessionQueue() {
    }

    public func sessionQueueAsync(execute block: @escaping () -> Void) {
        sessionQueue.async {
            block()
        }
    }

    public func sessionQueueAsyncAfter(deadline: DispatchTime, execute block: @escaping () -> Void) {
        sessionQueue.asyncAfter(deadline: deadline) {
            block()
        }
    }

    public var deviceURI: URL {
        return URL(string: "esp32://\(peripheralIdentifier.uuidString)")!
    }

    public func runSession(withName name: String, completion: @escaping (CommandSession) -> Void) {
        sessionQueue.async {
            let session = CommandSession()
            completion(session)
        }
    }

    public func updateBatteryLevel() {
    }
}

public class CommandSession {
    public var firmwareVersion: String = "Unknown"

    public init() {}

    public func resetRadioConfig() throws {
    }

    public func updateRegister(_ address: CC111XRegister, value: UInt8) throws {
    }

    public func setBaseFrequency(_ frequency: Measurement<UnitFrequency>) throws {
    }

    public func listen(onChannel channel: Int, timeout: TimeInterval) throws -> RFPacket? {
        return nil
    }

    public func send(_ data: Data, onChannel channel: Int, timeout: TimeInterval) throws {
    }

    public func getRileyLinkStatistics() throws -> RileyLinkStatistics {
        return RileyLinkStatistics()
    }
}

public struct ESP32DeviceStatus {
    public let name: String?
    public let firmwareVersion: String?
    public let RSSI: Int?
    public let battery: Int?
    public let lastCommunication: Date?
    public let connectionState: ESP32ConnectionState

    public init(name: String?, firmwareVersion: String?, RSSI: Int?, battery: Int?, lastCommunication: Date?, connectionState: ESP32ConnectionState) {
        self.name = name
        self.firmwareVersion = firmwareVersion
        self.RSSI = RSSI
        self.battery = battery
        self.lastCommunication = lastCommunication
        self.connectionState = connectionState
    }
}

public struct ESP32DeviceVersionInfo {
    public let firmwareVersion: String
    public let hardwareVersion: String?
    public let firmwareDate: Date?

    public init(firmwareVersion: String, hardwareVersion: String?, firmwareDate: Date?) {
        self.firmwareVersion = firmwareVersion
        self.hardwareVersion = hardwareVersion
        self.firmwareDate = firmwareDate
    }
}

public struct ESP32ConnectionState: RawRepresentable {
    public typealias RawValue = [String: Any]

    public var autoConnectIDs: Set<UUID>
    public var connectionState: ESP32BluetoothDeviceProvider.ConnectionState

    public init(autoConnectIDs: Set<UUID> = [], connectionState: ESP32BluetoothDeviceProvider.ConnectionState = .disconnected) {
        self.autoConnectIDs = autoConnectIDs
        self.connectionState = connectionState
    }

    public init?(rawValue: RawValue) {
        guard let autoConnectIDsStrings = rawValue["autoConnectIDs"] as? [String],
              let connectionStateRaw = rawValue["connectionState"] as? String
        else {
            return nil
        }
        self.autoConnectIDs = Set(autoConnectIDsStrings.compactMap { UUID(uuidString: $0) })
        self.connectionState = ESP32BluetoothDeviceProvider.ConnectionState(rawValue: connectionStateRaw) ?? .disconnected
    }

    public var rawValue: RawValue {
        return [
            "autoConnectIDs": autoConnectIDs.map { $0.uuidString },
            "connectionState": connectionState.rawValue
        ]
    }
}

public struct ESP32Statistics {
    public var transmitAttemptCount: Int = 0
    public var transmitSuccessCount: Int = 0
    public var receiveAttemptCount: Int = 0
    public var receiveSuccessCount: Int = 0
    public var noise: Double = 0
    public var resetCount: Int = 0

    public init() {}
}
