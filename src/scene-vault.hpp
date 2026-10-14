#pragma once
#include "vault-state.hpp"
#include <obs-frontend-api.h>
#include <QPointer>
#include <QObject>
#include <QTimer>
#include <QPoint>

class SceneController;
class QDialog;
class QLineEdit;
class QCheckBox;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QTreeWidget;
class QTreeWidgetItem;
class QAction;
class QPushButton;
class QMainWindow;

class SceneVault final : public QObject {
public:
    explicit SceneVault(QMainWindow *window);
    ~SceneVault() override;
    void showManager();
    void showController();
    void refresh();
    void saveLoad(obs_data_t *data, bool saving);
    void frontendEvent(obs_frontend_event event);
    void shutdown();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    static void eventCallback(obs_frontend_event event, void *data);
    static void saveCallback(obs_data_t *data, bool saving, void *opaque);
    void bindSceneList();
    void applyVisibility();
    void updateDeleteProtection();
    void rebuildManager();
    void filterManager();
    void hideScene(const QString &id);
    void restoreScene(const QString &id);
    void beginEdit(const QString &id);
    void finishEdit();
    void restoreAll();
    void contextMenu(const QString &id, const QPoint &position);
    QString selectedId() const;
    QString idForItem(QListWidgetItem *item) const;
    QSet<QString> activeIds() const;
    QSet<QString> existingIds() const;
    void persist();
    void tell(const QString &message);

    QPointer<QMainWindow> window_;
    QPointer<QListWidget> sceneList_;
    QPointer<QDialog> manager_;
    SceneController *controller_ = nullptr;
    QPointer<QAction> controllerAction_;
    QTreeWidget *tree_ = nullptr;
    QLineEdit *search_ = nullptr;
    QCheckBox *hiddenOnly_ = nullptr;
    QLabel *status_ = nullptr;
    QPointer<QAction> toolsAction_;
    QPointer<QAction> removeAction_;
    QPushButton *showAll_ = nullptr;
    QTimer timer_;
    VaultState state_;
    QSet<QListWidgetItem *> hiddenItems_;
    bool shuttingDown_ = false;
    bool collectionChanging_ = false;
    bool refreshing_ = false;
    bool deleteProtected_ = false;
    bool removeWasEnabled_ = true;
};
