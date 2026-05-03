import Combine
import LoopKit
import LoopKitUI
import SwiftUI
import UIKit

enum M640GKitUIScreen {
    case welcomeScreen
    case insulinTypeScreen
    case patchSettingsScreen
    case deactivatePatchScreen
    case pumpBaseSettingsScreen
    case patchPrimingScreen
    case patchActivationScreen
    case settingsScreen
    case patchDetailsScreen
    case patchPreviousDetailsScreen
}

class M640GKitUICoordinator: UINavigationController, PumpManagerOnboarding, CompletionNotifying,
    UINavigationControllerDelegate
{
    private let colorPalette: LoopUIColorPalette
    private var pumpManager: M640GKitPumpManager?
    private var allowedInsulinTypes: [InsulinType]
    private var allowDebugFeatures: Bool
    private let logger = M640GKitLogger(category: "M640GKitUICoordinator")

    var pumpManagerOnboardingDelegate: (any LoopKitUI.PumpManagerOnboardingDelegate)?
    var completionDelegate: (any LoopKitUI.CompletionDelegate)?

    var screenStack = [M640GKitUIScreen]()
    var currentScreen: M640GKitUIScreen {
        screenStack.last!
    }

    init(
        pumpManager: M640GKitPumpManager? = nil,
        colorPalette: LoopUIColorPalette,
        pumpManagerSettings: PumpManagerSetupSettings? = nil,
        allowDebugFeatures: Bool,
        allowedInsulinTypes: [InsulinType] = []
    )
    {
        if pumpManager == nil, pumpManagerSettings == nil {
            self.pumpManager = M640GKitPumpManager(state: M640GKitPumpState(rawValue: [:]))
        } else if pumpManager == nil, let pumpManagerSettings = pumpManagerSettings {
            self.pumpManager = M640GKitPumpManager(state: M640GKitPumpState(pumpManagerSettings.basalSchedule))
        } else {
            self.pumpManager = pumpManager
        }

        self.colorPalette = colorPalette
        self.allowDebugFeatures = allowDebugFeatures
        self.allowedInsulinTypes = allowedInsulinTypes
        super.init(navigationBarClass: UINavigationBar.self, toolbarClass: UIToolbar.self)
    }

    @available(*, unavailable) required init?(coder _: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        navigationBar.prefersLargeTitles = true

        if screenStack.isEmpty {
            screenStack = getInitialScreens()

            let viewControllers = screenStack.map {
                let viewController = viewControllerForScreen($0)
                viewController.isModalInPresentation = false
                return viewController
            }

            setViewControllers(viewControllers, animated: false)
        }
    }

    func getInitialScreens() -> [M640GKitUIScreen] {
        guard let pumpManager = self.pumpManager else {
            return [.settingsScreen]
        }

        if !pumpManager.isOnboarded {
            return [.welcomeScreen]
        }

        if pumpManager.state.pumpState.rawValue < PatchState.priming.rawValue {
            return [.settingsScreen, .pumpBaseSettingsScreen]
        }

        if pumpManager.state.pumpState.rawValue < PatchState.primed.rawValue {
            return [.patchPrimingScreen]
        }

        if pumpManager.state.pumpState.rawValue < PatchState.active.rawValue {
            return [.patchActivationScreen]
        }

        return [.settingsScreen]
    }

    private func viewControllerForScreen(_ screen: M640GKitUIScreen) -> UIViewController {
        switch screen {
        case .welcomeScreen:
            return hostingController(rootView: OnboardingWelcomeView(nextStep: { self.navigateTo(.insulinTypeScreen) }))

        case .insulinTypeScreen:
            let nextStep: (InsulinType) -> Void = { insulinType in
                self.pumpManager?.state.insulinType = insulinType
                self.pumpManager?.notifyStateDidChange()

                if let pumpManager = self.pumpManager, pumpManager.isOnboarded {
                    return
                }

                self.navigateTo(.patchSettingsScreen)
            }
            return hostingController(rootView: InsulinTypeSelector(
                initialValue: pumpManager?.state.insulinType ?? allowedInsulinTypes[0],
                supportedInsulinTypes: allowedInsulinTypes,
                showSave: pumpManager?.isOnboarded ?? false,
                didConfirm: nextStep
            ))

        case .patchSettingsScreen:
            let nextStep = {
                if let pumpManager = self.pumpManager, pumpManager.isOnboarded {
                    return
                }

                self.navigateTo(.pumpBaseSettingsScreen)
            }
            let viewModel = PatchSettingsViewModel(
                pumpManager,
                updatePatch: pumpManager?.isOnboarded ?? false,
                nextStep: nextStep
            )

            var dirtyCheck = false
            if let pumpManager = pumpManager {
                dirtyCheck = !pumpManager.state.patchId.isEmpty
            }

            return hostingController(rootView: PatchSettingsView(
                viewModel: viewModel,
                doDirtyCheck: dirtyCheck
            ))

        case .deactivatePatchScreen:
            let nextStep = { self.resetNavigationTo([.settingsScreen, .pumpBaseSettingsScreen]) }
            let viewModel = DeactivatePatchViewModel(pumpManager, nextStep)
            return hostingController(rootView: PatchDeactivationView(viewModel: viewModel))

        case .pumpBaseSettingsScreen:
            let nextStep = {
                self.navigateTo(.patchPrimingScreen)

                if let pumpManager = self.pumpManager {
                    pumpManager.state.isOnboarded = true
                    pumpManager.notifyStateDidChange()

                    if let pumpManagerOnboardingDelegate = self.pumpManagerOnboardingDelegate {
                        pumpManagerOnboardingDelegate.pumpManagerOnboarding(didCreatePumpManager: pumpManager)
                    } else {
                        self.logger.warning("Not onboarded -> no onboardDelegate...")
                    }
                }
            }

            let viewModel = PumpBaseSettingsViewModel(pumpManager, nextStep)

            return hostingController(rootView: PumpBaseSettingsView(viewModel: viewModel))

        case .patchPrimingScreen:
            let viewModel = PatchPrimingViewModel(
                pumpManager,
                { self.resetNavigationTo([.patchActivationScreen]) },
                { self.navigateTo(.pumpBaseSettingsScreen) },
                { self.resetNavigationTo([.settingsScreen]) }
            )
            return hostingController(
                rootView: PatchPrimingView(viewModel: viewModel)
                    .onAppear { UIApplication.shared.isIdleTimerDisabled = true }
            )

        case .patchActivationScreen:
            let viewModel = PatchActivationViewModel(
                pumpManager,
                { self.resetNavigationTo([.settingsScreen]) },
                { self.navigateTo(.patchPrimingScreen) }
            )
            return hostingController(
                rootView: PatchActivationView(viewModel: viewModel)
                    .onAppear { UIApplication.shared.isIdleTimerDisabled = true }
            )

        case .settingsScreen:
            let toDeactivation = {
                self.navigateTo(.deactivatePatchScreen)
            }
            let toActivation: (Bool) -> Void = { alreadyPrimed in
                self.navigateTo(alreadyPrimed ? .patchActivationScreen : .patchPrimingScreen)
            }
            let toSettings = {
                self.navigateTo(.patchSettingsScreen)
            }
            let toPatchDetails = {
                self.navigateTo(.patchDetailsScreen)
            }
            let toPreviousPatchDetails = {
                self.navigateTo(.patchPreviousDetailsScreen)
            }
            let toInsulinType = {
                self.navigateTo(.insulinTypeScreen)
            }
            let toActivatePatch = {
                self.navigateTo(.pumpBaseSettingsScreen)
            }

            let viewModel = M640GKitSettingsViewModel(
                pumpManager,
                toDeactivation,
                toActivation,
                toSettings,
                toPatchDetails,
                toPreviousPatchDetails,
                toInsulinType,
                pumpRemoval,
                toActivatePatch
            )
            return hostingController(rootView: M640GKitSettings(
                viewModel: viewModel,
                supportedInsulinTypes: allowedInsulinTypes
            ))
        case .patchDetailsScreen:
            let viewModel = PatchDetailsViewModel(pumpManager: pumpManager)
            return hostingController(rootView: PatchDetailsView(viewModel: viewModel))
        case .patchPreviousDetailsScreen:
            let viewModel = PreviousPatchDetailsViewModel(pumpManager: pumpManager)
            return hostingController(rootView: PreviousPatchDetailsView(viewModel: viewModel))
        }
    }

    private func hostingController<Content: View>(rootView: Content) -> DismissibleHostingController<some View> {
        let rootView = rootView
            .environment(\.appName, Bundle.main.bundleDisplayName)
        return DismissibleHostingController(content: rootView, colorPalette: colorPalette)
    }

    override func viewDidDisappear(_: Bool) {
        UIApplication.shared.isIdleTimerDisabled = false
    }

    private func pumpRemoval() {
        NotificationManager.clearPendingNotifications()
        guard let completionDelegate = self.completionDelegate, let pumpManager = self.pumpManager else {
            return
        }

        pumpManager.notifyDelegateOfDeactivation {
            DispatchQueue.main.async {
                completionDelegate.completionNotifyingDidComplete(self)
            }
        }
    }
}

extension M640GKitUICoordinator {
    func navigateTo(_ screen: M640GKitUIScreen) {
        screenStack.append(screen)
        let viewController = viewControllerForScreen(screen)
        viewController.isModalInPresentation = false
        pushViewController(viewController, animated: true)
        viewController.view.layoutSubviews()
    }

    func resetNavigationTo(_ screens: [M640GKitUIScreen]) {
        screenStack = screens
        let viewControllers = screenStack.map {
            let viewController = viewControllerForScreen($0)
            viewController.isModalInPresentation = false
            viewController.view.layoutSubviews()
            return viewController
        }

        setViewControllers(viewControllers, animated: true)
    }
}
