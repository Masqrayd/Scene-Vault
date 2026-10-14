# Validation record: Scene Vault 0.2.1 controller correction

- Studio Mode control now reports ON/OFF and toggles with one click. The previous
  Enable button always requested ON and appeared unresponsive when already on.
- Transition Preview is now Send Preview to Program, colored #e6b84a to match Preview.
- Native Linux build and local controller/state suites passed. Toggle tests verify
  enabling, disabling, and re-enabling, with label updates.
- All three GitHub Linux suites passed:
  https://github.com/Masqrayd/Scene-Vault/actions/runs/38503890879
- Windows OBS 32.2.2 build and packaging succeeded:
  https://github.com/Masqrayd/Scene-Vault/actions/runs/38503890895
- Downloaded ZIP checksum, integrity, required installation files, and DLL entry
  points verified. SHA-256:
  `38c330b50666c9c2d463119568248431ad6fc0172bf687491256c34ee4990466`.
- Inspected the rendered controller layout with the ON label and yellow action.
- Actual Windows OBS runtime testing of this update remains required.

---

# Validation record: Scene Vault 0.2.0 + Scene Controller

## Local results for 0.2.0

- Native plugin and all three test executables compiled on Linux with GCC 13.3,
  Qt 6.4.2, and OBS 30.0.2 headers/libraries.
- `controller-behavior` and `vault-state` passed. Controller tests use real Qt
  mouse events and a simulated frontend/source contract without a renderer.
- Verified: single-click Preview only; one double-click transition; second release
  preserves Preview/Program swap; duplicate gesture guard; hidden membership;
  restore/hide updates; rename retains UUID/button identity; scene deletion;
  collection-change click guard; Program highlight; unowned floating window;
  Always on top flag toggle, persistence, and restoration.
- The real-libobs `vault-integration` executable compiled but could not run here:
  libobs needs an X display, and this environment blocks Xvfb's listening sockets.
  GitHub's Linux workflow runs the complete suite under Xvfb.
- GitHub's Linux workflow passed all three suites, including real-libobs integration:
  https://github.com/Masqrayd/Scene-Vault/actions/runs/38502595737
- Windows x64 compilation and packaging succeeded against official OBS 32.2.2:
  https://github.com/Masqrayd/Scene-Vault/actions/runs/38502595761
- Built source commit: `b3575b3ae6816441b072e7e8eac61c4b912de44e`.
- Downloaded artifact integrity and required contents verified: DLL, PDB,
  installation/uninstallation scripts, and quick-start guide.
- Artifact SHA-256 matches GitHub's digest:
  `e1574dfc7ff4066b2d5fba20ff105592548bd9668243d75380907840e927b14c`.
- DLL exports load/unload/post-load entry points and links to OBS, the OBS
  frontend API, Qt6, and the Windows C/C++ runtime.
- Windows runtime, exclusive full-screen, and monitor-position behavior still
  require validation in a real OBS session.

## New acceptance checks

- Compile on Windows against official OBS 32.2.2.
- Qt mouse test: one click selects Preview without transition; double-click triggers
  exactly one transition; its final release does not undo OBS's Preview/Program swap.
- Confirm vaulted scenes remain absent; production scene renaming retains UUID identity.
- Studio Mode off and collection switching disable scene buttons.
- Always on top toggles the Qt window flag and is saved with geometry.
- Existing Scene Vault tests must continue passing.

## Windows runtime checks

1. Open Tools > Masqrayd Scene Controller in OBS 32.2.2.
2. Single-click then double-click scenes using Fade, Cut, and a scene transition override.
3. Check Preview/Program swaps and repeated double-clicks; verify Program switches once.
4. Toggle Always on top and place a browser above/below the controller. Move it across monitors.
5. Change columns/height, close/reopen, restart OBS, and disconnect a monitor.
6. Hide/restore/rename/reorder scenes and switch scene collections.
7. Confirm temporarily editing a vaulted scene never adds its controller button.
8. Shut down OBS with the controller open; verify clean exit.

