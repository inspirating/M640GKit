//
//  M640GPumpManagerRecents.swift
//  M640GKit
//
//  Copyright © 2019 LoopKit Authors. All rights reserved.
//

import Foundation
import LoopKit

public struct M640GPumpManagerRecents: Equatable {

    internal enum EngageablePumpState: Equatable {
        case engaging
        case disengaging
        case stable
    }

    internal var suspendEngageState: EngageablePumpState = .stable

    internal var bolusEngageState: EngageablePumpState = .stable

    internal var tempBasalEngageState: EngageablePumpState = .stable

    var lastAddedPumpEvents: Date = .distantPast
    
    var lastContinuousReservoir: Date = .distantPast

    var latestPumpStatus: PumpStatus? = nil

    var latestPumpStatusFromMySentry: MySentryPumpStatusMessageBody? = nil {
        didSet {
            if let sensorState = latestPumpStatusFromMySentry {
                self.sensorState = EnliteSensorDisplayable(sensorState)
            }
        }
    }

    var sensorState: EnliteSensorDisplayable? = nil
}

extension M640GPumpManagerRecents: CustomDebugStringConvertible {
    public var debugDescription: String {
        return """
        ### M640GPumpManagerRecents
        suspendEngageState: \(suspendEngageState)
        bolusEngageState: \(bolusEngageState)
        tempBasalEngageState: \(tempBasalEngageState)
        lastAddedPumpEvents: \(lastAddedPumpEvents)
        latestPumpStatus: \(String(describing: latestPumpStatus))
        lastContinuousReservoir: \(lastContinuousReservoir)
        latestPumpStatusFromMySentry: \(String(describing: latestPumpStatusFromMySentry))
        sensorState: \(String(describing: sensorState))
        """
    }
}
