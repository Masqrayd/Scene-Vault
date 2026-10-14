#pragma once
#include <QDialog>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <functional>

class QCheckBox;
class QGridLayout;
class QLabel;
class QPushButton;
class QScrollArea;
class QBoxLayout;
class QSettings;
class SceneButton;

// A separate floating window; Scene Vault supplies its current hidden membership.
class SceneController final : public QDialog {
public:
    SceneController(QWidget *parent, std::function<QSet<QString>()> hidden,
                    const QString &settingsPath);
    ~SceneController() override;
    void refresh();
    void setAvailable(bool available);
    void openController();
    static bool opensOnStartup(const QString &settingsPath);
protected:
    void closeEvent(QCloseEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
private:
    bool selectPreview(const QString &id);
    void transitionScene(const QString &id);
    void transitionPreview();
    void rearrange();
    void saveSettings();
    void keepOnScreen();
    std::function<QSet<QString>()> hidden_;
    QSettings *settings_;
    QCheckBox *onTop_;
    QCheckBox *openOnStartup_;
    QScrollArea *scroll_;
    QBoxLayout *bottom_;
    int layoutColumns_ = 0;
    int layoutHeight_ = 0;
    QStringList layoutOrder_;
    QGridLayout *grid_;
    QLabel *status_;
    QPushButton *transition_;
    QPushButton *studio_;
    QHash<QString, SceneButton *> buttons_;
    QStringList order_;
    bool available_ = true;
    bool restoring_ = true;
    bool transitionPending_ = false;
};
