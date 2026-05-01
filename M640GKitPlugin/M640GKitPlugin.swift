import M640GKit
import Foundation
import LoopKitUI
import os.log

class M640GKitPlugin: NSObject, PumpManagerUIPlugin {
    private let log = OSLog(category: "M640GKitPlugin")

    public var pumpManagerType: PumpManagerUI.Type? {
        M640GKitPumpManager.self
    }

    public var cgmManagerType: CGMManagerUI.Type? {
        nil
    }

    override init() {
        super.init()
        log.default("M640GKitPlugin Instantiated")
    }
}
