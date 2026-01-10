// swift-tools-version: 6.2
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
    name: "YukinekoEngine",
    platforms: [
        .macOS(.v13)
    ],
    products: [
        .executable(name: "YukinekoBenchmark", targets: ["YukinekoBenchmark"])
    ],
    targets: [
        .target(
            name: "CShim",
            dependencies: []
        ),
        .executableTarget(
            name: "YukinekoBenchmark",
            dependencies: ["CShim"]
        ),
    ]
)
