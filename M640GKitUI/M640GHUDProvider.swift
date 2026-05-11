import LoopKit
import LoopKitUI
import SwiftUI
import UIKit

internal class M640GKitHUDProvider: NSObject, HUDProvider {
    private let processQueue = DispatchQueue(label: "com.nightscout.M640GKit.hudProvider")
    private let pumpManager: M640GPumpManager

    private let bluetoothProvider: BluetoothProvider

    private let colorPalette: LoopUIColorPalette

    private let allowedInsulinTypes: [InsulinType]
    private var reservoirView: M640GReservoirView?

    var visible: Bool = true {
        didSet {
            if oldValue != visible, visible {
                hudDidAppear()
            }
        }
    }

    public init(
        pumpManager: M640GPumpManager,
        bluetoothProvider: BluetoothProvider,
        colorPalette: LoopUIColorPalette,
        allowedInsulinTypes: [InsulinType]
    ) {
        self.pumpManager = pumpManager
        self.bluetoothProvider = bluetoothProvider
        self.colorPalette = colorPalette
        self.allowedInsulinTypes = allowedInsulinTypes
        super.init()

        self.pumpManager.addStatusObserver(self, queue: processQueue)
    }

    public func createHUDView() -> BaseHUDView? {
        reservoirView = M640GReservoirView.instantiate()
        updateReservoirView()

        return reservoirView
    }

    func didTapOnHUDView(_: LoopKitUI.BaseHUDView, allowDebugFeatures _: Bool) -> LoopKitUI.HUDTapAction? {
        nil
    }

    var hudViewRawState: HUDViewRawState {
        var rawValue: HUDProvider.HUDViewRawState = [:]

        rawValue["lastSync"] = pumpManager.state.lastSync
        rawValue["reservoir"] = pumpManager.state.reservoir
        rawValue["maxReservoir"] = pumpManager.state.pumpName.contains("300u") ? 300 : 200
        return rawValue
    }

    var managerIdentifier: String {
        pumpManager.managerIdentifier
    }

    public static func createHUDView(rawValue: HUDProvider.HUDViewRawState) -> BaseHUDView? {
        let reservoirView: M640GReservoirView?

        if let lastStatusDate = rawValue["lastSync"] as? Date,
           let reservoir = rawValue["reservoir"] as? Double,
           let maxReservoir = rawValue["maxReservoir"] as? Double
        {
            reservoirView = M640GReservoirView.instantiate()
            reservoirView!.update(level: reservoir, at: lastStatusDate, max: maxReservoir)
        } else {
            reservoirView = nil
        }

        return reservoirView
    }

    private func hudDidAppear() {
        updateReservoirView()
        pumpManager.ensureCurrentPumpData { _ in
            self.updateReservoirView()
        }
    }

    private func updateReservoirView() {
        guard let reservoirView = reservoirView else {
            return
        }

        DispatchQueue.main.async {
            reservoirView.update(
                level: self.pumpManager.state.reservoir,
                at: self.pumpManager.state.lastSync,
                max: self.pumpManager.state.pumpName.contains("300u") ? 300 : 200
            )
        }
    }
}

extension M640GKitHUDProvider: PumpManagerStatusObserver {
    func pumpManager(_: any LoopKit.PumpManager, didUpdate _: LoopKit.PumpManagerStatus, oldStatus _: LoopKit.PumpManagerStatus) {
        updateReservoirView()
    }
}
