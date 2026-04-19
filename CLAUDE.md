# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

FLEX (Flipboard Explorer) is an in-app debugging and exploration toolkit for iOS. It is a library compiled into a host app (CocoaPods / Carthage / manual integration) and, when `[[FLEXManager sharedManager] showExplorer]` is called, presents a toolbar in a dedicated `UIWindow` above the host app. From that toolbar users can introspect the view hierarchy, heap, runtime, network traffic, file system, databases, keychain, and `NSUserDefaults` at runtime.

The codebase is predominantly Objective-C (`.h`/`.m`/`.mm`) with a small amount of Swift used for Swift-runtime introspection. Minimum iOS target is 9.0 (`FLEX.podspec`).

## Build, Run, and Test

The project uses `FLEX.xcworkspace`, which references both `FLEX.xcodeproj` (the library) and `Example/UICatalog.xcodeproj` (a sample host app for running FLEX in the simulator).

Common commands (matching CI in `.travis.yml`):

```sh
# Build the FLEX library
xcodebuild -workspace FLEX.xcworkspace -scheme FLEX -sdk iphonesimulator build | xcpretty

# Build and run the UICatalog example app (the interactive test bed)
xcodebuild -workspace FLEX.xcworkspace -scheme UICatalog -sdk iphonesimulator build | xcpretty

# Run the unit tests (XCTest target: FLEXTests)
xcodebuild test -workspace FLEX.xcworkspace -scheme FLEX -sdk iphonesimulator -destination 'platform=iOS Simulator,name=iPhone 14'

# Run a single test method
xcodebuild test -workspace FLEX.xcworkspace -scheme FLEX \
  -sdk iphonesimulator -destination 'platform=iOS Simulator,name=iPhone 14' \
  -only-testing:FLEXTests/FLEXTests/testRuntimeAdditions
```

There is no linter script; formatting is defined in `.clang-format` (WebKit base, 4-space indent, no column limit, pointer binds to name).

Manual interactive testing is done by running the `UICatalog` scheme in the simulator and pressing `f` to toggle the FLEX toolbar (`?` lists keyboard shortcuts).

## Architecture

### Entry Point and Window Model

- `FLEXManager` (singleton, `Classes/Manager/`) is the only public entry point. `FLEXManager.h` exposes `showExplorer`/`hideExplorer`/`toggleExplorer`; `FLEXManager+Extensibility.h` adds globals-list entries and simulator keyboard shortcuts; `FLEXManager+Networking.h` toggles network instrumentation.
- `FLEXWindow` (`Classes/ExplorerInterface/`) is a borderless `UIWindow` that forwards touches outside of FLEX UI back to the host app via `FLEXWindowEventDelegate`. This is how FLEX can live "above" the app without blocking input.
- `FLEXExplorerViewController` owns the floating toolbar, the select/move/views/menu tools, drag handling for the toolbar, and modal presentation of "tools" via `toggleToolWithViewControllerProvider:completion:`.

### Object Explorer Pipeline

The Object Explorer is the central UI for inspecting any `id`. It is built from composable sections:

- `FLEXObjectExplorer` (`Classes/ObjectExplorers/`) is the model. `+forObject:` produces a view model that walks the class hierarchy and returns arrays-of-arrays of `FLEXProperty`, `FLEXIvar`, `FLEXMethod`, `FLEXProtocol`, etc. The `classScope` index selects which class in the hierarchy to show.
- `FLEXObjectExplorerViewController` is a `FLEXTableViewController` subclass that renders those sections.
- `FLEXObjectExplorerFactory` dispatches to a per-class explorer view controller. Use `+registerExplorerSection:forClass:` to inject shortcut sections for a specific class at runtime.
- Sections live under `Classes/ObjectExplorers/Sections/` and subclass `FLEXTableViewSection` (see next).
- `Sections/Shortcuts/` contains per-type "shortcut" sections (`FLEXViewShortcuts`, `FLEXLayerShortcuts`, `FLEXImageShortcuts`, `FLEXClassShortcuts`, `FLEXBlockShortcuts`, ...) surfaced at the top of the explorer for common object types. `FLEXShortcutsFactory+Defaults` registers the built-ins.

### Section-Based Table Views

`Classes/Core/` defines the section abstraction used throughout FLEX:

- `FLEXTableViewSection` — abstract base. Subclasses declare `numberOfRows`, a `cellRegistrationMapping`, `canSelectRow:` + (`didSelectRowAction:` or `viewControllerToPushForRow:`), and honor a `filterText` property for search.
- `FLEXSingleRowSection` — convenience for one-row sections.
- `FLEXTableViewController` — owns a list of sections, a search bar (with debounce constants `kFLEXDebounce*`), and an optional `FLEXScopeCarousel` (horizontal scope selector for >4 scopes).

When adding a new screen that shows grouped content, prefer composing `FLEXTableViewSection` subclasses inside a `FLEXTableViewController` rather than writing a monolithic `UITableViewDataSource`.

