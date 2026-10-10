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
