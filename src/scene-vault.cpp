#include "scene-vault.hpp"
#include <obs-module.h>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QContextMenuEvent>
#include <QDialog>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QScopedValueRollback>
#include <QScrollBar>

namespace {
constexpr const char *storageKey = "masqrayd-scene-vault";
QString uuid(obs_source_t *source)
{
    return source ? QString::fromUtf8(obs_source_get_uuid(source)) : QString();
}
struct SceneList {
    obs_frontend_source_list list{};
    SceneList() { obs_frontend_get_scenes(&list); }
    ~SceneList() { obs_frontend_source_list_free(&list); }
};
struct Source {
    obs_source_t *ptr;
    explicit Source(obs_source_t *source) : ptr(source) {}
    ~Source() { if (ptr) obs_source_release(ptr); }
};
}

SceneVault::SceneVault(QMainWindow *window) : QObject(window), window_(window)
{
    toolsAction_ = static_cast<QAction *>(obs_frontend_add_tools_menu_qaction("Scene Vault…"));
    if (toolsAction_) connect(toolsAction_.data(), &QAction::triggered, this, &SceneVault::showManager);
    obs_frontend_add_event_callback(eventCallback, this);
    obs_frontend_add_save_callback(saveCallback, this);
    connect(&timer_, &QTimer::timeout, this, &SceneVault::refresh);
    timer_.start(500);
    bindSceneList();
}

SceneVault::~SceneVault()
{
    shutdown();
    if (manager_) delete manager_.data();
    if (toolsAction_) delete toolsAction_.data();
}

void SceneVault::shutdown()
{
    if (shuttingDown_) return;
    shuttingDown_ = true;
    timer_.stop();
    if (removeAction_ && deleteProtected_) removeAction_->setEnabled(removeWasEnabled_);
    obs_frontend_remove_event_callback(eventCallback, this);
    obs_frontend_remove_save_callback(saveCallback, this);
    // Restore only entries we hid. Never remove scene objects or list entries.
    if (sceneList_) {
        sceneList_->viewport()->removeEventFilter(this);
        sceneList_->removeEventFilter(this);
        for (int i = 0; i < sceneList_->count(); ++i) {
            auto *item = sceneList_->item(i);
            if (hiddenItems_.contains(item)) item->setHidden(false);
        }
    }
    hiddenItems_.clear();
}

void SceneVault::eventCallback(obs_frontend_event event, void *data)
{
    auto *self = static_cast<SceneVault *>(data);
    if (event == OBS_FRONTEND_EVENT_EXIT) {
        self->shutdown();
        return;
    }
    // Scene callbacks can originate outside the GUI thread.
    QMetaObject::invokeMethod(self, [self, event] { self->frontendEvent(event); }, Qt::AutoConnection);
}

void SceneVault::saveCallback(obs_data_t *data, bool saving, void *opaque)
{
    static_cast<SceneVault *>(opaque)->saveLoad(data, saving);
}

void SceneVault::saveLoad(obs_data_t *data, bool saving)
{
    if (saving) {
        auto *root = obs_data_create();
        obs_data_set_int(root, "schema_version", 1);
        auto *array = obs_data_array_create();
        QStringList ids = state_.hidden.values();
        ids.sort();
        for (const auto &id : ids) {
            auto *entry = obs_data_create();
            obs_data_set_string(entry, "uuid", id.toUtf8().constData());
            obs_data_array_push_back(array, entry);
            obs_data_release(entry);
        }
        obs_data_set_array(root, "hidden", array);
        obs_data_set_obj(data, storageKey, root);
        obs_data_array_release(array);
        obs_data_release(root);
        return;
    }
    state_.hidden.clear();
    state_.resetSession();
    auto *root = obs_data_get_obj(data, storageKey);
    if (root) {
        if (obs_data_get_int(root, "schema_version") == 1) {
            auto *array = obs_data_get_array(root, "hidden");
            if (array) {
                for (size_t i = 0; i < obs_data_array_count(array); ++i) {
                    auto *entry = obs_data_array_item(array, i);
                    auto id = QString::fromUtf8(obs_data_get_string(entry, "uuid"));
                    if (!id.isEmpty()) state_.hidden.insert(id);
                    obs_data_release(entry);
                }
                obs_data_array_release(array);
            }
        }
        obs_data_release(root);
    }
    // OBS may still be rebuilding its scene list during the load callback.
    QTimer::singleShot(0, this, &SceneVault::refresh);
}

