#include "scene-controller.hpp"
#include <obs-frontend-api.h>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QGridLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QSpinBox>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

namespace {
struct Source {
    obs_source_t *ptr;
    explicit Source(obs_source_t *p) : ptr(p) {}
    ~Source() { if (ptr) obs_source_release(ptr); }
};
QString sourceId(obs_source_t *source)
{
    return source ? QString::fromUtf8(obs_source_get_uuid(source)) : QString();
}
}

class SceneButton final : public QPushButton {
public:
    using QPushButton::QPushButton;
    std::function<void()> doubleClick;
protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && isEnabled()) {
            // Consume the second press and its release: no trailing clicked signal
            // may change Preview after OBS swaps Preview and Program.
            setDown(false);
            suppressRelease_ = true;
            if (doubleClick) doubleClick();
            event->accept();
            return;
        }
        QPushButton::mouseDoubleClickEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (suppressRelease_ && event->button() == Qt::LeftButton) {
            suppressRelease_ = false;
            setDown(false);
            event->accept();
            return;
        }
        QPushButton::mouseReleaseEvent(event);
    }
private:
    bool suppressRelease_ = false;
};

SceneController::SceneController(QWidget *parent, std::function<QSet<QString>()> hidden,
                                 const QString &settingsPath)
    : QDialog(nullptr, Qt::Window), hidden_(std::move(hidden)),
      settings_(new QSettings(settingsPath, QSettings::IniFormat, this))
{
    Q_UNUSED(parent);
    // Unowned top-level window remains available when OBS is minimized.
    // SceneVault explicitly owns/deletes it and hides it during OBS shutdown.
    setObjectName("masqraydSceneController");
    setWindowTitle("Masqrayd Scene Controller");
    setMinimumSize(320, 240);
    resize(600, 430);
    auto *layout = new QVBoxLayout(this);
    auto *toolbar = new QHBoxLayout;
    onTop_ = new QCheckBox("Always on top", this);
    onTop_->setObjectName("controllerAlwaysOnTop");
    columns_ = new QSpinBox(this);
    columns_->setObjectName("controllerColumns");
    columns_->setRange(1, 6);
    columns_->setValue(settings_->value("columns", 3).toInt());
    buttonHeight_ = new QSpinBox(this);
    buttonHeight_->setObjectName("controllerButtonHeight");
    buttonHeight_->setRange(48, 160);
    buttonHeight_->setValue(settings_->value("buttonHeight", 80).toInt());
    toolbar->addWidget(onTop_);
    toolbar->addStretch();
    toolbar->addWidget(new QLabel("Columns", this));
    toolbar->addWidget(columns_);
    toolbar->addWidget(new QLabel("Height", this));
    toolbar->addWidget(buttonHeight_);
    layout->addLayout(toolbar);
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    auto *panel = new QWidget(scroll);
    grid_ = new QGridLayout(panel);
    grid_->setAlignment(Qt::AlignTop);
    scroll->setWidget(panel);
    layout->addWidget(scroll, 1);
    auto *bottom = new QHBoxLayout;
    studio_ = new QPushButton("Studio Mode: OFF — click to enable", this);
    studio_->setObjectName("controllerStudioMode");
    studio_->setAutoDefault(false);
    transition_ = new QPushButton("Send Preview to Program", this);
    transition_->setAutoDefault(false);
    transition_->setObjectName("controllerSendPreview");
    transition_->setToolTip("Send the current Preview scene to live Program using your OBS transition. OBS may swap Preview and Program afterward.");
    bottom->addWidget(studio_);
    bottom->addWidget(transition_);
    layout->addLayout(bottom);
    status_ = new QLabel(this);
    status_->setObjectName("controllerStatus");
    status_->setWordWrap(true);
    layout->addWidget(status_);
    setStyleSheet("QPushButton[sceneButton=\"true\"] { padding: 8px; font-size: 14px; border: 2px solid #777; border-radius: 6px; }"
                  "QPushButton[sceneButton=\"true\"][preview=\"true\"] { border-color: #e6b84a; }"
                  "QPushButton[sceneButton=\"true\"][program=\"true\"] { border-color: #ea6565; }"
                  "QPushButton[sceneButton=\"true\"][preview=\"true\"][program=\"true\"] { border-color: #c487ed; }"
                  "QPushButton#controllerSendPreview:enabled { background-color: #e6b84a; color: #161616; border: 2px solid #e6b84a; border-radius: 4px; padding: 6px; }"
                  "QPushButton#controllerSendPreview:disabled { color: #888; border: 2px solid #888; border-radius: 4px; padding: 6px; }");
    connect(studio_, &QPushButton::clicked, this, [this] {
        if (!available_) return;
        obs_frontend_set_preview_program_mode(!obs_frontend_preview_program_mode_active());
        refresh();
        QTimer::singleShot(100, this, &SceneController::refresh);
    });
    connect(transition_, &QPushButton::clicked, this, &SceneController::transitionPreview);
    connect(columns_, qOverload<int>(&QSpinBox::valueChanged), this, [this] { rearrange(); saveSettings(); });
    connect(buttonHeight_, qOverload<int>(&QSpinBox::valueChanged), this, [this] { rearrange(); saveSettings(); });
    connect(onTop_, &QCheckBox::toggled, this, [this](bool enabled) {
        const auto geometry = saveGeometry();
        const bool visible = isVisible();
        setWindowFlag(Qt::WindowStaysOnTopHint, enabled);
        restoreGeometry(geometry);
        if (visible) { show(); raise(); }
        saveSettings();
    });
    onTop_->setChecked(settings_->value("alwaysOnTop", false).toBool());
    restoreGeometry(settings_->value("geometry").toByteArray());
    keepOnScreen();
    restoring_ = false;
    refresh();
}

