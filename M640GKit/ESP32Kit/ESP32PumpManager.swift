//
//  ESP32PumpManager.swift
//  M640GKit
//
//  Created by ESP32 on 2026-05-02
//  Copyright © 2026 LoopKit Authors. All rights reserved.
//

import Foundation
import LoopKit

public protocol ESP32PumpManagerDelegate: PumpManagerDelegate {
    func pumpManager(_ pumpManager: ESP32PumpManager, didUpdate state: PumpManagerStatus, oldState: PumpManagerStatus)
}

public class ESP32PumpManager: PumpManager {
    public var pumpManagerDelegate: PumpManagerDelegate? {
        get { return delegate }
        set { delegate = newValue }
    }

    private weak var delegate: PumpManagerDelegate?

    public var pumpOps: PumpOps?

    public var esp32ConnectionManagerState: ESP32ConnectionState? {
        get { return esp32DeviceProvider != nil ? ESP32ConnectionState(autoConnectIDs: esp32DeviceProvider!.autoConnectIDs) : nil }
        set { }
    }

    public var rileyLinkConnectionManagerState: RileyLinkConnectionState? {
        get {
            guard let provider = esp32DeviceProvider else { return nil }
            return RileyLinkConnectionState(autoConnectIDs: provider.autoConnectIDs)
        }
        set { }
    }

    public var rileyLinkBatteryAlertLevel: Int? {
        get { return nil }
        set { }
    }

    public var esp32DeviceProvider: ESP32BluetoothDeviceProvider?

    public init(esp32DeviceProvider: ESP32BluetoothDeviceProvider) {
        self.esp32DeviceProvider = esp32DeviceProvider
    }

    public var status: PumpManagerStatus {
        return PumpManagerStatus()
    }

    public func createPumpStatusHighlight(from state: PumpManagerStatus, at date: Date, ifNeeded: Bool) -> PumpStatusHighlight? {
        return nil
    }

    public func createDeviceStatus(from state: PumpManagerStatus, and date: Date, resultBy: @escaping (DeviceStatus?) -> Void) {
        resultBy(nil)
    }

    // MARK: - Device Communication

    public func device(_ device: ESP32Device, didReceivePacket packet: RFPacket) {
    }

    public func deviceTimerDidTick(_ device: ESP32Device) {
    }

    public func device(_ device: ESP32Device, didUpdateBattery level: Int) {
    }

    // MARK: - RileyLinkPumpManager Compatibility

    public var isPumpDataStale: Bool {
        return false
    }

    public func acknowledgeAlert(alertIdentifier: Alert.AlertIdentifier, completion: @escaping (Error?) -> Void) {
        completion(nil)
    }

    public func clearUpdatedAlertIssue(alertIdentifier: Alert.AlertIdentifier, completion: @escaping (Error?) -> Void) {
        completion(nil)
    }

    public func dismissAlert(alertIdentifier: Alert.AlertIdentifier, completion: @escaping (Error?) -> Void) {
        completion(nil)
    }

    public func getCurrentPumpManagerState() -> PumpManagerState {
        return PumpManagerState()
    }

    public var lastLoopDate: Date? {
        return nil
    }

    public func updateBLEHeartbeatPreference() {
    }

    public var shouldProvideBLEHeartbeat: Bool {
        return false
    }

    public func issueAlert(_ alert: Alert) {
    }

    public func retractAlert(identifier: Alert.Identifier) {
    }

    public static func pluginIdentifier() -> String {
        return "ESP32PumpManager"
    }

    public func deliverVolume(_ units: Double, overMinutes: Int, callback: @escaping (Error?) -> Void) {
        callback(nil)
    }

    public func setMaximumBasalRate(_ unitsPerHour: Double, callback: @escaping (Error?) -> Void) {
        callback(nil)
    }

    public func setMaximumBolus(_ units: Double, callback: @escaping (Error?) -> Void) {
        callback(nil)
    }

    public func setSuspended(_ suspended: Bool, callback: @escaping (Error?) -> Void) {
        callback(nil)
    }

    public func resumeDelivery(callback: @escaping (Error?) -> Void) {
        callback(nil)
    }

    public func syncBasalSchedule(basaldose: [BasalScheduleEntry], callback: @escaping (Error?) -> Void) {
        callback(nil)
    }

    public func syncDeliverySettings(ls: LocalStorage, pumpSettings: PumpSettings, pumpState: PumpState, lastPumpEvents: [NewPumpEvent], resultBy: @escaping (Result<Void, Error>) -> Void) {
        resultBy(.success(()))
    }

    public func syncWaveform(ls: LocalStorage, events: [PumpEvent, Any], fromDate: Date, callback: @escaping (Error?) -> Void) {
        callback(nil)
    }

    public func toggleRemoteWakeup(_ enabled: Bool) {
    }
}
