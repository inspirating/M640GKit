import LoopKit
import LoopKitUI
import SwiftUI
import UIKit

internal class M640GKitHUDProvider: NSObject, HUDProvider {
    var managerIdentifier: String {
        pumpManager.managerIdentifier
    }

    private let pumpManager: M640GKitPumpManager

    private var reservoirView: M640GKitReservoirView?

    private let bluetoothProvider: BluetoothProvider

    private let colorPalette: LoopUIColorPalette

    private var refreshTimer: Timer?

    private let allowedInsulinTypes: [InsulinType]

    var visible: Bool = true {
        didSet {
            if oldValue != visible, visible {
                hudDidAppear()
            }
        }
    }

    public init(
        pumpManager: M640GKitPumpManager,
        bluetoothProvider: BluetoothProvider,
        colorPalette: LoopUIColorPalette,
        allowedInsulinTypes: [InsulinType]
    ) {
        self.pumpManager = pumpManager
        self.bluetoothProvider = bluetoothProvider
        self.colorPalette = colorPalette
        self.allowedInsulinTypes = allowedInsulinTypes
        super.init()
        self.pumpManager.addStateObserver(self, queue: .main)
    }

    public func createHUDView() -> BaseHUDView? {
        reservoirView = M640GKitReservoirView.instantiate()
        updateReservoirView()

        return reservoirView
    }

    public var hudViewRawState: HUDProvider.HUDViewRawState {
        var rawValue: HUDProvider.HUDViewRawState = [:]

        rawValue["lastStatusDate"] = pumpManager.rawState["lastStatusDate"]
        rawValue["reservoirLevel"] = pumpManager.rawState["reservoirLevel"]

        return rawValue
    }

    public func didTapOnHUDView(_: BaseHUDView, allowDebugFeatures _: Bool) -> HUDTapAction? {
        nil
    }

    private func hudDidAppear() {
        updateReservoirView()
        pumpManager.ensureCurrentPumpData { _ in
            DispatchQueue.main.async {
                self.updateReservoirView()
            }
        }
    }

    public static func createHUDView(rawValue: HUDProvider.HUDViewRawState) -> BaseHUDView? {
        let reservoirView: M640GKitReservoirView?

        if let lastStatusDate = rawValue["lastStatusDate"] as? Date {
            reservoirView = M640GKitReservoirView.instantiate()
            reservoirView!.update(level: rawValue["reservoirLevel"] as? Double, at: lastStatusDate)
        } else {
            reservoirView = nil
        }

        return reservoirView
    }

    private func updateReservoirView() {
        guard let reservoirView = reservoirView,
              let lastStatusDate = pumpManager.rawState["lastStatusDate"] as? Date
        else {
            return
        }

        reservoirView.update(level: pumpManager.rawState["reservoirLevel"] as? Double, at: lastStatusDate)
    }
}

extension M640GKitHUDProvider: StateObserver {
    func deviceScanDidUpdate(_: DanaPumpScan) {
        // Ble scan not needed here
    }

    func stateDidUpdate(_ state: M640GKitPumpManagerState, _: M640GKitPumpManagerState) {
        updateReservoirView()

        visible = state.deviceName != nil
    }
}
