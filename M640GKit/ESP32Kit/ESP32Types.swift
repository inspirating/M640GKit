//
//  ESP32Types.swift
//  M640GKit
//
//  Created by ESP32 on 2026-05-02
//  Copyright © 2026 LoopKit Authors. All rights reserved.
//

import Foundation
import CoreBluetooth
import LoopKit

public enum CC111XRegister: UInt8 {
    case IOCFG0 = 0x00
    case IOCFG1 = 0x01
    case IOCFG2 = 0x02
    case SYNC1 = 0x04
    case SYNC0 = 0x05
    case PKTLEN = 0x06
    case PKTCTRL1 = 0x07
    case PKTCTRL0 = 0x08
    case ADDR = 0x09
    case CHANNR = 0x0A
    case FSCTRL1 = 0x0B
    case FSCTRL0 = 0x0C
    case FREQ2 = 0x0D
    case FREQ1 = 0x0E
    case FREQ0 = 0x0F
    case MDMCFG4 = 0x10
    case MDMCFG3 = 0x11
    case MDMCFG2 = 0x12
    case MDMCFG1 = 0x13
    case MDMCFG0 = 0x14
    case DEVIATN = 0x15
    case MCSM2 = 0x17
    case MCSM1 = 0x18
    case MCSM0 = 0x19
    case FOCCFG = 0x1A
    case BSCFG = 0x1B
    case AGCCTRL2 = 0x1C
    case AGCCTRL1 = 0x1D
    case AGCCTRL0 = 0x1E
    case BarkerTh = 0x1F
    case HT12 = 0x20
    case HT21 = 0x21
    case FREND1 = 0x22
    case FREND0 = 0x23
    case FSCAL3 = 0x23
    case FSCAL2 = 0x24
    case FSCAL1 = 0x25
    case FSCAL0 = 0x26
    case FSTEST = 0x29
    case PTEST = 0x2A
    case AGCTEST = 0x2B
    case TEST2 = 0x2C
    case TEST1 = 0x2D
    case TEST0 = 0x2E
    case RCCTRL1 = 0x2F
    case RCCTRL0 = 0x30
    case FSTDLY1 = 0x31
    case FSTDLY0 = 0x32
    case STSTDLY1 = 0x33
    case STSTDLY0 = 0x34
    case RSVD35 = 0x35
    case RSVD36 = 0x36
    case RSVD37 = 0x37
    case RSVD38 = 0x38
    case RSVD39 = 0x39
    case RSVD3A = 0x3A
    case PTEST1 = 0x3B
    case PTEST2 = 0x3C
    case RSVD3D = 0x3D
    case RSVD3E = 0x3E
    case RSVD3F = 0x3F
}

public struct RFPacket {
    public let data: Data
    public let rssi: Int
    public let timestamp: Date

    public init(data: Data, rssi: Int = 0, timestamp: Date = Date()) {
        self.data = data
        self.rssi = rssi
        self.timestamp = timestamp
    }
}

public struct RileyLinkStatistics {
    public var transmitAttemptCount: Int = 0
    public var transmitSuccessCount: Int = 0
    public var receiveAttemptCount: Int = 0
    public var receiveSuccessCount: Int = 0
    public var noise: Double = 0
    public var resetCount: Int = 0

    public init() {}
}

public struct RileyLinkConnectionState: RawRepresentable {
    public typealias RawValue = [String: Any]

    public var autoConnectIDs: Set<UUID>
    public var connectionState: ConnectionState

    public init(autoConnectIDs: Set<UUID> = [], connectionState: ConnectionState = .disconnected) {
        self.autoConnectIDs = autoConnectIDs
        self.connectionState = connectionState
    }

    public init?(rawValue: RawValue) {
        guard let autoConnectIDsStrings = rawValue["autoConnectIDs"] as? [String]
        else {
            return nil
        }
        self.autoConnectIDs = Set(autoConnectIDsStrings.compactMap { UUID(uuidString: $0) })
        self.connectionState = .disconnected
    }

    public var rawValue: RawValue {
        return [
            "autoConnectIDs": autoConnectIDs.map { $0.uuidString }
        ]
    }
}

public enum ConnectionState: String {
    case disconnected
    case connecting
    case connected
}
