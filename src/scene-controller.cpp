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
#include <QStylePainter>
#include <QStyleOptionButton>
#include <QTextLayout>
#include <QResizeEvent>
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
    void paintEvent(QPaintEvent *) override
    {
        QStyleOptionButton option;
        initStyleOption(&option);
        option.text.clear();
        QStylePainter painter(this);
        painter.drawControl(QStyle::CE_PushButton, option);
        const QRect area = rect().adjusted(10, 6, -10, -6);
        QFont fitted = font();
        const int largest = qBound(10, qRound(qMin(width() / 10.0, height() / 3.0)), 24);
        QTextLayout textLayout;
        QTextOption textOption;
        textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        textOption.setAlignment(Qt::AlignHCenter);
        textLayout.setTextOption(textOption);
        qreal total = 0;
        for (int pixels = largest; pixels >= 8; --pixels) {
            fitted.setPixelSize(pixels);
            textLayout.setFont(fitted);
            textLayout.setText(text());
            textLayout.beginLayout();
            total = 0;
            while (true) {
                auto line = textLayout.createLine();
                if (!line.isValid()) break;
                line.setLineWidth(qMax(1, area.width()));
                line.setPosition(QPointF(0, total));
                total += line.height();
            }
            textLayout.endLayout();
            if (total <= area.height()) break;
            if (pixels > 8) textLayout.clearLayout();
        }
        const bool live = property("program").toBool();
        const bool queued = property("preview").toBool();
        painter.setPen(isEnabled() && (live || queued || objectName() == "controllerSendPreview")
                       ? QColor("#161616") : palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::ButtonText));
        painter.setClipRect(area);
        textLayout.draw(&painter, QPointF(area.left(), area.top() + qMax(0.0, (area.height() - total) / 2)));
    }
    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && isEnabled() && doubleClick) {
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
    setMinimumSize(220, 220);
    resize(600, 430);
    auto *layout = new QVBoxLayout(this);
    auto *toolbar = new QHBoxLayout;
    onTop_ = new QCheckBox("Always on top", this);
    onTop_->setObjectName("controllerAlwaysOnTop");
    toolbar->addWidget(onTop_);
    toolbar->addStretch();
    layout->addLayout(toolbar);
    openOnStartup_ = new QCheckBox("Open when OBS starts", this);
    openOnStartup_->setObjectName("controllerOpenOnStartup");
    openOnStartup_->setChecked(settings_->value("openOnStartup", false).toBool());
    layout->addWidget(openOnStartup_);
    connect(openOnStartup_, &QCheckBox::toggled, this, [this] { saveSettings(); });
    scroll_ = new QScrollArea(this);
    scroll_->setWidgetResizable(true);
    scroll_->viewport()->installEventFilter(this);
    auto *panel = new QWidget(scroll_);
    grid_ = new QGridLayout(panel);
    grid_->setAlignment(Qt::AlignTop);
    grid_->setContentsMargins(4, 4, 4, 4);
    scroll_->setWidget(panel);
    layout->addWidget(scroll_, 1);
    bottom_ = new QBoxLayout(QBoxLayout::LeftToRight);
    studio_ = new SceneButton("Studio Mode: OFF — click to enable", this);
    studio_->setObjectName("controllerStudioMode");
    studio_->setAutoDefault(false);
    transition_ = new SceneButton("Send Preview to Program", this);
    transition_->setAutoDefault(false);
    transition_->setObjectName("controllerSendPreview");
    transition_->setToolTip("Send the current Preview scene to live Program using your OBS transition. OBS may swap Preview and Program afterward.");
    studio_->setMinimumSize(0, 40);
    transition_->setMinimumSize(0, 40);
    studio_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    transition_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    bottom_->addWidget(studio_);
    bottom_->addWidget(transition_);
    layout->addLayout(bottom_);
    status_ = new QLabel(this);
    status_->setObjectName("controllerStatus");
    status_->setWordWrap(true);
    status_->setToolTip("Red: Program (live). Gold: Preview. If both are the same scene, red takes priority.");
    layout->addWidget(status_);
    setStyleSheet("QPushButton[sceneButton=\"true\"] { padding: 8px; border: 2px solid #777; border-radius: 6px; }"
                  "QPushButton[sceneButton=\"true\"][preview=\"true\"] { background-color: #e6b84a; color: #161616; border-color: #e6b84a; }"
                  "QPushButton[sceneButton=\"true\"][program=\"true\"] { background-color: #ea6565; color: #161616; border-color: #ea6565; }"
                  "QPushButton#controllerSendPreview:enabled { background-color: #e6b84a; color: #161616; border: 2px solid #e6b84a; border-radius: 4px; padding: 6px; }"
                  "QPushButton#controllerSendPreview:disabled { color: #888; border: 2px solid #888; border-radius: 4px; padding: 6px; }");
    connect(studio_, &QPushButton::clicked, this, [this] {
        if (!available_) return;
        obs_frontend_set_preview_program_mode(!obs_frontend_preview_program_mode_active());
        refresh();
        QTimer::singleShot(100, this, &SceneController::refresh);
    });
    connect(transition_, &QPushButton::clicked, this, &SceneController::transitionPreview);
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

bool SceneController::opensOnStartup(const QString &settingsPath)
{
    QSettings settings(settingsPath, QSettings::IniFormat);
    return settings.value("openOnStartup", false).toBool();
}

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
    studio_->setText(studio ? "Studio Mode: ON" : "Studio Mode: OFF");
    studio_->setToolTip(studio ? "Click to disable Studio Mode" : "Click to enable Studio Mode");
    Source program(obs_frontend_get_current_scene());
    Source preview(studio ? obs_frontend_get_current_preview_scene() : nullptr);
    const auto programId = sourceId(program.ptr), previewId = sourceId(preview.ptr);
    for (const auto &id : order_) {
        auto *button = buttons_.value(id);
        const bool live = id == programId, queued = id == previewId;
        const auto name = button->property("sceneName").toString();
        button->setText(name);
        const bool changed = button->property("program").toBool() != live || button->property("preview").toBool() != queued;
        button->setProperty("program", live);
        button->setProperty("preview", queued);
        if (changed) { button->style()->unpolish(button); button->style()->polish(button); }
        button->setEnabled(studio && !transitionPending_);
        button->setToolTip(name + "\nSingle-click: Preview. Double-click: transition to Program.");
    }
    transition_->setEnabled(studio && !transitionPending_ && preview.ptr && !hidden.contains(previewId));
    status_->setText(studio ? (order_.isEmpty() ? "No production scenes available." :
                     "Click: Preview • Double-click: Program") :
                     "Studio Mode is OFF. Click the Studio Mode button to enable Preview selection and double-click transitions.");
}

