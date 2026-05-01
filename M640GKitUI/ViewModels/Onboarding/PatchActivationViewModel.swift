class PatchActivationViewModel: ObservableObject {
    @Published var isActivating: Bool = false
    @Published var activationError: String = ""

    private let pumpManager: M640GKitPumpManager?
    private let nextStep: () -> Void
    let previousStep: () -> Void
    init(_ pumpManager: M640GKitPumpManager?, _ nextStep: @escaping () -> Void, _ previousStep: @escaping () -> Void) {
        self.pumpManager = pumpManager
        self.nextStep = nextStep
        self.previousStep = previousStep
    }

    func activate() {
        #if targetEnvironment(simulator)
            if let pumpManager = pumpManager {
                // Add some mock data
                pumpManager.state.patchId = Data([1, 2, 3, 4])
                pumpManager.state.initialReservoir = nil
                pumpManager.state.reservoir = 200
                pumpManager.state.battery = 2.5
                pumpManager.state.pumpState = .active
                pumpManager.state.patchActivatedAt = Date.now
                pumpManager.state.lastSync = Date.now
                pumpManager.notifyStateDidChange()
            }

            nextStep()
        #else
            guard let pumpManager = self.pumpManager else {
                nextStep()
                return
            }

            isActivating = true
            activationError = ""
            pumpManager.activatePatch { result in
                DispatchQueue.main.async {
                    if case let .failure(error) = result {
                        self.activationError = error.localizedDescription
                        self.isActivating = false
                        return
                    }

                    self.nextStep()
                }
            }
        #endif
    }
}
