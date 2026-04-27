# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

FLEX (Flipboard Explorer) is an Objective-C in-app debugging library for iOS. It ships a singleton (`FLEXManager`) that overlays a toolbar window above the host app and exposes tools for inspecting/modifying live state: view hierarchy, runtime metadata, network requests, heap, file system, SQLite/Realm, `NSUserDefaults`, keychain, system log, etc. Minimum target is iOS 9 (CocoaPods) / iOS 10–12 (SPM, depending on Swift version — see `Package.swift`).

## Build, test, run

This project has **three parallel build configurations** that must all stay in sync when files are added/moved:

1. **Xcode project** (`FLEX.xcodeproj`) — primary, has `FLEX` framework target and `FLEXTests` unit-test target.
2. **CocoaPods** (`FLEX.podspec`) — `source_files`, `public_header_files`, framework links.
3. **Swift Package Manager** (`Package.swift`) — `headerSearchPaths` array and excludes.

When adding a new folder under `Classes/`, you must:
- Add it to `Package.swift`'s `headerSearchPaths` (regenerate via `bash generate-spm-headers.sh | grep headerSearchPath`).
- Re-run `bash generate-spm-headers.sh` to refresh the symlinks in `Classes/Headers/` (these are the umbrella public headers SPM consumers see).
- Update `public_header_files` in `FLEX.podspec` if the new headers should be public — the comment block in `generate-spm-headers.sh` documents what must match.

Build commands (require macOS + Xcode):
```sh
# Library (matches Travis CI)
xcodebuild -workspace FLEX.xcworkspace -scheme FLEX -sdk iphonesimulator build

# Example app
xcodebuild -workspace FLEX.xcworkspace -scheme UICatalog -sdk iphonesimulator build

# Tests (XCTest target FLEXTests)
xcodebuild -project FLEX.xcodeproj -scheme FLEX -sdk iphonesimulator test
# Single test:
xcodebuild -project FLEX.xcodeproj -scheme FLEX -sdk iphonesimulator \
  -only-testing:FLEXTests/FLEXTests/testRuntimeAdditions test
```

The example app lives in `Example/`; run `pod install` there before opening `FLEXample-Cocoapods.xcworkspace`. There is also `FLEXample-SPM.xcodeproj` for testing SPM integration.

CI is Travis (`.travis.yml`) and just runs `xcodebuild ... build` for the `FLEX` and `UICatalog` schemes — no test step in CI today.

Code style: see `.clang-format` (LLVM-based, 4-space indent, 100-col).

## Architecture

The codebase is mostly Objective-C (`.m`), with a few `.mm` files that need C++ for runtime work (`FLEXObjcInternal.mm`, `FLEXSwiftInternal.mm`, `FLEXFirebaseTransaction.mm`) — this is why `Package.swift` sets `cxxLanguageStandard: .gnucxx11` and the podspec sets `CLANG_CXX_LANGUAGE_STANDARD = gnu++11`.

Top-level layering inside `Classes/`:

