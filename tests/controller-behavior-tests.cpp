#include "scene-controller.hpp"
#include <obs-frontend-api.h>
#include <util/bmem.h>
#include <QApplication>
#include <QCheckBox>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest/QTest>
#include <cstdio>
#include <cstdlib>

// The controller needs no renderer. Simulate only the frontend/source contract
// so mouse handling can run offscreen; real scene integration is a separate test.
struct FakeScene { const char *id; const char *name; };
static FakeScene mainScene{"main-id", "Main"}, camera{"camera-id", "Camera"}, overlay{"overlay-id", "Overlay"};
static obs_source_t *source(FakeScene &scene) { return reinterpret_cast<obs_source_t *>(&scene); }
static obs_source_t *program = source(mainScene), *preview = source(mainScene);
static bool studio = true;
static int transitions = 0, previewChanges = 0;
static bool showOverlay = true;
extern "C" {
const char *obs_source_get_uuid(const obs_source_t *p) { return reinterpret_cast<const FakeScene *>(p)->id; }
const char *obs_source_get_name(const obs_source_t *p) { return reinterpret_cast<const FakeScene *>(p)->name; }
bool obs_source_is_scene(const obs_source_t *) { return true; }
void obs_source_release(obs_source_t *) {}
obs_source_t *obs_get_source_by_uuid(const char *id) {
    for (auto *p : {source(mainScene), source(camera), source(overlay)})
        if (QString::fromUtf8(obs_source_get_uuid(p)) == QString::fromUtf8(id) && (p != source(overlay) || showOverlay)) return p;
    return nullptr;
}
void obs_frontend_get_scenes(obs_frontend_source_list *out) {
    out->sources.num = out->sources.capacity = showOverlay ? 3 : 2;
    out->sources.array = static_cast<obs_source_t **>(bmalloc(3 * sizeof(obs_source_t *)));
    out->sources.array[0] = source(mainScene); out->sources.array[1] = source(camera); out->sources.array[2] = source(overlay);
}
#if LIBOBS_API_MAJOR_VER >= 32
void obs_frontend_source_list_free(obs_frontend_source_list *out) { bfree(out->sources.array); *out = {}; }
#endif
obs_source_t *obs_frontend_get_current_scene() { return program; }
obs_source_t *obs_frontend_get_current_preview_scene() { return preview; }
bool obs_frontend_preview_program_mode_active() { return studio; }
void obs_frontend_set_preview_program_mode(bool value) { studio = value; }
void obs_frontend_set_current_preview_scene(obs_source_t *p) { ++previewChanges; preview = p; }
void obs_frontend_preview_program_trigger_transition() { ++transitions; std::swap(program, preview); }
}
static void check(bool ok, const char *message) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QTemporaryDir config;
    QSet<QString> hidden{"camera-id"};
    const auto settingsPath = config.filePath("controller.ini");
    {
        SceneController controller(nullptr, [&] { return hidden; }, settingsPath);
        controller.openController();
        const auto previewPath = qEnvironmentVariable("SCENE_CONTROLLER_PREVIEW");
        if (!previewPath.isEmpty()) controller.grab().save(previewPath);
        auto find = [&](const char *id) -> QPushButton * {
            for (auto *b : controller.findChildren<QPushButton *>())
                if (b->property("sceneUuid").toString() == QString::fromUtf8(id) && b->isVisible()) return b;
            return nullptr;
        };
        check(!find("camera-id") && find("main-id") && find("overlay-id"), "hidden membership filters buttons");
        check(!controller.parentWidget(), "floating window independent of main OBS window");
        auto *button = find("overlay-id");
        studio = false; controller.refresh();
        QTest::mouseClick(button, Qt::LeftButton);
        check(previewChanges == 0 && transitions == 0, "Studio Mode off disables selection");
        auto *studioButton = controller.findChild<QPushButton *>("controllerStudioMode");
        check(studioButton->text().contains("OFF"), "Studio button reports OFF");
        QTest::mouseClick(studioButton, Qt::LeftButton);
        check(studio && studioButton->text().contains("ON"), "one click enables Studio Mode and updates label");
        QTest::mouseClick(studioButton, Qt::LeftButton);
        check(!studio && studioButton->text().contains("OFF"), "one click disables Studio Mode and updates label");
        QTest::mouseClick(studioButton, Qt::LeftButton);
        check(studio, "Studio Mode can be re-enabled");
        auto *sendButton = controller.findChild<QPushButton *>("controllerSendPreview");
        check(sendButton->text() == "Send Preview to Program", "transition action clearly names its direction");
        QTest::mouseClick(button, Qt::LeftButton);
        check(preview == source(overlay) && program == source(mainScene) && transitions == 0, "single click changes only Preview");
        QTest::mouseDClick(button, Qt::LeftButton);
        QTest::mouseRelease(button, Qt::LeftButton);
        check(transitions == 1 && program == source(overlay) && preview == source(mainScene), "double-click transitions once without undoing swap");
        QTest::mouseDClick(button, Qt::LeftButton);
        QTest::mouseRelease(button, Qt::LeftButton);
        check(transitions == 1, "duplicate double click suppressed");
        QTest::qWait(400);
        controller.refresh();
        check(button->property("program").toBool(), "Program highlight tracks output");
        overlay.name = "Overlay renamed"; controller.refresh();
        check(find("overlay-id") == button && button->text().startsWith("Overlay renamed"), "rename preserves identity");
        hidden.remove("camera-id"); controller.refresh();
        check(find("camera-id"), "restore makes button available");
        hidden.insert("camera-id"); controller.refresh();
        check(!find("camera-id"), "hide removes button");
        controller.setAvailable(false);
        const auto before = previewChanges;
        QTest::mouseClick(button, Qt::LeftButton);
        check(previewChanges == before, "collection change blocks clicks");
        controller.setAvailable(true);
        auto *top = controller.findChild<QCheckBox *>("controllerAlwaysOnTop");
        top->setChecked(true);
        check(controller.windowFlags().testFlag(Qt::WindowStaysOnTopHint), "on-top flag enabled");
        top->setChecked(false);
        check(!controller.windowFlags().testFlag(Qt::WindowStaysOnTopHint), "on-top flag disabled");
        top->setChecked(true);
        controller.close();
        QSettings saved(settingsPath, QSettings::IniFormat);
        check(saved.value("alwaysOnTop").toBool() && !saved.value("geometry").toByteArray().isEmpty(), "settings persisted");
        showOverlay = false; controller.refresh();
        check(!find("overlay-id"), "deleted scene removes button");
    }
    {
        SceneController restored(nullptr, [&] { return hidden; }, settingsPath);
        check(restored.windowFlags().testFlag(Qt::WindowStaysOnTopHint), "on-top preference restored");
    }
    std::puts("PASS: offscreen Qt mouse events, Preview/Program swap, vault filtering, rename, deletion, collection guard, on-top and persistence");
}
