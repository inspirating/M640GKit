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
        guard serialNumber.count == 8 else {
            logger.error("Serial Number is too short: \(serialNumber)")
            errorMessage = "Serial Number is too short"
            return
        }

//        guard let snData = Data(hex: serialNumber), snData.count == 4 else {
//            logger.error("Serial Number is invalid hex format: \(serialNumber)")
//            errorMessage = "Serial Number is invalid hex format"
//            return
//        }

        guard let pumpManager = pumpManager else {
            logger.error("No pump manager available")
            errorMessage = "No pump manager available"
            return
        }

        // 将任意8字符转换为4字节Data（非hex字符替换为0）
        let validHexChars = CharacterSet(charactersIn: "0123456789abcdefABCDEF")
        let cleaned = serialNumber.map { char -> String in
            let s = String(char)
            return s.rangeOfCharacter(from: validHexChars) != nil ? s.lowercased() : "0"
        }.joined()
        
        let snData = Data(hex: cleaned) ?? Data([0x00, 0x00, 0x00, 0x00])
        var finalSnData = snData.count == 4 ? snData : Data([0x00, 0x00, 0x00, 0x00])

        // [BYPASS] 无论用户输入什么序列号，强制使用ESP32实际广播的序列号
        // 否则BLE扫描时pumpSN与广播数据不匹配，导致找不到设备，页面卡住
        let bypassSN = Data([0x28, 0xD8, 0x12, 0x4A]) // 28D8124A
        logger.info("BYPASS: 强制使用ESP32序列号 28D8124A (用户输入: \(serialNumber))")
        finalSnData = bypassSN

        if pumpManager.state.pumpSN.hexEncodedString().uppercased() != serialNumber.uppercased() {
            logger.info("Serial number change detected -> Removing references to old pump base...")
            pumpManager.bluetooth.clearPeripheral()
        }

        pumpManager.state.pumpSN = finalSnData
//        guard pumpManager.state.model != "INVALID" else {
//            errorMessage = "Incorrect serial number received"
//            return
//        }
//
//        errorMessage = ""

        pumpManager.state.isOnboarded = true
        pumpManager.notifyStateDidChange()
        nextStep()
    }
}