- **`Manager/`** — `FLEXManager` (public singleton) is intentionally thin. Categories (`+Extensibility`, `+Networking`) split the public API surface; the actual UI work is delegated to `FLEXExplorerViewController`. `Manager/Private/` holds extensibility internals (custom content viewers, simulator shortcuts, global entries).
- **`ExplorerInterface/`** — `FLEXWindow` is a passthrough `UIWindow` that hosts `FLEXExplorerViewController`, which owns the floating toolbar and is the root of all "tools" presented modally. `Tabs/` and `Bookmarks/` implement the multi-tab navigation model used when a tool is presented.
- **`Toolbar/`** — the floating draggable toolbar (`FLEXExplorerToolbar` + `FLEXExplorerToolbarItem`).
- **`ObjectExplorers/`** — generic object inspector. `FLEXObjectExplorer` is a model that produces `FLEXProperty` / `FLEXIvar` / `FLEXMethod` / `FLEXProtocol` arrays for any class/instance via the runtime. `FLEXObjectExplorerFactory` is the entry point that picks a subclass of `FLEXObjectExplorerViewController` based on the object's class. `Sections/` contains reusable `FLEXTableViewSection` subclasses; `Sections/Shortcuts/` provides class-specific shortcut rows (e.g. for `UIView`, `NSData`, etc.).
- **`Editing/`** — modal editors for properties/ivars and method-calling. `FLEXArgumentInputViewFactory` returns the right input view per Objective-C type encoding.
- **`GlobalStateExplorers/`** — top-level tools reachable from the globals tab: `RuntimeBrowser/` (all loaded classes/images), `FileBrowser/`, `DatabaseBrowser/` (SQLite + Realm), `Keychain/`, `SystemLog/`, address explorer, live-objects (heap) explorer, cookies, APNS, web. Each tool is self-contained and presented via `FLEXManager presentTool:`.
- **`ViewHierarchy/`** — two view debugger UIs: `TreeExplorer/` (list) and `SnapshotExplorer/` (3D, SceneKit-based — hence the `SceneKit` framework dep).
- **`Network/`** — network MITM. `FLEXNetworkRecorder` is the singleton that stores transactions; recording is enabled by swizzling `NSURLSession`/`NSURLConnection` in `FLEXNetworkObserver` (under `PonyDebugger/`, the in-tree adapted Pony Debugger code). `Firestore.h`/`FLEXFirebaseTransaction.mm` add Firebase observation when Firebase is linked. `OSCache/` is the in-tree response cache. `FLEXMITMDataSource` is the shared abstraction across REST/WebSocket/Firebase transactions.
- **`Core/`** — base classes used everywhere: `FLEXTableViewController`, `FLEXFilteringTableViewController`, `FLEXNavigationController`, `FLEXTableViewSection` and standard cells. New explorer screens should subclass these rather than `UITableViewController` directly.
- **`Utility/Runtime/Objc/`** — the runtime layer that everything else depends on. `FLEXObjcInternal` (heap-pointer validation), `FLEXTypeEncodingParser` (parses `@encode` strings, including struct layouts), `FLEXRuntimeSafety` (denylist of classes unsafe to message), and `Reflection/` which wraps the Objective-C runtime in objects (`FLEXMethod`, `FLEXProperty`, `FLEXIvar`, `FLEXProtocol`, `FLEXMirror`). `flex_fishhook.c` is the in-tree fishhook used to swizzle C functions. `FLEXSwiftInternal.mm` performs limited Swift class detection.
- **`Utility/Categories/`** — Foundation/UIKit categories (mostly `flex_` prefixed). `Private/` is for categories not exposed publicly.

### Key cross-cutting concerns

- **Heap safety**: any code that walks `malloc` blocks (live-objects explorer, address explorer) goes through `FLEXHeapEnumerator` and `FLEXObjcInternal.flex_isaPointerLooksLikeObject` to avoid dereferencing garbage. Don't bypass these checks.
- **Public vs. internal headers**: only headers listed in `FLEX.podspec`'s `public_header_files` and symlinked into `Classes/Headers/` by `generate-spm-headers.sh` are visible to consumers. The umbrella imports are `FLEX.h`, `FLEX-Core.h`, `FLEX-Runtime.h`, `FLEX-Categories.h`, `FLEX-ObjectExploring.h`. If you make a header public, update both the podspec and the generator script.
- **Excluding from Release**: the README documents this for every integration path. Don't add code that assumes FLEX is always linked — host apps strip it from Release builds via `-Wl` / `Excluded Source File Names` / `:configurations => ['Debug']`.
- **Warnings**: FLEX intentionally uses deprecated APIs (it's a debugger). The `-Wno-deprecated-declarations`, `-Wno-strict-prototypes`, `-Wno-unsupported-availability-guard` flags are required; don't try to "fix" the underlying warnings unless you understand why FLEX uses the deprecated API.

## Tests

`FLEXTests/` is an XCTest target. Notable suites:
- `FLEXTests.m` — runtime additions, type encoding round-trips, heap enumerator sanity.
- `FLEXTypeEncodingParserTests.m` — `@encode` parser; this is the hairiest piece of code in the project, exercise it whenever you touch `FLEXTypeEncodingParser`.
- `FLEXTestsMethodsList.m` — verifies method-list reflection.