A successful build and simulated frontend tests do not establish Windows runtime behavior.

---

Historical 0.1.0 validation follows; these results apply to the prior version.

# Validation record: Scene Vault 0.1.0

## Completed

- Windows x64 build against official OBS 32.2.2 succeeded on GitHub Actions:
  https://github.com/Masqrayd/Scene-Vault/actions/runs/38013457530
- Downloaded and checked the artifact ZIP: DLL, PDB, installer/uninstaller scripts,
  and quick-start guide are present; archive integrity check passed.
- Artifact SHA-256 matches GitHub's reported digest:
  `a6fe50a94318ef27bd7017e0965f5d3634554dc9024ddc583f051a7bafd3f759`.
- DLL exports OBS's load/unload/post-load entry points and links against OBS,
  OBS frontend, Qt6, and the Windows C/C++ runtime.


- Compiled the native plugin and test executables on Linux with GCC 13.3, Qt 6.4.2,
  and the packaged OBS 30.0.2 development/runtime libraries.
- Separately checked both production C++ files against the official OBS 32.2.2
  headers with `-Wall -Wextra -Werror`; no errors or warnings.
- Two CTest suites passed: state safeguards and frontend integration.
- Integration tests used real libobs scenes, real nested scene items, and real
  OBS data serialization, with a simulated Qt frontend and virtual X display.
- Verified: manager selection does not switch output; hidden entries disappear;
  nested references and their visibility remain intact; settings serialize by
  UUID; renaming retains hidden membership; editing enables Studio Mode and
  changes only Preview; Finish editing restores Preview and re-hides the entry;
  native deletion is disabled during vault editing and restored afterward;
  temporary recovery retains saved membership; search works; external activation
  remains accessible; collections do not inherit one another's hidden membership;
  settings can reload; permanent restore works; unload restores native list entries.
- State tests additionally cover active-scene protection, nonexistent scene
  rejection, the last-main-scene safeguard, and clearing temporary state.
- Parsed project JSON and Windows workflow YAML successfully.
- Verified the downloaded official OBS 32.2.2 source ZIP's SHA-256 and used the
  matching dependency metadata from that release's CMake presets.

These checks do not constitute Windows runtime testing or rendered video-output
testing. The simulated frontend checks the plugin's actions; a real OBS session
is still needed to confirm OBS's native UI behavior and integration.

## Still required before a Windows release

1. Windows compilation is complete. Start with installation and runtime testing below.
2. Install the compiled DLL into Windows OBS 32.2.2.
3. Use a disposable scene collection containing Main, Camera, and Overlay scenes.
   Nest Camera and Overlay in Main, then hide them through the manager.
4. Check both native scene-list and scene-grid layouts; confirm Main's output is
   unchanged and nested scenes remain available in the Add Scene source selector.
5. Rename a hidden scene, reorder the list, restart OBS, and switch between two
   collections. Confirm hidden membership remains correct.
6. Edit a hidden scene, finish the edit, and check that Program never switches.
   Check the native Remove Scene button, right-click action, Delete and Backspace
   while a vault scene is temporarily revealed.
7. Test external hotkey/WebSocket activation. It should reveal the active vault
   scene temporarily, without pretending to block that external action.
8. Test temporary Show all, permanent Restore all, deletion followed by Undo,
   plugin removal, and OBS shutdown.
9. Check installation/uninstallation scripts in a standard OBS installation.
   Portable installations and other scene-list plugins need separate validation.

## Sources used for implementation

- https://github.com/obsproject/obs-studio/tree/32.2.2
- https://docs.obsproject.com/reference-frontend-api
- https://github.com/obsproject/obs-plugintemplate
- https://github.com/actions/runner-images/blob/main/images/windows/Windows2025-VS2026-Readme.md
