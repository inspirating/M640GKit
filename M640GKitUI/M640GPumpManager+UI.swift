//
//  M640GPumpManager+UI.swift
//  Loop
//
//  Copyright © 2018 LoopKit Authors. All rights reserved.
//

import SwiftUI
import UIKit
import LoopKit
import LoopKitUI
import M640GKit
import RileyLinkKitUI


extension M640GPumpManager: PumpManagerUI {

    
    public static var onboardingImage: UIImage? {
        return UIImage.pumpImage(in: nil, isLargerModel: false, isSmallImage: true)
    }

    static public func setupViewController(initialSettings settings: PumpManagerSetupSettings, bluetoothProvider: BluetoothProvider, colorPalette: LoopUIColorPalette, allowDebugFeatures: Bool, prefersToSkipUserInteraction: Bool, allowedInsulinTypes: [InsulinType]) -> SetupUIResult<PumpManagerViewController, PumpManagerUI> {
        let navVC = M640GPumpManagerSetupViewController.instantiateFromStoryboard()
        navVC.supportedInsulinTypes = allowedInsulinTypes
        let didConfirm: (InsulinType) -> Void = { [weak navVC] (confirmedType) in
            if let navVC = navVC {
                navVC.insulinType = confirmedType
                let nextViewController = navVC.storyboard?.instantiateViewController(identifier: "RileyLinkSetup") as! RileyLinkSetupTableViewController
                navVC.pushViewController(nextViewController, animated: true)
            }
        }
        let didCancel: () -> Void = { [weak navVC] in
            if let navVC = navVC {
                navVC.didCancel()
            }
        }
        let insulinSelectionView = InsulinTypeConfirmation(initialValue: .novolog, supportedInsulinTypes: allowedInsulinTypes, didConfirm: didConfirm, didCancel: didCancel)
        let rootVC = UIHostingController(rootView: insulinSelectionView)
        rootVC.title = "Insulin Type"
        navVC.pushViewController(rootVC, animated: false)
        navVC.navigationBar.backgroundColor = .secondarySystemBackground
        navVC.maxBasalRateUnitsPerHour = settings.maxBasalRateUnitsPerHour
        navVC.maxBolusUnits = settings.maxBolusUnits
        navVC.basalSchedule = settings.basalSchedule
        return .userInteractionRequired(navVC)
    }

    public func settingsViewController(bluetoothProvider: BluetoothProvider, colorPalette: LoopUIColorPalette, allowDebugFeatures: Bool, allowedInsulinTypes: [InsulinType]) -> PumpManagerViewController {
        return M640GUICoordinator(pumpManager: self, colorPalette: colorPalette, allowDebugFeatures: allowDebugFeatures, allowedInsulinTypes: allowedInsulinTypes)
    }
    
    public func deliveryUncertaintyRecoveryViewController(colorPalette: LoopUIColorPalette, allowDebugFeatures: Bool) -> (UIViewController & CompletionNotifying) {
        return M640GUICoordinator(pumpManager: self, colorPalette: colorPalette, allowDebugFeatures: allowDebugFeatures, allowedInsulinTypes: [])
    }
    
    public var smallImage: UIImage? {
        return state.smallPumpImage
    }
    
    public func hudProvider(bluetoothProvider: BluetoothProvider, colorPalette: LoopUIColorPalette, allowedInsulinTypes: [InsulinType]) -> HUDProvider? {
        return M640GHUDProvider(pumpManager: self, bluetoothProvider: bluetoothProvider, colorPalette: colorPalette, allowedInsulinTypes: allowedInsulinTypes)
    }
    
    public static func createHUDView(rawValue: HUDProvider.HUDViewRawState) -> BaseHUDView? {
        return M640GHUDProvider.createHUDView(rawValue: rawValue)
    }
}

    public enum M640GStatusBadge: DeviceStatusBadge {
        case timeSyncNeeded

        public var image: UIImage? {
            switch self {
            case .timeSyncNeeded:
                return UIImage(systemName: "clock.fill")
            }
        }

        public var state: DeviceStatusBadgeState {
            switch self {
            case .timeSyncNeeded:
                return .warning
            }
        }
    }

// MARK: - PumpStatusIndicator
extension M640GPumpManager {
    
    public var pumpStatusHighlight: DeviceStatusHighlight? {
        return buildPumpStatusHighlight(for: state, recents: recents, andDate: dateGenerator())
    }
    
    public var pumpLifecycleProgress: DeviceLifecycleProgress? {
        return nil
    }
    
    public var pumpStatusBadge: DeviceStatusBadge? {
        if isClockOffset {
            return M640GStatusBadge.timeSyncNeeded
        } else {
            return nil
        }
    }
}
