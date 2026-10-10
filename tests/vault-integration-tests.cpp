#include "scene-vault.hpp"
#include <obs.h>
#include <util/bmem.h>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
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
        choose(tree, "Camera renamed"); click(manager, "Hide selected");
        vault.shutdown();
        check(!sceneWidget->item(1)->isHidden(), "shutdown restores native dock entries");
    }
    obs_data_release(saved);
    for (auto *scene : scenes) obs_scene_release(scene);
    scenes.clear();
    obs_shutdown();
    std::puts("PASS: real libobs nested scenes, persistence, rename, editing, recovery, collection isolation, unload");
}