SceneController::~SceneController() { saveSettings(); }

void SceneController::keepOnScreen()
{
    for (auto *screen : QGuiApplication::screens()) {
        if (screen->availableGeometry().contains(frameGeometry().topLeft() + QPoint(30, 15))) return;
    }
    if (auto *screen = QGuiApplication::primaryScreen()) {
        const auto area = screen->availableGeometry();
        resize(qMin(width(), area.width()), qMin(height(), area.height()));
        move(area.topLeft() + QPoint(20, 20));
    }
}

void SceneController::openController()
{
    keepOnScreen();
    show();
    raise();
    activateWindow();
    refresh();
}

void SceneController::setAvailable(bool available)
{
    available_ = available;
    if (!available) {
        transitionPending_ = false;
        for (auto *button : buttons_) button->setEnabled(false);
        transition_->setEnabled(false);
        studio_->setEnabled(false);
        status_->setText("Loading scene collection…");
    } else refresh();
}

void SceneController::refresh()
{
    if (!available_) return;
    const auto hidden = hidden_();
    obs_frontend_source_list list{};
    obs_frontend_get_scenes(&list);
    QStringList nextOrder;
    for (size_t i = 0; i < list.sources.num; ++i) {
        auto *source = list.sources.array[i];
        const auto id = sourceId(source);
        if (id.isEmpty() || hidden.contains(id)) continue;
        nextOrder.append(id);
        auto *button = buttons_.value(id, nullptr);
        if (!button) {
            button = new SceneButton(this);
            button->setAutoDefault(false);
            button->setDefault(false);
            button->setProperty("sceneButton", true);
            button->setProperty("sceneUuid", id);
            buttons_.insert(id, button);
            connect(button, &QPushButton::clicked, this, [this, id] { selectPreview(id); });
            button->doubleClick = [this, id] { transitionScene(id); };
        }
        button->setProperty("sceneName", QString::fromUtf8(obs_source_get_name(source)));
    }
    obs_frontend_source_list_free(&list);
    for (const auto &id : buttons_.keys()) {
        if (!nextOrder.contains(id)) {
            auto *button = buttons_.take(id);
            grid_->removeWidget(button);
            button->hide();
            button->deleteLater();
        }
    }
    if (order_ != nextOrder) { order_ = nextOrder; rearrange(); }
    const bool studio = obs_frontend_preview_program_mode_active();
    studio_->setEnabled(true);
    studio_->setText(studio ? "Studio Mode: ON — click to disable" : "Studio Mode: OFF — click to enable");
    Source program(obs_frontend_get_current_scene());
    Source preview(studio ? obs_frontend_get_current_preview_scene() : nullptr);
    const auto programId = sourceId(program.ptr), previewId = sourceId(preview.ptr);
    for (const auto &id : order_) {
        auto *button = buttons_.value(id);
        const bool live = id == programId, queued = id == previewId;
        const auto name = button->property("sceneName").toString();
        const auto label = button->fontMetrics().elidedText(name, Qt::ElideRight, qMax(32, button->width() - 24));
        button->setText(label +
                        (live && queued ? "\nPROGRAM + PREVIEW" : live ? "\nPROGRAM" : queued ? "\nPREVIEW" : ""));
        const bool changed = button->property("program").toBool() != live || button->property("preview").toBool() != queued;
        button->setProperty("program", live);
        button->setProperty("preview", queued);
        if (changed) { button->style()->unpolish(button); button->style()->polish(button); }
        button->setEnabled(studio && !transitionPending_);
        button->setToolTip(name + "\nSingle-click: Preview. Double-click: transition to Program.");
    }
    transition_->setEnabled(studio && !transitionPending_ && preview.ptr && !hidden.contains(previewId));
    status_->setText(studio ? (order_.isEmpty() ? "No production scenes available." :
                     "Single-click: Preview • Double-click: transition • Red: Program • Gold: Preview") :
                     "Studio Mode is OFF. Click the Studio Mode button to enable Preview selection and double-click transitions.");
}

