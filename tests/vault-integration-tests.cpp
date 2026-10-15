#include "scene-vault.hpp"
#include "scene-controller.hpp"
#include <QSettings>
#include <QTemporaryDir>
#include <QMouseEvent>
#include <obs.h>
#include <obs-module.h>
#include <util/bmem.h>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QAction>
#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QPushButton>
#include <QTreeWidget>
#include <QtTest/QTest>
#include <cstdlib>
#include <cstdio>
#include <vector>

// Real libobs scene/data objects; only the OBS frontend is simulated.
static std::vector<obs_scene_t *> scenes;
static obs_source_t *program = nullptr;
static obs_source_t *preview = nullptr;
static QListWidget *sceneWidget = nullptr;
static bool studio = false;
static int programChanges = 0;
static int transitionCalls = 0;
static obs_frontend_save_cb saveCb = nullptr;
static void *saveOpaque = nullptr;
static obs_frontend_event_cb eventCb = nullptr;
static void *eventOpaque = nullptr;
static obs_data_t *saved = nullptr;

static void check(bool ok, const char *message)
{
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
static void syncSelection(obs_source_t *source)
{
    auto matches = sceneWidget->findItems(QString::fromUtf8(obs_source_get_name(source)), Qt::MatchExactly);
    if (!matches.isEmpty()) sceneWidget->setCurrentItem(matches.first());
}

extern "C" {
void *obs_frontend_add_tools_menu_qaction(const char *name) { return new QAction(QString::fromUtf8(name)); }
void obs_frontend_add_event_callback(obs_frontend_event_cb cb, void *p) { eventCb = cb; eventOpaque = p; }
void obs_frontend_remove_event_callback(obs_frontend_event_cb, void *) { eventCb = nullptr; }
void obs_frontend_add_save_callback(obs_frontend_save_cb cb, void *p) { saveCb = cb; saveOpaque = p; }
void obs_frontend_remove_save_callback(obs_frontend_save_cb, void *) { saveCb = nullptr; }
void obs_frontend_get_scenes(obs_frontend_source_list *out)
{
    out->sources.num = out->sources.capacity = scenes.size();
    out->sources.array = static_cast<obs_source_t **>(bmalloc(scenes.size() * sizeof(obs_source_t *)));
    for (size_t i = 0; i < scenes.size(); ++i)
        out->sources.array[i] = obs_source_get_ref(obs_scene_get_source(scenes[i]));
}
#if LIBOBS_API_MAJOR_VER >= 32
void obs_frontend_source_list_free(obs_frontend_source_list *out)
{
    for (size_t i = 0; i < out->sources.num; ++i) obs_source_release(out->sources.array[i]);
    bfree(out->sources.array);
    *out = {};
}
#endif
obs_source_t *obs_frontend_get_current_scene() { return obs_source_get_ref(program); }
obs_source_t *obs_frontend_get_current_preview_scene() { return obs_source_get_ref(preview); }
bool obs_frontend_preview_program_mode_active() { return studio; }
void obs_frontend_set_preview_program_mode(bool enable) { studio = enable; preview = program; }
void obs_frontend_set_current_preview_scene(obs_source_t *source) { preview = source; syncSelection(source); }
void obs_frontend_set_current_scene(obs_source_t *source) { ++programChanges; program = source; syncSelection(source); }
void obs_frontend_preview_program_trigger_transition() {
    ++transitionCalls;
    auto *oldProgram = program;
    program = preview;
    preview = oldProgram; // Exercise OBS swap-scenes behavior.
    syncSelection(preview);
}
obs_module_t *obs_current_module(void) { return nullptr; }
char *obs_module_get_config_path(obs_module_t *, const char *) { return nullptr; }
void obs_frontend_save() { if (saveCb) saveCb(saved, true, saveOpaque); }
}

static void choose(QTreeWidget *tree, const QString &name)
{
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        auto *item = tree->topLevelItem(i);
        if (item->text(0) == name) { tree->setCurrentItem(item); return; }
    }
    check(false, "manager row exists");
}
static void click(QDialog *manager, const QString &text)
{
    for (auto *button : manager->findChildren<QPushButton *>()) {
        if (button->text() == text) { button->click(); QApplication::processEvents(); return; }
    }
    check(false, "manager button exists");
}

