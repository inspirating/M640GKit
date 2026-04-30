//
//  M640GPumpUICoordinator.swift
//  M640GKitUI
//
//  Created by Pete Schwamb on 11/29/22.
//  Copyright © 2022 LoopKit Authors. All rights reserved.
//

import Foundation
import LoopKit
import LoopKitUI
import M640GKit
import RileyLinkBLEKit
import RileyLinkKit
import RileyLinkKitUI
import SwiftUI

enum M640GUIScreen {
    case settings

    func next() -> M640GUIScreen? {
        switch self {
        case .settings:
            return nil
        }
    }
}

class M640GUICoordinator: UINavigationController, PumpManagerOnboarding, CompletionNotifying, UINavigationControllerDelegate {

    public weak var pumpManagerOnboardingDelegate: PumpManagerOnboardingDelegate?

    public weak var completionDelegate: CompletionDelegate?

    private let colorPalette: LoopUIColorPalette

    private var allowedInsulinTypes: [InsulinType]

    private var allowDebugFeatures: Bool

    var pumpManager: M640GPumpManager

    var screenStack = [M640GUIScreen]()

    var currentScreen: M640GUIScreen {
        return screenStack.last!
    }

    init(pumpManager: M640GPumpManager? = nil, colorPalette: LoopUIColorPalette, pumpManagerSettings: PumpManagerSetupSettings? = nil, allowDebugFeatures: Bool, allowedInsulinTypes: [InsulinType] = [])
    {
        if pumpManager == nil, let pumpManagerSettings = pumpManagerSettings {
            let basalSchedule = pumpManagerSettings.basalSchedule

            let deviceProvider = RileyLinkBluetoothDeviceProvider(autoConnectIDs: [])

            let pumpManagerState = M640GPumpManagerState(
                isOnboarded: false,
                useMySentry: true, // TODO
                pumpColor: .blue, // TODO
                pumpID: "111111", // TODO
                pumpModel: .model508, // TODO
                pumpFirmwareVersion: "1.11", // TODO
                pumpRegion: .northAmerica, // TODO
                rileyLinkConnectionState: nil,
                timeZone: basalSchedule.timeZone,
                suspendState: .resumed(Date()), // TODO
                insulinType: .novolog, // TODO
                lastTuned: nil,
                lastValidFrequency: nil,
                basalSchedule: BasalSchedule(repeatingScheduleValues: pumpManagerSettings.basalSchedule.items)
            )

            self.pumpManager = M640GPumpManager(state: pumpManagerState, rileyLinkDeviceProvider: deviceProvider)
        } else {
            guard let pumpManager = pumpManager else {
                fatalError("Unable to create M640G PumpManager")
            }
            self.pumpManager = pumpManager
        }

        self.colorPalette = colorPalette

        self.allowDebugFeatures = allowDebugFeatures

        self.allowedInsulinTypes = allowedInsulinTypes

        super.init(navigationBarClass: UINavigationBar.self, toolbarClass: UIToolbar.self)
    }

    required init?(coder aDecoder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        self.navigationBar.prefersLargeTitles = true
        delegate = self
    }

    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated)

        if screenStack.isEmpty {
            screenStack = [determineInitialStep()]
            let viewController = viewControllerForScreen(currentScreen)
            viewController.isModalInPresentation = false
            setViewControllers([viewController], animated: false)
        }
    }

    private func determineInitialStep() -> M640GUIScreen {
        return .settings
    }

    private func viewControllerForScreen(_ screen: M640GUIScreen) -> UIViewController {
        switch screen {
        case .settings:
            let viewModel = M640GPumpSettingsViewModel(pumpManager: pumpManager)
            viewModel.didFinish = { [weak self] in
                self?.stepFinished()
            }

            let rileyLinkListDataSource = RileyLinkListDataSource(rileyLinkPumpManager: pumpManager)

            let handleRileyLinkSelection = { [weak self] (device: RileyLinkDevice) in
                if let self = self {
                    let vc = RileyLinkDeviceTableViewController(
                        device: device,
                        batteryAlertLevel: self.pumpManager.rileyLinkBatteryAlertLevel,
                        batteryAlertLevelChanged: { [weak self] value in
                            self?.pumpManager.rileyLinkBatteryAlertLevel = value
                        }
                    )
                    self.show(vc, sender: self)
                }
            }

            let view = M640GPumpSettingsView(viewModel: viewModel, supportedInsulinTypes: allowedInsulinTypes, handleRileyLinkSelection: handleRileyLinkSelection, rileyLinkListDataSource: rileyLinkListDataSource)
            let controller = hostingController(rootView: view)
            controller.navigationItem.title = String(format: NSLocalizedString("Medtronic %1$@", comment: "Format string fof navigation bar title for M640GPumpSettingsView (1: model number)"), pumpManager.state.pumpModel.description)
            return controller
        }
    }

    private func hostingController<Content: View>(rootView: Content) -> DismissibleHostingController<some View> {
        return DismissibleHostingController(content: rootView, colorPalette: colorPalette)
    }

    private func stepFinished() {
        if let nextStep = currentScreen.next() {
            navigateTo(nextStep)
        } else {
            completionDelegate?.completionNotifyingDidComplete(self)
        }
    }

    func navigateTo(_ screen: M640GUIScreen) {
        screenStack.append(screen)
        let viewController = viewControllerForScreen(screen)
        viewController.isModalInPresentation = false
        self.pushViewController(viewController, animated: true)
        viewController.view.layoutSubviews()
    }

}
