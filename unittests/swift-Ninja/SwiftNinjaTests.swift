// This source file is part of the Swift.org open source project
//
// Copyright 2020 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for Swift project authors

import XCTest

import llbuildTestSupport
import Ninja

class SwiftNinjaTests: XCTestCase {
    func testBasics() throws {
        let manifestFile = makeTemporaryFile("""
            rule CMD
                command = ls $in $out $thing
            build output: CMD input1 input2 | implicit-input || order-only-input
                generator = true
                description = xyz
                restat = true
                thing = x\n
            """)
        let manifest = try NinjaManifest(path: manifestFile)
        XCTAssertEqual(manifest.rules["CMD"], Ninja.Rule(command: "ls $in $out $thing"))
        XCTAssertEqual(manifest.commands, [
                Ninja.Command(
                    rule: "CMD",
                    inputs: ["input1", "input2"],
                    implicitInputs: ["implicit-input"],
                    orderOnlyInputs: ["order-only-input"],
                    outputs: ["output"],
                    command: "ls input1 input2 output x",
                    description: "xyz",
                    generator: true,
                    restat: true)
            ])
    }
}