void SceneController::rearrange()
{
    while (auto *item = grid_->takeAt(0)) delete item;
    for (int col = 0; col < 6; ++col) grid_->setColumnStretch(col, col < columns_->value() ? 1 : 0);
    for (int i = 0; i < order_.size(); ++i) {
        auto *button = buttons_.value(order_[i]);
        button->setMinimumHeight(buttonHeight_->value());
        button->setMinimumWidth(0);
        button->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        grid_->addWidget(button, i / columns_->value(), i % columns_->value());
        button->show();
    }
}

bool SceneController::selectPreview(const QString &id)
{
    if (!available_ || transitionPending_ || !obs_frontend_preview_program_mode_active() || hidden_().contains(id)) return false;
    Source source(obs_get_source_by_uuid(id.toUtf8().constData()));
    if (!source.ptr || !obs_source_is_scene(source.ptr)) return false;
    obs_frontend_set_current_preview_scene(source.ptr);
    refresh();
    return true;
}

void SceneController::transitionScene(const QString &id)
{
    // Resolve the clicked UUID again; never transition an unrelated current Preview.
    if (selectPreview(id)) transitionPreview();
}

void SceneController::transitionPreview()
{
    if (!available_ || transitionPending_ || !obs_frontend_preview_program_mode_active()) return;
    Source preview(obs_frontend_get_current_preview_scene());
    if (!preview.ptr || hidden_().contains(sourceId(preview.ptr))) return;
    transitionPending_ = true;
    obs_frontend_preview_program_trigger_transition();
    refresh();
    // Guard duplicate gestures, including instantaneous Cut transitions. OBS itself
    // owns transition duration, overrides, and rejects overlapping transitions.
    QTimer::singleShot(350, this, [this] { transitionPending_ = false; refresh(); });
}

void SceneController::saveSettings()
{
    if (restoring_) return;
    QDir().mkpath(QFileInfo(settings_->fileName()).absolutePath());
    settings_->setValue("geometry", saveGeometry());
    settings_->setValue("alwaysOnTop", onTop_->isChecked());
    settings_->setValue("columns", columns_->value());
    settings_->setValue("buttonHeight", buttonHeight_->value());
    settings_->sync();
}

void SceneController::closeEvent(QCloseEvent *event) { saveSettings(); QDialog::closeEvent(event); }
void SceneController::moveEvent(QMoveEvent *event) { QDialog::moveEvent(event); saveSettings(); }
void SceneController::resizeEvent(QResizeEvent *event) { QDialog::resizeEvent(event); saveSettings(); }