void SceneVault::frontendEvent(obs_frontend_event event)
{
    if (shuttingDown_) return;
    if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING) {
        collectionChanging_ = true;
        state_.resetSession();
        return;
    }
    if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP) {
        state_.hidden.clear();
        hiddenItems_.clear();
        if (tree_) tree_->clear();
        return;
    }
    if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED ||
        event == OBS_FRONTEND_EVENT_FINISHED_LOADING) collectionChanging_ = false;
    refresh();
}

void SceneVault::bindSceneList()
{
    if (sceneList_ || !window_) return;
    sceneList_ = window_->findChild<QListWidget *>("scenes");
    if (!sceneList_) return; // Compatibility failure is deliberately fail-open.
    hiddenItems_.clear();
    sceneList_->viewport()->installEventFilter(this);
    sceneList_->installEventFilter(this);
    removeAction_ = window_->findChild<QAction *>("actionRemoveScene");
    connect(sceneList_, &QListWidget::currentItemChanged, this, [this] { refresh(); });
    connect(sceneList_, &QListWidget::itemSelectionChanged, this, [this] { refresh(); });
    connect(sceneList_->model(), &QAbstractItemModel::rowsAboutToBeRemoved, this,
            [this](const QModelIndex &, int first, int last) {
                if (!sceneList_) return;
                for (int row = first; row <= last; ++row)
                    hiddenItems_.remove(sceneList_->item(row));
            });
}

QString SceneVault::idForItem(QListWidgetItem *item) const
{
    if (!item) return {};
    // Use supported source lookup instead of decoding private Qt metatypes.
    Source source(obs_get_source_by_name(item->text().toUtf8().constData()));
    return source.ptr && obs_source_is_scene(source.ptr) ? uuid(source.ptr) : QString();
}

QSet<QString> SceneVault::existingIds() const
{
    SceneList scenes;
    QSet<QString> ids;
    for (size_t i = 0; i < scenes.list.sources.num; ++i) ids.insert(uuid(scenes.list.sources.array[i]));
    ids.remove(QString());
    return ids;
}

QSet<QString> SceneVault::activeIds() const
{
    QSet<QString> ids;
    Source program(obs_frontend_get_current_scene());
    ids.insert(uuid(program.ptr));
    if (obs_frontend_preview_program_mode_active()) {
        Source preview(obs_frontend_get_current_preview_scene());
        ids.insert(uuid(preview.ptr));
    }
    if (sceneList_) ids.insert(idForItem(sceneList_->currentItem()));
    ids.remove(QString());
    return ids;
}

void SceneVault::applyVisibility()
{
    if (!sceneList_) return;
    auto active = activeIds();
    for (int i = 0; i < sceneList_->count(); ++i) {
        auto *item = sceneList_->item(i);
        const auto id = idForItem(item);
        if (id.isEmpty()) continue;
        const bool hide = state_.shouldHide(id, active);
        if (hide) {
            // A hidden selection must not remain a Delete-key target.
            item->setSelected(false);
            item->setHidden(true);
            hiddenItems_.insert(item);
        } else if (hiddenItems_.remove(item)) {
            item->setHidden(false);
        }
    }
}

void SceneVault::refresh()
{
    if (shuttingDown_ || collectionChanging_ || refreshing_) return;
    QScopedValueRollback<bool> guard(refreshing_, true);
    bindSceneList();
    applyVisibility();
    updateDeleteProtection();
    if (manager_ && manager_->isVisible()) rebuildManager();
}

void SceneVault::updateDeleteProtection()
{
    if (!sceneList_ || !removeAction_) return;
    bool protect = state_.hidden.contains(idForItem(sceneList_->currentItem()));
    for (auto *item : sceneList_->selectedItems())
        protect = protect || state_.hidden.contains(idForItem(item));
    if (protect) {
        if (!deleteProtected_) removeWasEnabled_ = removeAction_->isEnabled();
        deleteProtected_ = true;
        removeAction_->setEnabled(false);
    } else if (deleteProtected_) {
        deleteProtected_ = false;
        removeAction_->setEnabled(removeWasEnabled_);
    }
}