int main(int argc, char **argv)
{
    QTemporaryDir appConfig;
    qputenv("XDG_CONFIG_HOME", appConfig.path().toUtf8());
    QApplication app(argc, argv);
    check(obs_startup("en-US", nullptr, nullptr), "libobs startup");
    saved = obs_data_create();
    auto *main = obs_scene_create("Main");
    auto *camera = obs_scene_create("Camera");
    auto *overlay = obs_scene_create("Overlay");
    scenes = {main, camera, overlay};
    program = preview = obs_scene_get_source(main);
    auto *nested = obs_scene_add(main, obs_scene_get_source(camera));
    obs_sceneitem_set_visible(nested, true);
    {
        QMainWindow window;
        auto *removeAction = new QAction(&window);
        removeAction->setObjectName("actionRemoveScene");
        sceneWidget = new QListWidget(&window);
        sceneWidget->setObjectName("scenes");
        sceneWidget->addItems({"Main", "Camera", "Overlay"});
        sceneWidget->setCurrentRow(0);
        SceneVault vault(&window);
        vault.showManager();
        auto *manager = window.findChild<QDialog *>();
        auto *tree = manager->findChild<QTreeWidget *>();
        check(tree->topLevelItemCount() == 3, "all scenes discoverable");
        choose(tree, "Camera");
        check(program == obs_scene_get_source(main), "manager selection does not switch output");
        click(manager, "Hide selected");
        check(sceneWidget->item(1)->isHidden(), "secondary hidden from native dock");
        check(!sceneWidget->item(0)->isHidden(), "main remains visible");
        check(obs_sceneitem_visible(nested), "nested source visibility unchanged");
        check(obs_sceneitem_get_source(nested) == obs_scene_get_source(camera), "nested reference preserved");
        auto *state = obs_data_get_obj(saved, "masqrayd-scene-vault");
        auto *hidden = obs_data_get_array(state, "hidden");
        check(obs_data_array_count(hidden) == 1, "hidden UUID saved");
        obs_data_array_release(hidden); obs_data_release(state);

        // Rename without changing the persisted identity.
        obs_source_set_name(obs_scene_get_source(camera), "Camera renamed");
        sceneWidget->item(1)->setText("Camera renamed");
        vault.refresh();
        check(sceneWidget->item(1)->isHidden(), "rename preserves hidden membership");
        choose(tree, "Camera renamed");
        click(manager, "Edit temporarily");
        check(studio && preview == obs_scene_get_source(camera), "editing opens in Studio Preview");
        check(program == obs_scene_get_source(main) && programChanges == 0, "editing never switches Program");
        check(!sceneWidget->item(1)->isHidden(), "editing temporarily reveals entry");
        check(!removeAction->isEnabled(), "native scene deletion disabled during vault edit");
        click(manager, "Finish editing");
        check(preview == obs_scene_get_source(main), "previous Preview restored");
        check(sceneWidget->item(1)->isHidden(), "finished edit re-hides entry");
        check(removeAction->isEnabled(), "main scene deletion restored after edit");

        click(manager, "Show all temporarily");
        check(!sceneWidget->item(1)->isHidden(), "recovery shows hidden scenes");
        click(manager, "Reapply hiding");
        check(sceneWidget->item(1)->isHidden(), "recovery does not erase membership");
        manager->findChild<QLineEdit *>()->setText("renamed");
        check(!tree->topLevelItem(1)->isHidden() && tree->topLevelItem(0)->isHidden(), "search filters manager");
        manager->findChild<QLineEdit *>()->clear();

        // The production controller uses actual Qt mouse events, with a real
        // libobs scene list and a frontend that swaps Preview after transition.
        QTemporaryDir configDir;
        auto *controller = new SceneController(&window, [&] {
            QSet<QString> hiddenIds;
            hiddenIds.insert(QString::fromUtf8(obs_source_get_uuid(obs_scene_get_source(camera))));
            return hiddenIds;
        }, configDir.filePath("controller.ini"));
        controller->openController();
        check(!controller->parentWidget(), "controller remains independent of OBS minimization");
        auto findSceneButton = [&](const QString &name) -> QPushButton * {
            for (auto *b : controller->findChildren<QPushButton *>())
                if (b->property("sceneName").toString() == name && b->isVisible()) return b;
            return nullptr;
        };
        check(!findSceneButton("Camera renamed"), "controller excludes vaulted scenes");
        auto *overlayButton = findSceneButton("Overlay");
        check(overlayButton, "visible production scene has button");
        studio = false; controller->refresh();
        QTest::mouseClick(overlayButton, Qt::LeftButton);
        check(preview == obs_scene_get_source(main) && transitionCalls == 0,
              "Studio Mode off cannot switch live output");
        studio = true; controller->refresh();
        QTest::mouseClick(overlayButton, Qt::LeftButton);
        check(preview == obs_scene_get_source(overlay) && program == obs_scene_get_source(main),
              "single click changes only Preview");
        check(transitionCalls == 0, "single click never triggers transition");
        QTest::mouseDClick(overlayButton, Qt::LeftButton);
        QTest::mouseRelease(overlayButton, Qt::LeftButton);
        check(transitionCalls == 1 && program == obs_scene_get_source(overlay),
              "double click transitions selected scene exactly once");
        check(preview == obs_scene_get_source(main), "double-click release cannot undo Preview swap");
        QTest::mouseDClick(overlayButton, Qt::LeftButton);
        QTest::mouseRelease(overlayButton, Qt::LeftButton);
        check(transitionCalls == 1, "duplicate double click ignored during guard");
        QTest::qWait(400);
        controller->refresh();
        check(overlayButton->property("program").toBool(), "Program button highlight follows output");
        auto *top = controller->findChild<QAction *>("controllerAlwaysOnTop");
        top->setChecked(true);
        check(controller->windowFlags().testFlag(Qt::WindowStaysOnTopHint), "on-top flag enabled");
        top->setChecked(false);
        check(!controller->windowFlags().testFlag(Qt::WindowStaysOnTopHint), "on-top flag disabled");
        top->setChecked(true);
        controller->setAvailable(false);
        QTest::mouseClick(overlayButton, Qt::LeftButton);
        check(preview == obs_scene_get_source(main), "collection change disables scene buttons");
        controller->setAvailable(true);
        obs_source_set_name(obs_scene_get_source(overlay), "Overlay renamed");
        sceneWidget->item(2)->setText("Overlay renamed");
        controller->refresh();
        check(findSceneButton("Overlay renamed") == overlayButton, "rename retains button identity");
        controller->close();
        QSettings controllerSettings(configDir.filePath("controller.ini"), QSettings::IniFormat);
        check(controllerSettings.value("alwaysOnTop").toBool(), "on-top preference persisted");
        check(!controllerSettings.value("geometry").toByteArray().isEmpty(), "geometry persisted");
        delete controller;
        program = preview = obs_scene_get_source(main);
        syncSelection(preview);

        vault.showController();
        SceneController *vaultController = nullptr;
        for (auto *widget : QApplication::topLevelWidgets()) {
            if (widget->objectName() == "masqraydSceneController")
                vaultController = static_cast<SceneController *>(widget);
        }
        check(vaultController, "Tools controller created through Scene Vault");
        auto hasVaultButton = [&] {
            for (auto *b : vaultController->findChildren<QPushButton *>()) {
                if (b->isVisible() && b->property("sceneName").toString() == "Camera renamed") return true;
            }
            return false;
        };
        check(!hasVaultButton(), "live vault membership filters integrated controller");

        // Simulate another controller/hotkey selecting the scene externally.
        preview = obs_scene_get_source(camera); syncSelection(preview); vault.refresh();
        check(!sceneWidget->item(1)->isHidden(), "external active selection remains accessible");
        preview = obs_scene_get_source(main); syncSelection(preview); vault.refresh();
        check(sceneWidget->item(1)->isHidden(), "external selection leaving scene re-hides it");

        // A collection with no saved state must not inherit hidden membership.
        auto *blank = obs_data_create();
        vault.saveLoad(blank, false); QApplication::processEvents(); vault.refresh();
        check(!sceneWidget->item(1)->isHidden(), "collection isolation");
        obs_data_release(blank);
        vault.saveLoad(saved, false); QApplication::processEvents(); vault.refresh();
        check(sceneWidget->item(1)->isHidden(), "saved state loads again");
        choose(tree, "Camera renamed");
        click(manager, "Restore selected");
        check(!sceneWidget->item(1)->isHidden(), "permanent restore");
        check(hasVaultButton(), "permanent restore adds integrated controller button");
        choose(tree, "Camera renamed"); click(manager, "Hide selected");
        check(!hasVaultButton(), "hiding removes integrated controller button");
        vault.shutdown();
        check(!vaultController->isVisible(), "controller closes on OBS shutdown");
        check(!sceneWidget->item(1)->isHidden(), "shutdown restores native dock entries");
    }
    // Exercise a new OBS launch with persisted startup preference, then verify
    // collection changes and repeated load events never reopen a closed window.
    {
        QMainWindow startupWindow;
        sceneWidget = new QListWidget(&startupWindow);
        sceneWidget->setObjectName("scenes");
        sceneWidget->addItems({"Main", "Camera renamed", "Overlay renamed"});
        sceneWidget->setCurrentRow(0);
        SceneVault startupVault(&startupWindow);
        startupVault.showController();
        SceneController *controller = nullptr;
        for (auto *w : QApplication::topLevelWidgets())
            if (w->objectName() == "masqraydSceneController") controller = static_cast<SceneController *>(w);
        check(controller, "startup controller exists");
        controller->findChild<QAction *>("controllerOpenOnStartup")->setChecked(true);
        controller->close();
        startupVault.frontendEvent(OBS_FRONTEND_EVENT_FINISHED_LOADING);
        check(controller->isVisible(), "saved preference opens controller after OBS loads");
        controller->close();
        startupVault.frontendEvent(OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED);
        startupVault.frontendEvent(OBS_FRONTEND_EVENT_FINISHED_LOADING);
        check(!controller->isVisible(), "startup auto-open happens only once per OBS launch");
        controller->findChild<QAction *>("controllerOpenOnStartup")->setChecked(false);
    }
    {
        QMainWindow window;
        SceneVault vault(&window);
        vault.frontendEvent(OBS_FRONTEND_EVENT_FINISHED_LOADING);
        bool exists = false;
        for (auto *w : QApplication::topLevelWidgets())
            if (w->objectName() == "masqraydSceneController") exists = true;
        check(!exists, "disabled startup option leaves controller closed");
    }
    obs_data_release(saved);
    for (auto *scene : scenes) obs_scene_release(scene);
    scenes.clear();
    obs_shutdown();
    std::puts("PASS: real libobs nested scenes, persistence, rename, editing, recovery, collection isolation, unload");
}
