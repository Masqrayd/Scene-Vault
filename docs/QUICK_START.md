# Scene Vault: Windows quick start

This guide applies to the compiled Windows package containing
`scene-vault/bin/64bit/scene-vault.dll`. A source ZIP does not contain that DLL.

## Install

1. Close OBS.
2. Extract the compiled Windows artifact.
3. Copy its `scene-vault` folder into `C:\ProgramData\obs-studio\plugins\`.
   The DLL should end up at:
   `C:\ProgramData\obs-studio\plugins\scene-vault\bin\64bit\scene-vault.dll`.
   Windows may ask for administrator access.
4. Open OBS, then **Tools > Scene Vault**.

Alternatively, run the included `Install-SceneVault.ps1` in PowerShell from the
compiled package. It checks that OBS is closed and retains an existing plugin as
a backup. Portable OBS installations need an explicitly selected plugin location
and are not covered by the default installer path.

## Floating scene controller

1. Open **Tools > Masqrayd Scene Controller**.
2. Move and resize the window on your preferred monitor.
3. Check **Always on top** to keep it above ordinary application windows.
4. Enable Studio Mode in OBS, or use **Enable Studio Mode** in the controller.
5. Single-click a scene button to select Preview. Double-click to transition it
   to Program using your OBS transition. **Transition Preview** is also available.
6. Adjust **Columns** and **Height** for your preferred button layout.

Program has a red border, Preview a gold border, and both a purple border; buttons
also show text labels. Hidden scenes never appear in this controller, even if
Scene Vault temporarily reveals them for editing. Permanently restoring a scene
makes its button available again. Studio Mode off disables scene switching here.
Closing the window hides it; reopen it through Tools. Position, size, button
layout, and Always on top are saved. The controller closes with OBS. Exclusive
full-screen applications and other topmost windows may cover an Always on top window.

This 0.2.0 package updates the existing Scene Vault DLL. Close OBS and replace
that DLL/folder; keep only one Scene Vault installation. Your saved hidden-scene
membership remains in the scene collection.

## Hide secondary scenes

1. Select your main production scene in OBS. If Studio Mode is on, move Preview
   away from the secondary scene too.
2. Open **Tools > Scene Vault**.
3. Search for a secondary scene, select its manager row, and click **Hide selected**.
4. Repeat for your other secondary scenes. Their nested uses keep working.

Selecting rows in the manager does not switch OBS scenes. The current scene and
the last remaining main scene cannot be hidden.

## Edit later

1. Open Scene Vault; check **Only show scenes in the vault** if desired.
2. Select a hidden scene and click **Edit temporarily**.
3. OBS turns on Studio Mode and opens that scene in Preview. Make your changes.
4. Click **Finish editing** to return to the previous Preview and hide the scene again.

Edits to shared sources affect all uses of those sources, including live output.
Studio Mode prevents an automatic scene switch; it does not isolate shared edits.

## Recover or remove

- **Show all temporarily** reveals scenes while retaining their hidden settings.
- **Restore selected** permanently returns one scene to the main list.
- **Restore all permanently** returns every scene to the list after confirmation.
- Close OBS and remove the installed `scene-vault` plugin folder to uninstall.
  All scenes will be visible when OBS restarts. Saved vault metadata can remain
  harmlessly in the collection and will be reused if the plugin is reinstalled.

Hidden scenes are still reachable through OBS hotkeys and other controllers.
The native Remove Scene action is disabled when a vaulted scene is selected.
Other tools can still select or delete scenes; this is not a global lock.