void SceneVault::showManager()
{
    if (!manager_) {
        manager_ = new QDialog(window_);
        manager_->setWindowTitle("Scene Vault");
        manager_->resize(700, 510);
        auto *layout = new QVBoxLayout(manager_);
        auto *intro = new QLabel("Hide secondary scenes from the Scenes dock. Nested uses keep working.\n"
                                "To hide a scene, first switch Preview and Program to your main scenes.\n"
                                "Edits to shared sources also affect their uses in other scenes, including live output.", manager_);
        intro->setWordWrap(true);
        layout->addWidget(intro);
        search_ = new QLineEdit(manager_);
        search_->setPlaceholderText("Find a scene…");
        search_->setClearButtonEnabled(true);
        layout->addWidget(search_);
        hiddenOnly_ = new QCheckBox("Only show scenes in the vault", manager_);
        layout->addWidget(hiddenOnly_);
        tree_ = new QTreeWidget(manager_);
        tree_->setColumnCount(2);
        tree_->setHeaderLabels({"Scene", "Status"});
        tree_->setRootIsDecorated(false);
        tree_->setSelectionMode(QAbstractItemView::SingleSelection);
        tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        tree_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        layout->addWidget(tree_);
        auto *buttons = new QHBoxLayout;
        auto add = [&](const QString &label, auto callback) {
            auto *button = new QPushButton(label, manager_);
            buttons->addWidget(button);
            connect(button, &QPushButton::clicked, this, callback);
        };
        add("Hide selected", [this] { hideScene(selectedId()); });
        add("Edit temporarily", [this] { beginEdit(selectedId()); });
        add("Finish editing", [this] { finishEdit(); });
        add("Restore selected", [this] { restoreScene(selectedId()); });
        layout->addLayout(buttons);
        auto *bottom = new QHBoxLayout;
        auto *showAll = new QPushButton("Show all temporarily", manager_);
        showAll_ = showAll;
        auto *restore = new QPushButton("Restore all permanently", manager_);
        bottom->addWidget(showAll);
        bottom->addWidget(restore);
        layout->addLayout(bottom);
        status_ = new QLabel(manager_);
        status_->setWordWrap(true);
        layout->addWidget(status_);
        connect(search_, &QLineEdit::textChanged, this, [this] { filterManager(); });
        connect(hiddenOnly_, &QCheckBox::toggled, this, [this] { filterManager(); });
        connect(showAll, &QPushButton::clicked, this, [this, showAll] {
            state_.suspended = !state_.suspended;
            showAll->setText(state_.suspended ? "Reapply hiding" : "Show all temporarily");
            refresh();
        });
        connect(restore, &QPushButton::clicked, this, &SceneVault::restoreAll);
        // No scene switching or deletion is bound to selecting a manager row.
    }
    manager_->show();
    manager_->raise();
    manager_->activateWindow();
    refresh();
}

QString SceneVault::selectedId() const
{
    auto *item = tree_ ? tree_->currentItem() : nullptr;
    return item ? item->data(0, Qt::UserRole).toString() : QString();
}

void SceneVault::rebuildManager()
{
    showAll_->setText(state_.suspended ? "Reapply hiding" : "Show all temporarily");
    const auto selected = selectedId();
    const int scroll = tree_->verticalScrollBar()->value();
    SceneList scenes;
    const auto active = activeIds();
    tree_->clear();
    int hiddenCount = 0;
    for (size_t i = 0; i < scenes.list.sources.num; ++i) {
        auto *source = scenes.list.sources.array[i];
        auto id = uuid(source);
        const bool hidden = state_.hidden.contains(id);
        if (hidden) ++hiddenCount;
        QString status = hidden ? "Hidden" : "Main list";
        if (hidden && state_.suspended) status = "Vault · temporarily shown";
        if (hidden && active.contains(id)) status = "Vault · active, temporarily shown";
        if (state_.editing == id) status = "Editing in Preview";
        auto *item = new QTreeWidgetItem(tree_, {QString::fromUtf8(obs_source_get_name(source)), status});
        item->setData(0, Qt::UserRole, id);
        item->setData(1, Qt::UserRole, hidden);
        if (selected == id) tree_->setCurrentItem(item);
    }
    filterManager();
    tree_->verticalScrollBar()->setValue(scroll);
    status_->setText(sceneList_ ? QString("%1 scene(s) in the vault. Selecting a row here does not switch scenes.").arg(hiddenCount)
                              : "Scenes dock not recognized. Hiding is disabled; all scenes remain accessible.");
}

void SceneVault::filterManager()
{
    if (!tree_) return;
    for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
        auto *item = tree_->topLevelItem(i);
        const bool match = item->text(0).contains(search_->text(), Qt::CaseInsensitive);
        const bool included = !hiddenOnly_->isChecked() || item->data(1, Qt::UserRole).toBool();
        item->setHidden(!match || !included);
    }
}

