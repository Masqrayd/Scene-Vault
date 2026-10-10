# Validation record: Scene Vault 0.1.0

## Completed

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

1. Run the included GitHub Windows workflow and resolve any MSVC/package issues.
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
