# Scene Vault for OBS

Native C++/Qt plugin for Masqrayd's Windows OBS 32.2.2 workflow.

Hide secondary scenes from the native Scenes dock while retaining scene objects,
their sources, filters, transforms, and uses as nested sources. Find and edit them
through **Tools > Scene Vault**.

## Status

Version 0.2.0 adds **Tools > Masqrayd Scene Controller**: a floating scene-button
window with optional Always on top, single-click Preview, and double-click
transition. The [Windows build](https://github.com/Masqrayd/Scene-Vault/actions/runs/38502595761)
succeeded and [all three Linux test suites](https://github.com/Masqrayd/Scene-Vault/actions/runs/38502595737)
passed. Download **Scene-Vault-0.2.0-Windows-x64** from the Windows run's Artifacts.
Windows runtime validation in OBS is still required. See `docs/VALIDATION.md`.

## Features

- Floating scene controller shows production scenes, excluding saved vault membership.
- Single-click selects Preview; double-click transitions the clicked scene using OBS's active transition.
- Studio Mode is required; the window includes an Enable Studio Mode button.
- Optional Always on top, adjustable columns/button height, saved position and size.
- Program and Preview labels and colored borders track OBS selection.
- Scene renaming, ordering, visibility, and collection changes update the buttons.
- Vaulted scenes stay excluded even while temporarily shown for editing or recovery.

- Search all scenes, or show only scenes in the vault.
- Hide scenes without selecting them in OBS's production scene list.
- Edit temporarily in Studio Mode Preview, then restore the previous Preview.
- Hidden membership survives renaming and saves with each scene collection.
- Temporarily show all entries without losing hidden membership.
- Permanently restore selected scenes or all scenes.
- Shift + right-click a Scenes entry for the Scene Vault menu; ordinary right-click
  retains OBS's normal menu.
- Keep the current Program/Preview scenes accessible; reject hiding the last main scene.
- Restore list entries on unload. Removing the plugin makes every scene visible again.
- Manager rows do not switch scenes, and the manager provides no delete operation.
- Disable OBS's native Remove Scene action while a vaulted scene is selected.

## Important behavior

Scene Vault controls list visibility. OBS scene hotkeys, WebSocket requests,
automatic switchers, multiview, and other plugins can still select hidden scenes.
If that happens, the active scene is temporarily shown, then hidden when it is no
longer active. OBS's native Remove Scene action is disabled when a vaulted scene is selected,
including during temporary editing. External deletion through other tools is not
blocked. Hidden entries are
deselected to avoid leaving them as targets of native bulk deletion.

Temporary editing never calls OBS's Program-scene setter or transition trigger.
Studio Mode remains on after editing. Changes to shared sources can still affect
live output wherever those sources are used. A user can also manually transition
Preview to Program; this plugin does not disable that control.

This plugin finds the native Qt scene list by its `scenes` object name, verified in
the OBS 32.2.2 source. This is an internal UI dependency. On an unsupported layout
it disables hiding and reports the problem in the manager. Other scene-organization
plugins that alter the same list should not be assumed compatible without testing.

## Get a Windows build without installing Visual Studio

1. Create an empty GitHub repository, for example `Scene-Vault`.
2. Upload the contents of this project, including `.github`, to the repository root.
   `CMakeLists.txt` and `buildspec.json` must be at the root, not inside another folder.
3. Open **Actions > Build Scene Vault for Windows**. The initial push starts the build;
   **Run workflow** can start another one.
4. Download **Scene-Vault-0.2.0-Windows-x64** from the successful run's Artifacts section.
5. Extract it, close OBS, and follow `QUICK_START.md`.

The workflow downloads the official OBS 32.2.2 sources and the matching official
Windows dependency packages with SHA-256 verification. It uploads an installable
artifact and does not publish a public release.

## Developer build

Windows: Visual Studio 2026 C++ tools, CMake 4.2+, and the Windows 11 SDK 10.0.26100.0.

```powershell
cmake --preset windows-x64 -DBUILD_TESTING=OFF
cmake --build --preset windows-x64 --parallel
cmake --install build_x64 --config RelWithDebInfo --prefix dist
```

Linux: OBS development packages, Qt6 development packages, CMake 3.28+, and a compiler.

```sh
cmake -S . -B build -DSCENE_VAULT_SYSTEM_BUILD=ON -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Tests use real libobs scene and data objects with a simulated frontend; no live
OBS session or graphics initialization is required. On Linux, libobs still requires
an X11 display for startup (a virtual display is sufficient). The tests verify that hiding
does not remove the nested scene reference or change its visibility.

License: GPL-2.0-or-later. Build scaffolding is adapted from the official
`obsproject/obs-plugintemplate`, commit `3e7d7ac3b5342cd7d9b88890b9c70b472d1520fc`.
