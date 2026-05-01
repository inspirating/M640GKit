//
//  M640GPumpSetupViewController.swift
//  Loop
//
//  Copyright © 2018 LoopKit Authors. All rights reserved.
//

import UIKit
import LoopKit
import LoopKitUI
import M640GKit
import RileyLinkBLEKit
import RileyLinkKit
import RileyLinkKitUI


public class M640GPumpManagerSetupViewController: RileyLinkManagerSetupViewController {

    class func instantiateFromStoryboard() -> M640GPumpManagerSetupViewController {
        return UIStoryboard(name: "M640GPumpManager", bundle: Bundle(for: M640GPumpManagerSetupViewController.self)).instantiateInitialViewController() as! M640GPumpManagerSetupViewController
    }

    override public func viewDidLoad() {
        super.viewDidLoad()

        if #available(iOSApplicationExtension 13.0, *) {
            view.backgroundColor = .systemBackground
        } else {
            view.backgroundColor = .white
        }
        navigationBar.shadowImage = UIImage()
        
        if let pumpIDSetupVC = topViewController as? M640GPumpIDSetupViewController, let rileyLinkPumpManager = rileyLinkPumpManager {
            pumpIDSetupVC.rileyLinkPumpManager = rileyLinkPumpManager
        }

    }

    private(set) var pumpManager: M640GPumpManager?
    
    internal var insulinType: InsulinType?

    internal var supportedInsulinTypes: [InsulinType]?

    /*
     1. RileyLink
     - RileyLinkPumpManagerState

     2. Pump
     - PumpSettings
     - PumpColor
     -- Submit --
     - PumpOps
     - PumpState

     3. (Optional) Connect Devices

     4. Pump Setup Complete

     */

    override public func navigationController(_ navigationController: UINavigationController, willShow viewController: UIViewController, animated: Bool) {
        super.navigationController(navigationController, willShow: viewController, animated: animated)

        if let setupViewController = viewController as? SetupTableViewController {
            setupViewController.delegate = self
        }

        // Set state values
        switch viewController {
        case let vc as M640GPumpIDSetupViewController:
            vc.rileyLinkPumpManager = rileyLinkPumpManager
            vc.maxBolusUnits = maxBolusUnits
            vc.maxBasalRateUnitsPerHour = maxBasalRateUnitsPerHour
            vc.basalSchedule = basalSchedule
        case let vc as M640GPumpSetupCompleteViewController:
            vc.pumpImage = pumpManager?.state.largePumpImage
        default:
            break
        }

        // Adjust the appearance for the main setup view controllers only
        if viewController is SetupTableViewController {
            navigationBar.isTranslucent = false
            navigationBar.shadowImage = UIImage()
        } else {
            navigationBar.isTranslucent = true
            navigationBar.shadowImage = nil
            viewController.navigationItem.largeTitleDisplayMode = .never
        }
    }

    public func navigationController(_ navigationController: UINavigationController, didShow viewController: UIViewController, animated: Bool) {

        // Adjust the appearance for the main setup view controllers only
        if viewController is SetupTableViewController {
            navigationBar.isTranslucent = false
            navigationBar.shadowImage = UIImage()
        } else {
            navigationBar.isTranslucent = true
            navigationBar.shadowImage = nil
        }
    }

    public func pumpManagerSetupComplete(_ pumpManager: M640GPumpManager) {
        self.pumpManager = pumpManager
        pumpManagerOnboardingDelegate?.pumpManagerOnboarding(didCreatePumpManager: pumpManager)
    }

    override open func finishedSetup() {
        if let pumpManager = pumpManager {
            pumpManager.completeOnboard()

            pumpManagerOnboardingDelegate?.pumpManagerOnboarding(didOnboardPumpManager: pumpManager)
            completionDelegate?.completionNotifyingDidComplete(self)
        }
    }

    public func finishedSettingsDisplay() {
        completionDelegate?.completionNotifyingDidComplete(self)
    }
    
    public func didCancel() {
        completionDelegate?.completionNotifyingDidComplete(self)
    }
}

extension M640GPumpManagerSetupViewController: SetupTableViewControllerDelegate {
    public func setupTableViewControllerCancelButtonPressed(_ viewController: SetupTableViewController) {
        didCancel()
    }
}