void SceneController::rearrange()
{
    if (!scroll_ || !bottom_) return;
    bottom_->setDirection(width() < 420 ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
    const int columns = qBound(1, scroll_->viewport()->width() / 160, 12);
    const int rows = qMax(1, (order_.size() + columns - 1) / columns);
    const int cellWidth = qMax(1, (scroll_->viewport()->width() - 8 - (columns - 1) * grid_->spacing()) / columns);
    const int roomHeight = (scroll_->viewport()->height() - 8 - (rows - 1) * grid_->spacing()) / rows;
    const int cellHeight = qBound(48, qMin(qRound(cellWidth * 0.55), roomHeight), 112);
    if (columns == layoutColumns_ && cellHeight == layoutHeight_ && order_ == layoutOrder_) return;
    layoutColumns_ = columns;
    layoutHeight_ = cellHeight;
    layoutOrder_ = order_;
    while (auto *item = grid_->takeAt(0)) delete item;
    for (int col = 0; col < 12; ++col) grid_->setColumnStretch(col, col < columns ? 1 : 0);
    for (int i = 0; i < order_.size(); ++i) {
        auto *button = buttons_.value(order_[i]);
        button->setFixedHeight(cellHeight);
        button->setMinimumWidth(0);
        button->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        grid_->addWidget(button, i / columns, i % columns);
        button->show();
    }
}

bool SceneController::eventFilter(QObject *watched, QEvent *event)
{
    if (scroll_ && watched == scroll_->viewport() && event->type() == QEvent::Resize)
        QTimer::singleShot(0, this, &SceneController::rearrange);
    return QDialog::eventFilter(watched, event);
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
    settings_->setValue("openOnStartup", openOnStartup_->isChecked());
    settings_->sync();
}

void SceneController::closeEvent(QCloseEvent *event) { saveSettings(); QDialog::closeEvent(event); }
void SceneController::moveEvent(QMoveEvent *event) { QDialog::moveEvent(event); saveSettings(); }
void SceneController::resizeEvent(QResizeEvent *event) { QDialog::resizeEvent(event); QTimer::singleShot(0, this, &SceneController::rearrange); saveSettings(); }
