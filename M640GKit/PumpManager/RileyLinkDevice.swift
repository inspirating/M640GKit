//
//  RileyLinkDevice.swift
//  Loop
//
//  Copyright © 2017 LoopKit Authors. All rights reserved.
//

import HealthKit
import ESP32Kit


extension ESP32DeviceStatus {
    func device(pumpID: String, pumpModel: PumpModel) -> HKDevice {
        return HKDevice(
            name: name,
            manufacturer: "Medtronic",
            model: pumpModel.rawValue,
            hardwareVersion: nil,
            firmwareVersion: firmwareVersion,
            softwareVersion: String(M640GKitVersionNumber),
            localIdentifier: pumpID,
            udiDeviceIdentifier: nil
        )
    }
}
