//
//  ResumePumpEventTests.swift
//  M640GKitTests
//
//  Created by Pete Schwamb on 11/10/19.
//  Copyright © 2019 LoopKit Authors. All rights reserved.
//

import XCTest

@testable import M640GKit

class ResumePumpEventTests: XCTestCase {
    
    func testRemotelyTriggeredFlag() {

        let localResume = ResumePumpEvent(availableData: Data(hexadecimalString: "1f20a4e30e0a13")!, pumpModel: .model523)!
        XCTAssert(!localResume.wasRemotelyTriggered)

        let remoteResume = ResumePumpEvent(availableData: Data(hexadecimalString: "1f209de40e4a13")!, pumpModel: .model523)!
        XCTAssert(remoteResume.wasRemotelyTriggered)
    }
}
    