void SceneVault::tell(const QString &message)
{
    QMessageBox::information(manager_ ? static_cast<QWidget *>(manager_.data()) : window_.data(), "Scene Vault", message);
}

void SceneVault::persist()
{
    obs_frontend_save();
    refresh();
}

void SceneVault::hideScene(const QString &id)
{
    if (id.isEmpty()) return;
    if (!sceneList_) { tell("This OBS Scenes dock is not supported. No scenes have been hidden."); return; }
    if (!state_.mayHide(id, existingIds(), activeIds())) {
        tell("Keep at least one main scene visible, and switch Preview and Program away from this scene before hiding it.");
        return;
    }
    state_.hidden.insert(id);
    persist();
}

void SceneVault::restoreScene(const QString &id)
{
    if (id.isEmpty()) return;
    state_.hidden.remove(id);
    persist();
}

void SceneVault::beginEdit(const QString &id)
{
    if (id.isEmpty() || !state_.hidden.contains(id)) {
        tell("Choose a scene in the vault to edit temporarily."); return;
    }
    if (!state_.editing.isEmpty()) {
        tell("Finish your current edit before opening another hidden scene."); return;
    }
    Source scene(obs_get_source_by_uuid(id.toUtf8().constData()));
    if (!scene.ptr || !obs_source_is_scene(scene.ptr)) return;
    // Always use Studio Mode. Enabling it keeps the current Program scene intact.
    if (!obs_frontend_preview_program_mode_active()) obs_frontend_set_preview_program_mode(true);
    if (!obs_frontend_preview_program_mode_active()) {
        tell("Enable Studio Mode in OBS before editing this scene."); return;
    }
    Source preview(obs_frontend_get_current_preview_scene());
    state_.returnPreview = uuid(preview.ptr);
    state_.editing = id;
    applyVisibility();
    obs_frontend_set_current_preview_scene(scene.ptr);
    refresh();
}

void SceneVault::finishEdit()
{
    if (state_.editing.isEmpty()) return;
    // Avoid the Program-changing setter when Studio Mode has been turned off.
    if (!obs_frontend_preview_program_mode_active()) {
        tell("Re-enable Studio Mode, then finish editing. Your Program scene has not been changed by Scene Vault.");
        return;
    }
    Source current(obs_frontend_get_current_preview_scene());
    if (uuid(current.ptr) == state_.editing) {
        Source previous(obs_get_source_by_uuid(state_.returnPreview.toUtf8().constData()));
        if (!previous.ptr) {
            tell("The previous Preview scene is no longer available. Select a main scene in Preview, then finish editing.");
            return;
        }
        obs_frontend_set_current_preview_scene(previous.ptr);
    }
    state_.editing.clear();
    state_.returnPreview.clear();
    persist();
}

void SceneVault::restoreAll()
{
    if (QMessageBox::question(manager_, "Restore all scenes", "Permanently return every hidden scene to the Scenes dock?")
        != QMessageBox::Yes) return;
    state_.hidden.clear();
    // Preserve an edit's return scene until Finish editing is used.
    persist();
}

void SceneVault::contextMenu(const QString &id, const QPoint &position)
{
    QMenu menu(window_);
    auto *hide = menu.addAction("Hide in Scene Vault");
    hide->setEnabled(state_.mayHide(id, existingIds(), activeIds()));
    auto *restore = menu.addAction("Restore to Scenes");
    restore->setEnabled(state_.hidden.contains(id));
    menu.addSeparator();
    auto *manager = menu.addAction("Manage Scene Vault…");
    auto *picked = menu.exec(position);
    if (picked == hide) hideScene(id);
    else if (picked == restore) restoreScene(id);
    else if (picked == manager) showManager();
}

bool SceneVault::eventFilter(QObject *watched, QEvent *event)
{
    // Shift + right-click leaves OBS's normal right-click scene actions intact.
    if (sceneList_ && event->type() == QEvent::ContextMenu &&
        (watched == sceneList_ || watched == sceneList_->viewport()) &&
        (QApplication::keyboardModifiers() & Qt::ShiftModifier)) {
        auto *context = static_cast<QContextMenuEvent *>(event);
        auto *item = sceneList_->itemAt(sceneList_->viewport()->mapFromGlobal(context->globalPos()));
        if (item) {
            contextMenu(idForItem(item), context->globalPos());
            return true;
        }
    }
    return QObject::eventFilter(watched, event);
}
