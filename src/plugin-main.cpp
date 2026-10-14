#include <obs-module.h>
#include <QMainWindow>
#include "scene-vault.hpp"

OBS_DECLARE_MODULE()

static SceneVault *vault = nullptr;

MODULE_EXPORT const char *obs_module_name(void) { return "Scene Vault"; }
MODULE_EXPORT const char *obs_module_description(void)
{
    return "Hide secondary scenes from the Scenes dock while preserving nested uses; floating scene buttons with double-click transitions.";
}
MODULE_EXPORT const char *obs_module_author(void) { return "Masqrayd"; }

bool obs_module_load(void)
{
    blog(LOG_INFO, "[Scene Vault] Loading version 0.2.4");
    return true;
}

void obs_module_post_load(void)
{
    auto *window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
    if (!window) {
        blog(LOG_ERROR, "[Scene Vault] OBS main window unavailable; no scenes will be hidden.");
        return;
    }
    vault = new SceneVault(window);
    QObject::connect(window, &QObject::destroyed, [] { vault = nullptr; });
}

void obs_module_unload(void)
{
    delete vault;
    vault = nullptr;
}
