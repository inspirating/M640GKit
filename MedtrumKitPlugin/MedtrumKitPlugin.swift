import LoopKitUI
import MedtrumKit

class MedtrumKitPlugin: NSObject, PumpManagerUIPlugin {
    public var pumpManagerType: PumpManagerUI.Type? {
        MedtrumPumpManager.self
    }

    public var cgmManagerType: CGMManagerUI.Type? {
        nil
    }

    override init() {
        super.init()
    }
}
