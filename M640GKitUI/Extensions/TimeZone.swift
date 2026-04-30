//
//  TimeZone.swift
//  M640GKitUI
//
//  Created by Pete Schwamb on 3/19/23.
//  Copyright © 2023 LoopKit Authors. All rights reserved.
//

import Foundation

extension TimeZone {
    static var currentFixed: TimeZone {
        return TimeZone(secondsFromGMT: TimeZone.current.secondsFromGMT())!
    }

    var fixed: TimeZone {
        return TimeZone(secondsFromGMT: secondsFromGMT())!
    }
}
