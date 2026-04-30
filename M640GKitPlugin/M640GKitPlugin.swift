//
//  M640GKitPlugin.swift
//  M640GKitPlugin
//
//  Created by Pete Schwamb on 8/24/19.
//  Copyright © 2019 LoopKit Authors. All rights reserved.
//

import os.log
import LoopKitUI
import M640GKit
import M640GKitUI

class M640GKitPlugin: NSObject, PumpManagerUIPlugin {
    private let log = OSLog(category: "M640GKitPlugin")
    
    public var pumpManagerType: PumpManagerUI.Type? {
        return M640GPumpManager.self
    }
    
    override init() {
        super.init()
        log.default("Instantiated")
    }
}