### Runtime Introspection Layer

`Classes/Utility/Runtime/` wraps the `<objc/runtime.h>` C API into object-oriented types: `FLEXProperty`, `FLEXPropertyAttributes`, `FLEXIvar`, `FLEXMethod`/`FLEXMethodBase`, `FLEXProtocol`, `FLEXMirror` (reflection facade), `FLEXClassBuilder`/`FLEXProtocolBuilder` (dynamic class/protocol creation). `FLEXRuntimeSafety` gates unsafe classes that would crash on introspection; `FLEXRuntimeUtility` contains higher-level helpers (invoking methods, formatting type encodings, JSON-parsing user input for argument values).

`Classes/Utility/Swift/` is the Swift-runtime counterpart (in progress — see TODO in README). It mirrors metadata structures (`Metadata.h`, `MMetadata.h`, `MetadataValues.h`, `RelativePointer.h`) and defines `FLEXSwiftMirror`, `FLEXSwiftMethod`, `FLEXSwiftType`. The parallel `Classes/Utility/Categories/FLEXRuntime+*` files add UIKit-display helpers (cell building, sorting, comparison).

### Major Feature Areas

Each directory under `Classes/` is a largely self-contained feature:

- `Network/` — swizzles `NSURLConnection*Delegate` and `NSURLSession*Delegate` methods via `FLEXNetworkObserver` (in `PonyDebugger/`, adapted from Square's PonyDebugger), records transactions in `FLEXNetworkRecorder`, and renders them in MITM/detail view controllers. Enabled via `FLEXManager.networkDebuggingEnabled`.
- `ViewHierarchy/` — two modes: `TreeExplorer/` (table-of-views) and `SnapshotExplorer/` (3D-perspective layered preview using `FHSSnapshotView`).
- `GlobalStateExplorers/` — one subdirectory per "tool" surface: `DatabaseBrowser/` (SQLite via FMDB-style manager + Realm), `FileBrowser/`, `RuntimeBrowser/` (class browsing via keypath tokenizer `TBKeyPath*`), `SystemLog/` (ASL on old iOS, OSLog via `ActivityStreamAPI` on newer), `Keychain/` (vendored SSKeychain), `Globals/` (the top-level menu list; extend via `FLEXManager.registerGlobalEntryWithName:`).
- `Editing/` — the type-specialized argument/value editor. `FLEXArgumentInputViewFactory` picks an `FLEXArgumentInputView` subclass per Objective-C type encoding; `FLEXMethodCallingViewController` drives invocation flow.
- `ExplorerInterface/Bookmarks/` and `Tabs/` — saving explored objects and switching between open explorer stacks.
- `Toolbar/` — the draggable floating `FLEXExplorerToolbar` and `FLEXToolbarItem`.

### Utility Conventions

- `NSArray+Functional` provides `flex_mapped:`, `flex_filtered:`, etc. — prefer these over manual loops in this codebase.
- `NSObject+Reflection` is the shortest route to runtime data from any object (`flex_classHierarchy`, properties, ivars, methods).
- `FLEXAlert` is a block-based `UIAlertController` builder used everywhere for confirmations and forms.
- `FLEXColor` and `FLEXResources` centralize dark/light theming and bundled image/data resources.
- User input that represents an object value (ivar edits, `NSUserDefaults`, method arguments of type `id`) is parsed as JSON — see "Additional Notes" in `README.md`. Strings must be quoted.

## Conventions Specific to This Codebase

- All public types are prefixed `FLEX` (plus `FHS` for the hierarchy snapshot subsystem, `TB` for the Tanner-Bennett runtime-browser code). Preserve the prefix when adding new types so the "Excluded Source File Names = FLEX*" release-build exclusion pattern continues to work.
- Categories on system classes use a `flex_` method prefix (e.g. `flex_classHierarchy`, `flex_mapped:`) to avoid collisions with host-app categories.
- Headers imported by consumers must be listed in `FLEX.podspec` under `public_header_files`. The podspec's `source_files` glob (`Classes/**/*.{h,m,mm}`) already picks up new files automatically.
- FLEX frequently introspects classes that may crash on normal messaging; always route such access through `FLEXRuntimeSafety` / `FLEXObjcInternal` rather than calling runtime C APIs directly.
- Keyboard-shortcut code in `FLEXKeyboardShortcutManager` is compiled out on device builds via `#if TARGET_OS_SIMULATOR`; wrap new simulator-only affordances the same way.
- Use `NSAssert(NSThread.isMainThread, ...)` on any API that touches `FLEXManager.explorerWindow` or presents UI — see the pattern in `FLEXManager.m`.

## Release Build Safety

FLEX must never ship in App Store builds. The integration guidance in `README.md` (CocoaPods `:configurations => ['Debug']`, Carthage run-script guard, `FLEX*` excluded source files for manual installs) is load-bearing — do not change class prefixes, public header layout, or the podspec file-glob without updating that guidance.
