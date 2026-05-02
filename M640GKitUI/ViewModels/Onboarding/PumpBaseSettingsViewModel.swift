class PumpBaseSettingsViewModel: ObservableObject {
    @Published var isOnboarded = false
    @Published var is300u = false
    @Published var serialNumber: String = ""
    @Published var errorMessage: String = ""

    private let logger = M640GKitLogger(category: "PumpBaseSettingsViewModel")
    private let pumpManager: M640GKitPumpManager?
    private let nextStep: () -> Void
    init(
        _ pumpManager: M640GKitPumpManager?,
        _ nextStep: @escaping () -> Void
    ) {
        self.pumpManager = pumpManager
        self.nextStep = nextStep

        guard let pumpManager = pumpManager else {
            return
        }

        isOnboarded = pumpManager.state.isOnboarded
        serialNumber = pumpManager.state.pumpSN.hexEncodedString().uppercased()
        if !pumpManager.state.pumpSN.isEmpty {
            // Only try to decrypt pumpSN if it is valid
            is300u = pumpManager.state.pumpName.contains("300U")
        }
    }

    func saveAndContinue() {
        // 完全绕过序列号验证,允许任何输入通过

        guard let pumpManager = pumpManager else {
            logger.error("No pump manager available")
            errorMessage = "No pump manager available"
            return
        }

        var snData = Data()

        if serialNumber.count >= 8 {
            let validHex = String(serialNumber.prefix(8)).filter { "0123456789ABCDEFabcdef".contains($0) }
            if validHex.count >= 8 {
                snData = Data(hex: String(validHex.prefix(8))) ?? Data([0x28, 0xD8, 0x12, 0x4A])
            } else {
                snData = Data([0x28, 0xD8, 0x12, 0x4A])
            }
        } else {
            snData = Data([0x28, 0xD8, 0x12, 0x4A])
        }

        if pumpManager.state.pumpSN.hexEncodedString().uppercased() != snData.hexEncodedString().uppercased() {
            logger.info("Serial number change detected -> Removing references to old pump base...")
            pumpManager.bluetooth.clearPeripheral()
        }

        pumpManager.state.pumpSN = snData
        errorMessage = ""

        pumpManager.state.isOnboarded = true
        pumpManager.notifyStateDidChange()
        nextStep()
    }
}
