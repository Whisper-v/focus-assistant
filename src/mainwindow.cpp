#include "mainwindow.h"
#include "petwidget.h"
#include "systemlinker.h"
#include "statsdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QMouseEvent>
#include <QCloseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QTimer>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QScreen>
#include <QApplication>
#include <QIcon>
#include <QGuiApplication>
#include <QDialog>

#include <DPushButton>
#include <DSuggestButton>

DWIDGET_USE_NAMESPACE

namespace {

// 把对话框居中到 anchor(通常为主窗口)所在屏幕的中央
void centerDialogOnScreen(QDialog *dlg, QWidget *anchor)
{
    dlg->adjustSize();
    QScreen *sc = nullptr;
    if (anchor && anchor->window())
        sc = QGuiApplication::screenAt(anchor->window()->geometry().center());
    if (!sc)
        sc = QGuiApplication::primaryScreen();
    if (!sc)
        return;
    const QPoint c = sc->availableGeometry().center();
    const QSize hint = dlg->sizeHint();
    dlg->move(c.x() - hint.width() / 2, c.y() - hint.height() / 2);
    // 显示后再按真实窗口(含边框)微调一次，确保严格居中
    QTimer::singleShot(0, dlg, [dlg, c] {
        const QRect g = dlg->frameGeometry();
        dlg->move(c.x() - g.width() / 2, c.y() - g.height() / 2);
    });
}

// ---------- 无边框窗口手动缩放（可大可小） ----------
constexpr int kResizeEdge = 8; // 距窗口边缘多少像素内可拖拽缩放

// 命中测试：返回所在边缘位（1左 2右 4上 8下），内部为 0
int hitTestEdges(const QPoint &pos, const QSize &sz)
{
    const int x = pos.x(), y = pos.y();
    const int W = sz.width(), H = sz.height();
    int dir = 0;
    if (x <= kResizeEdge)               dir |= 1; // 左
    if (x >= W - 1 - kResizeEdge)       dir |= 2; // 右
    if (y <= kResizeEdge)               dir |= 4; // 上
    if (y >= H - 1 - kResizeEdge)       dir |= 8; // 下
    return dir;
}

Qt::CursorShape edgeCursorShape(int dir)
{
    switch (dir) {
    case 1: case 2:  return Qt::SizeHorCursor;   // 左右
    case 4: case 8:  return Qt::SizeVerCursor;   // 上下
    case 5: case 10: return Qt::SizeFDiagCursor; // 左上↘右下
    case 6: case 9:  return Qt::SizeBDiagCursor; // 右上↙左下
    default:         return Qt::ArrowCursor;
    }
}

} // namespace

MainWindow::MainWindow(FocusManager *mgr, SystemLinker *linker, QWidget *parent)
    : QWidget(parent)
    , m_mgr(mgr)
    , m_linker(linker)
    , m_prefs(QStringLiteral("focus-assistant"), QStringLiteral("focus-assistant"))
{
    setWindowTitle(QStringLiteral("专注助手 Focus Assistant"));
    setWindowIcon(QIcon(QStringLiteral(":/icons/focus-assistant.svg")));
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setMinimumSize(220, 300);   // 下限，实际最小尺寸由布局约束
    setMouseTracking(true);

    buildUi();
    buildSkinActions();
    buildTray();
    applyPrefs();
    updateSkinChecks();
    m_linker->recoverFromCrash(); // 若上次被强杀，先清理可能残留的系统色温

    connect(m_mgr, &FocusManager::phaseChanged, this, &MainWindow::onPhaseChanged);
    connect(m_mgr, &FocusManager::tick, this, &MainWindow::onTick);
    connect(m_mgr, &FocusManager::focusCompleted, this, &MainWindow::onFocusCompleted);
    connect(m_mgr, &FocusManager::focusAborted, this, &MainWindow::onFocusAborted);
    connect(m_mgr, &FocusManager::stageChanged, this, &MainWindow::onStageChanged);
    connect(m_mgr, &FocusManager::breakFinished, this, &MainWindow::onBreakFinished);
    connect(m_mgr, &FocusManager::statusMessage, this, &MainWindow::onStatusMessage);

    // 兜底：无论何种方式退出，都还原我们开启的护眼色温
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] { m_linker->onFocusEnded(); });

    m_pet->setStage(m_mgr->stage()); // 启动即同步存档中的成长阶段
    updateMoodAndText();
    updateActions();
    updateStageLabel();
    updateTimeLabel();

    // 恢复上次大小（无边框自由缩放，可大可小；下限由布局最小尺寸约束）
    if (layout())
        layout()->activate();
    const QSize layoutMin = layout() ? layout()->totalMinimumSize() : minimumSize();
    m_resizeMin = layoutMin.expandedTo(QSize(230, 330));
    const QSize savedSize = m_prefs.value(QStringLiteral("window/size"), QSize(300, 412)).toSize();
    resize(qBound(m_resizeMin.width(), savedSize.width(), 1800),
           qBound(m_resizeMin.height(), savedSize.height(), 1500));

    // 记忆位置 / 默认停靠屏幕右下角
    const QPoint saved = m_prefs.value(QStringLiteral("window/pos")).toPoint();
    if (saved.isNull()) {
        if (const QScreen *sc = QGuiApplication::primaryScreen()) {
            const QRect ag = sc->availableGeometry();
            move(ag.right() - width() - 42, ag.bottom() - height() - 36);
        }
    } else {
        move(saved);
    }

    // 双击露露 = 开始/暂停；按住露露或空白处 = 拖动窗口
    m_pet->installEventFilter(this);
    m_stateLabel->installEventFilter(this);
    m_msgTitle->installEventFilter(this);
    m_msgSub->installEventFilter(this);
    m_stageLabel->installEventFilter(this);
    m_timeLabel->installEventFilter(this);
    // 文字区域不消费鼠标事件，开启追踪让父窗口能收到 move（用于边缘缩放光标提示）
    for (QWidget *w : { static_cast<QWidget *>(m_stateLabel),
                        static_cast<QWidget *>(m_msgTitle),
                        static_cast<QWidget *>(m_msgSub),
                        static_cast<QWidget *>(m_stageLabel),
                        static_cast<QWidget *>(m_timeLabel) })
        w->setMouseTracking(true);
}

MainWindow::~MainWindow()
{
    m_prefs.setValue(QStringLiteral("window/pos"), pos());
    m_prefs.setValue(QStringLiteral("window/size"), size());
    m_prefs.sync();
}

void MainWindow::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 10, 16, 12);
    root->setSpacing(1);

    // ---- 顶栏 ----
    auto *head = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("🌱 专注助手"), this);
    QFont tf = font();
    tf.setPixelSize(13);
    tf.setBold(true);
    title->setFont(tf);
    title->setObjectName(QStringLiteral("headTitle"));
    head->addWidget(title);
    head->addStretch();
    m_statsBtn = new QPushButton(QStringLiteral("统计"), this);
    m_settingsBtn = new QPushButton(QStringLiteral("设置"), this);
    for (auto *b : { m_statsBtn, m_settingsBtn }) {
        b->setFixedSize(40, 22);
        b->setCursor(Qt::PointingHandCursor);
        QFont bf = font(); bf.setPixelSize(10);
        b->setFont(bf);
    }
    head->addWidget(m_statsBtn);
    head->addSpacing(3);
    head->addWidget(m_settingsBtn);
    root->addLayout(head);

    // ---- 露露（铺满可用区域；内部按 300x250 画布等比缩放居中 → 窗口可大可小）----
    m_pet = new PetWidget(this);
    root->addWidget(m_pet, 1);

    // ---- 状态与时间 ----
    m_stateLabel = new QLabel(this);
    QFont sf = font(); sf.setPixelSize(12); sf.setBold(true);
    m_stateLabel->setFont(sf);
    m_stateLabel->setAlignment(Qt::AlignCenter);
    root->addWidget(m_stateLabel);

    m_timeLabel = new QLabel(this);
    QFont clock = font();
    clock.setPixelSize(34);
    clock.setWeight(QFont::DemiBold);
    m_timeLabel->setFont(clock);
    m_timeLabel->setAlignment(Qt::AlignCenter);
    root->addWidget(m_timeLabel);

    m_stageLabel = new QLabel(this);
    QFont stf = font(); stf.setPixelSize(10);
    m_stageLabel->setFont(stf);
    m_stageLabel->setAlignment(Qt::AlignCenter);
    m_stageLabel->setStyleSheet(QStringLiteral("color: rgba(130,130,130,0.9);"));
    root->addWidget(m_stageLabel);

    root->addSpacing(2);

    // ---- 操作按钮 ----
    auto *actions = new QHBoxLayout;
    actions->setSpacing(8);
    m_primary = new DSuggestButton(this);
    m_secondary = new DPushButton(this);
    m_primary->setMinimumHeight(30);
    m_secondary->setMinimumHeight(30);
    actions->addWidget(m_primary, 1);
    actions->addWidget(m_secondary, 1);
    root->addSpacing(4);
    root->addLayout(actions);

    // ---- 消息气泡 ----
    m_msgTitle = new QLabel(this);
    QFont mtf = font(); mtf.setPixelSize(11); mtf.setBold(true);
    m_msgTitle->setFont(mtf);
    m_msgTitle->setAlignment(Qt::AlignCenter);
    m_msgTitle->setWordWrap(true);
    root->addWidget(m_msgTitle);

    m_msgSub = new QLabel(this);
    QFont msf = font(); msf.setPixelSize(10);
    m_msgSub->setFont(msf);
    m_msgSub->setAlignment(Qt::AlignCenter);
    m_msgSub->setWordWrap(true);
    m_msgSub->setStyleSheet(QStringLiteral("color: rgba(130,130,130,0.9);"));
    root->addWidget(m_msgSub);

    connect(m_primary, &QPushButton::clicked, this, &MainWindow::onPrimaryClicked);
    connect(m_secondary, &QPushButton::clicked, this, &MainWindow::onSecondaryClicked);
    connect(m_statsBtn, &QPushButton::clicked, this, &MainWindow::showStats);
    connect(m_settingsBtn, &QPushButton::clicked, this, &MainWindow::openSettings);

    // 初始提示
    onStatusMessage(QStringLiteral("你好，我是露露"), QStringLiteral("点击「开始专注」，和我一起种下今天的第一朵花吧"));
}

void MainWindow::buildTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;
    m_tray = new QSystemTrayIcon(this);
    m_tray->setIcon(QIcon(QStringLiteral(":/icons/focus-assistant.svg")));
    m_tray->setToolTip(QStringLiteral("专注助手 Focus Assistant"));

    m_trayMenu = new QMenu(this);
    m_trayToggle = m_trayMenu->addAction(QStringLiteral("隐藏到托盘"));
    m_trayPlay = m_trayMenu->addAction(QStringLiteral("开始专注"));
    m_trayMenu->addSeparator();
    m_trayPin = m_trayMenu->addAction(QStringLiteral("桌面常驻"));
    m_trayPin->setCheckable(true);
    m_trayPin->setToolTip(QStringLiteral("勾选后窗口常驻桌面（置顶显示），点击“显示桌面”也不会被隐藏"));
    connect(m_trayPin, &QAction::triggered, this, [this](bool on) {
        m_prefs.beginGroup(QStringLiteral("prefs"));
        m_prefs.setValue(QStringLiteral("pinDesktop"), on);
        m_prefs.endGroup();
        setPinDesktop(on);
        onStatusMessage(on ? QStringLiteral("已开启桌面常驻")
                           : QStringLiteral("已关闭桌面常驻"),
                        on ? QStringLiteral("露露会一直待在桌面上，不受「显示桌面」影响")
                           : QStringLiteral("露露会随「显示桌面」一起隐藏，需要时可在托盘重新开启"));
    });
    m_trayMenu->addSeparator();
    QAction *statsAct = m_trayMenu->addAction(QStringLiteral("成长记录"));
    m_trayMenu->addSeparator();
    QMenu *skinSub = m_trayMenu->addMenu(QStringLiteral("换肤 · 露露的皮肤"));
    for (QAction *a : m_skinActs)
        skinSub->addAction(a);
    m_trayMenu->addSeparator();
    QAction *quitAct = m_trayMenu->addAction(QStringLiteral("退出"));

    connect(m_trayToggle, &QAction::triggered, this, &MainWindow::toggleVisible);
    connect(m_trayPlay, &QAction::triggered, this, &MainWindow::onPrimaryClicked);
    connect(statsAct, &QAction::triggered, this, &MainWindow::showStats);
    connect(quitAct, &QAction::triggered, this, &MainWindow::quitApplication);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger || r == QSystemTrayIcon::DoubleClick)
            toggleVisible();
    });
    m_tray->setContextMenu(m_trayMenu);
    m_tray->show();
}

// ---------- 自选皮肤：右键露露 / 托盘菜单 ----------
void MainWindow::buildSkinActions()
{
    const PetWidget::Skin skins[] = {
        PetWidget::Skin::Classic,
        PetWidget::Skin::Sunflower
    };
    for (PetWidget::Skin sk : skins) {
        auto *act = new QAction(PetWidget::skinName(sk), this);
        act->setCheckable(true);
        act->setData(PetWidget::skinKey(sk));
        connect(act, &QAction::triggered, this, [this, act] {
            applySkinKey(act->data().toString());
        });
        m_skinActs.append(act);
    }
}

void MainWindow::updateSkinChecks()
{
    const QString cur = PetWidget::skinKey(m_pet->skin());
    for (QAction *a : m_skinActs)
        a->setChecked(a->data().toString() == cur);
}

void MainWindow::showSkinMenu(const QPoint &globalPos)
{
    updateSkinChecks();
    QMenu menu(this);
    for (QAction *a : m_skinActs)
        menu.addAction(a);
    menu.exec(globalPos);
}

void MainWindow::applySkinKey(const QString &key)
{
    const PetWidget::Skin sk = PetWidget::skinFromKey(key);
    if (m_pet->skin() == sk)
        return;
    m_prefs.beginGroup(QStringLiteral("prefs"));
    m_prefs.setValue(QStringLiteral("skin"), key);
    m_prefs.endGroup();
    m_pet->setSkin(sk);
    updateSkinChecks();
    onStatusMessage(QStringLiteral("换肤成功"),
                    QStringLiteral("露露现在是「%1」啦").arg(PetWidget::skinName(sk)));
}

void MainWindow::applyPrefs()
{
    m_prefs.beginGroup(QStringLiteral("prefs"));
    FocusManager::Config c = m_mgr->config();
    c.focusSeconds   = m_prefs.value(QStringLiteral("focusMin"), 25).toInt() * 60;
    c.breakSeconds   = m_prefs.value(QStringLiteral("breakMin"), 5).toInt() * 60;
    c.longBreakSeconds = m_prefs.value(QStringLiteral("longBreakMin"), 15).toInt() * 60;
    c.sessionsPerLongBreak = m_prefs.value(QStringLiteral("perLong"), 4).toInt();
    c.autoBreak      = m_prefs.value(QStringLiteral("autoBreak"), true).toBool();
    m_mgr->setConfig(c);

    const bool eyeOn = m_prefs.value(QStringLiteral("eyeProtect"), false).toBool();
    const int eyeTemp = m_prefs.value(QStringLiteral("eyeTemp"), 3500).toInt();
    m_linker->setEyeProtectionEnabled(eyeOn);
    m_linker->setEyeTemp(eyeTemp);

    const QString skin = m_prefs.value(QStringLiteral("skin"), QStringLiteral("classic")).toString();
    m_pet->setSkin(PetWidget::skinFromKey(skin));
    const bool pin = m_prefs.value(QStringLiteral("pinDesktop"), false).toBool();
    m_prefs.endGroup();
    updateSkinChecks();
    setPinDesktop(pin);
}

// ---------- 桌面常驻：不受「显示桌面」影响 ----------
void MainWindow::setPinDesktop(bool on)
{
    // X11 下用 override-redirect(绕过窗口管理器)实现：WM 无法把窗口最小化/隐藏，
    // 因此「显示桌面」不影响它；同时加置顶，窗口始终可见。非 X11 会话退化为普通置顶。
    const bool x11 = QGuiApplication::platformName() == QLatin1String("xcb");
    Qt::WindowFlags f = Qt::Window | Qt::FramelessWindowHint;
    if (on) {
        f |= Qt::WindowStaysOnTopHint;
        if (x11)
            f |= Qt::X11BypassWindowManagerHint;
    }
    const bool wasVisible = isVisible();
    if (f != windowFlags()) {
        setWindowFlags(f);
        setAttribute(Qt::WA_TranslucentBackground); // 保持自绘圆角透明背景
        if (wasVisible) {
            show();          // setWindowFlags 会隐式 hide，需重新显示
            raise();
        }
    }
    if (m_trayPin)
        m_trayPin->setChecked(on);
}

void MainWindow::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    QColor base = palette().color(QPalette::Base);
    base.setAlpha(236);
    const QRect r = rect().adjusted(1, 1, -1, -1);
    QPainterPath path;
    path.addRoundedRect(r, 18, 18);

    p.fillPath(path, base);
    QColor border = palette().color(QPalette::WindowText);
    border.setAlpha(24);
    p.setPen(QPen(border, 1));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);

    // 右下角三点：提示可拖拽边缘自由缩放窗口（可大可小）
    const int W = width(), H = height();
    if (W > 70 && H > 70) {
        QColor grip = palette().color(QPalette::WindowText);
        grip.setAlpha(78);
        p.setBrush(grip);
        p.setPen(Qt::NoPen);
        const qreal x0 = W - 16.0, y0 = H - 15.0;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j <= i; ++j)
                p.drawEllipse(QPointF(x0 - j * 5.0, y0 - i * 5.0), 1.4, 1.4);
    }
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    if (!m_quitting && m_tray) {
        e->ignore();
        hide();
        if (m_prefs.value(QStringLiteral("window/firstHide"), true).toBool()) {
            m_prefs.setValue(QStringLiteral("window/firstHide"), false);
            m_tray->showMessage(QStringLiteral("专注助手"),
                                QStringLiteral("露露会继续在托盘里陪你，右键托盘图标可以退出"),
                                QSystemTrayIcon::Information, 4000);
        }
        return;
    }
    m_prefs.setValue(QStringLiteral("window/pos"), pos());
    m_prefs.sync();
    e->accept();
}

void MainWindow::quitApplication()
{
    m_quitting = true;
    if (m_tray)
        m_tray->hide();
    close();
    QApplication::quit();
}

void MainWindow::toggleVisible()
{
    if (isVisible()) {
        hide();
        m_trayToggle->setText(QStringLiteral("显示主界面"));
    } else {
        show();
        raise();
        activateWindow();
        m_trayToggle->setText(QStringLiteral("隐藏到托盘"));
    }
}

// ---------- 鼠标拖动 / 双击 ----------
bool MainWindow::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj == m_pet) {
        if (ev->type() == QEvent::MouseButtonDblClick) {
            onPrimaryClicked();
            return true;
        }
        if (ev->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent *>(ev);
            if (me->button() == Qt::RightButton) {
                showSkinMenu(me->globalPosition().toPoint());
                return true;
            }
            if (me->button() == Qt::LeftButton) {
                m_dragging = true;
                m_dragOffset = me->globalPosition().toPoint() - frameGeometry().topLeft();
                return true;
            }
        } else if (ev->type() == QEvent::MouseMove && m_dragging) {
            auto *me = static_cast<QMouseEvent *>(ev);
            move(me->globalPosition().toPoint() - m_dragOffset);
            return true;
        } else if (ev->type() == QEvent::MouseButtonRelease) {
            m_dragging = false;
            return true;
        }
    }
    return QWidget::eventFilter(obj, ev);
}

void MainWindow::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        // 贴到窗口边缘 => 开始自由缩放（优先级高于拖动）
        const int dir = hitTestEdges(e->position().toPoint(), size());
        if (dir) {
            beginResize(dir, e->globalPosition().toPoint());
            e->accept();
            return;
        }
        // 点按窗口空白处 => 拖动整个窗口
        if (childAt(e->pos()) == this) {
            m_dragging = true;
            m_dragOffset = e->globalPosition().toPoint() - frameGeometry().topLeft();
            e->accept();
            return;
        }
    }
    QWidget::mousePressEvent(e);
}

void MainWindow::mouseMoveEvent(QMouseEvent *e)
{
    if (m_resizing) {
        doResize(e->globalPosition().toPoint());
        e->accept();
        return;
    }
    if (m_dragging) {
        move(e->globalPosition().toPoint() - m_dragOffset);
        e->accept();
        return;
    }
    // 无按键移动：悬停到边缘时给出可缩放的光标提示
    updateHoverCursor(e->position().toPoint());
    QWidget::mouseMoveEvent(e);
}

void MainWindow::mouseReleaseEvent(QMouseEvent *e)
{
    if (m_resizing) {
        endResize();
        e->accept();
        return;
    }
    m_dragging = false;
    QWidget::mouseReleaseEvent(e);
}

// ---------- 无边框手动缩放实现 ----------
void MainWindow::beginResize(int dir, const QPoint &globalPos)
{
    m_resizing = true;
    m_resizeDir = dir;
    m_resizeStart = globalPos;
    m_resizeGeom = geometry();
    // 重新评估布局最小尺寸作为缩放下限
    if (layout())
        layout()->activate();
    const QSize hint = layout() ? layout()->totalMinimumSize() : minimumSize();
    m_resizeMin = hint.expandedTo(QSize(230, 330));
    setEdgeCursor(dir);
}

void MainWindow::doResize(const QPoint &globalPos)
{
    const QPoint d = globalPos - m_resizeStart;
    QRect g = m_resizeGeom;
    const int dir = m_resizeDir;
    const int minW = m_resizeMin.width();
    const int minH = m_resizeMin.height();

    if (dir & 2) g.setRight(m_resizeGeom.right() + d.x()); // 右
    if (dir & 8) g.setBottom(m_resizeGeom.bottom() + d.y()); // 下
    if (dir & 1) g.setLeft(m_resizeGeom.left() + d.x());    // 左
    if (dir & 4) g.setTop(m_resizeGeom.top() + d.y());      // 上

    if (g.width() < minW) {
        if (dir & 1) g.setLeft(g.right() - minW + 1);
        else         g.setRight(g.left() + minW - 1);
    }
    if (g.height() < minH) {
        if (dir & 4) g.setTop(g.bottom() - minH + 1);
        else         g.setBottom(g.top() + minH - 1);
    }
    setGeometry(g);
}

void MainWindow::endResize()
{
    m_resizing = false;
    m_resizeDir = 0;
    unsetCursor();
    // 立即记住新尺寸，下次启动恢复
    m_prefs.setValue(QStringLiteral("window/size"), size());
    m_prefs.sync();
}

void MainWindow::setEdgeCursor(int dir)
{
    const Qt::CursorShape shape = edgeCursorShape(dir);
    if (shape == Qt::ArrowCursor)
        unsetCursor();
    else
        setCursor(shape);
}

void MainWindow::updateHoverCursor(const QPoint &pos)
{
    if (m_resizing || m_dragging)
        return;
    setEdgeCursor(hitTestEdges(pos, size()));
}

// ---------- 状态刷新 ----------
void MainWindow::onPhaseChanged(FocusManager::Phase phase)
{
    Q_UNUSED(phase)
    updateMoodAndText();
    updateActions();
}

void MainWindow::onTick(int)
{
    updateTimeLabel();
    const int total = m_mgr->totalSeconds();
    if (total > 0) {
        const qreal p = qBound<qreal>(0.0, (qreal)m_mgr->remaining() / total, 1.0);
        m_pet->setProgress(p);
    }
}

void MainWindow::updateMoodAndText()
{
    const auto phase = m_mgr->phase();
    QString state;
    PetWidget::Mood mood = PetWidget::Idle;
    switch (phase) {
    case FocusManager::Idle:
        state = QStringLiteral("准备就绪");
        mood = m_justCompleted ? PetWidget::Happy : PetWidget::Idle;
        m_justCompleted = false;
        m_pet->setProgress(0.0);
        break;
    case FocusManager::Focus:
        state = QStringLiteral("专注中 · 露露为你守护");
        mood = PetWidget::Focused;
        break;
    case FocusManager::Pause:
        state = QStringLiteral("专注已暂停");
        mood = PetWidget::Idle;
        break;
    case FocusManager::Break:
        state = m_mgr->isLongBreak() ? QStringLiteral("长休息 · 露露在打盹") : QStringLiteral("休息中 · 露露在打盹");
        mood = PetWidget::Sleepy;
        if (m_justCompleted) {
            m_pet->startCelebration();
            m_justCompleted = false;
        }
        break;
    }
    m_stateLabel->setText(state);
    m_pet->setMood(mood);
    updateTimeLabel();
}

void MainWindow::updateTimeLabel()
{
    const auto phase = m_mgr->phase();
    int secs;
    switch (phase) {
    case FocusManager::Idle:
        secs = m_mgr->config().focusSeconds;
        break;
    default:
        secs = m_mgr->remaining();
        break;
    }
    m_timeLabel->setText(fmt(qMax(0, secs)));
}

void MainWindow::updateActions()
{
    const auto phase = m_mgr->phase();
    switch (phase) {
    case FocusManager::Idle:
        m_primary->setText(QStringLiteral("开始专注"));
        m_secondary->setText(QStringLiteral("统计"));
        break;
    case FocusManager::Focus:
        m_primary->setText(QStringLiteral("暂停"));
        m_secondary->setText(QStringLiteral("放弃本次"));
        break;
    case FocusManager::Pause:
        m_primary->setText(QStringLiteral("继续专注"));
        m_secondary->setText(QStringLiteral("放弃本次"));
        break;
    case FocusManager::Break:
        m_primary->setText(QStringLiteral("开始专注"));
        m_secondary->setText(QStringLiteral("跳过休息"));
        break;
    }
    if (m_trayPlay) {
        m_trayPlay->setText(phase == FocusManager::Focus ? QStringLiteral("暂停专注")
                          : phase == FocusManager::Pause ? QStringLiteral("继续专注")
                          : QStringLiteral("开始专注"));
    }
}

void MainWindow::updateStageLabel()
{
    if (!m_mgr)
        return;
    const int remain = m_mgr->nextStageRemainSessions();
    if (remain < 0) {
        m_stageLabel->setText(QStringLiteral("成长：%1 · 已到盛放形态，继续陪伴你").arg(m_mgr->stageName()));
    } else {
        m_stageLabel->setText(QStringLiteral("成长：%1 · 再专注 %2 次可成长").arg(m_mgr->stageName()).arg(remain));
    }
}

QString MainWindow::fmt(int seconds)
{
    const int h = seconds / 3600;
    const int m = (seconds % 3600) / 60;
    const int s = seconds % 60;
    if (h > 0)
        return QStringLiteral("%1:%2:%3").arg(h).arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2").arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0'));
}

// ---------- 业务回调 ----------
void MainWindow::onPrimaryClicked()
{
    switch (m_mgr->phase()) {
    case FocusManager::Idle:
    case FocusManager::Break:
        m_linker->onFocusStarted();
        m_mgr->startFocus();
        break;
    case FocusManager::Focus:
        m_mgr->pauseFocus();
        break;
    case FocusManager::Pause:
        m_mgr->resumeFocus();
        break;
    }
    updateActions();
}

void MainWindow::onSecondaryClicked()
{
    switch (m_mgr->phase()) {
    case FocusManager::Idle:
        showStats();
        break;
    case FocusManager::Focus:
    case FocusManager::Pause:
        m_linker->onFocusEnded();
        m_mgr->abortFocus();
        break;
    case FocusManager::Break:
        m_mgr->skipBreak();
        break;
    }
    updateActions();
}

void MainWindow::onFocusCompleted(int inRow, int stage)
{
    Q_UNUSED(inRow)
    Q_UNUSED(stage)
    m_justCompleted = true;
    m_linker->onFocusEnded(); // 一段专注结束(进入休息)，恢复系统状态
    notify(QStringLiteral("专注完成！"), QStringLiteral("露露伸了个懒腰：你太棒啦 🌸"));
    onStatusMessage(QStringLiteral("专注完成！"), QStringLiteral("露露开出了一朵小花，快去休息一下"));
    updateStageLabel();
}

void MainWindow::onFocusAborted()
{
    m_linker->onFocusEnded();
    updateMoodAndText();
    updateActions();
}

void MainWindow::onStageChanged(int stage)
{
    m_pet->setStage(stage);
    updateStageLabel();
    m_pet->setMood(PetWidget::Celebrate);
    m_pet->startCelebration();
    notify(QStringLiteral("露露成长啦！"),
           QStringLiteral("已长成「%1」形态").arg(m_mgr->stageName()));
    onStatusMessage(QStringLiteral("✦ 露露成长了 ✦"),
                    QStringLiteral("现在的形态：%1").arg(m_mgr->stageName()));
    // 若在专注中成长，庆祝后回到专注表情
    if (m_mgr->phase() == FocusManager::Focus)
        QTimer::singleShot(1600, this, [this] { m_pet->setMood(PetWidget::Focused); });
}

void MainWindow::onBreakFinished(bool wasLong)
{
    m_justCompleted = false;
    if (wasLong) {
        notify(QStringLiteral("长休息结束"), QStringLiteral("满电复活！开始新一轮专注吧"));
        onStatusMessage(QStringLiteral("露露满电复活！"), QStringLiteral("新一轮专注，准备开始"));
    }
    updateMoodAndText();
    updateActions();
}

void MainWindow::onStatusMessage(const QString &title, const QString &sub)
{
    m_msgTitle->setText(title);
    m_msgSub->setText(sub);
}

void MainWindow::showStats()
{
    StatsDialog dlg(m_mgr, this);
    centerDialogOnScreen(&dlg, this);   // 屏幕中间呼出
    dlg.exec();
}

void MainWindow::openSettings()
{
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("专注助手 · 设置"));
    dlg.setFixedWidth(420);

    auto *form = new QFormLayout(&dlg);
    form->setContentsMargins(24, 22, 24, 10);
    form->setSpacing(12);

    m_prefs.beginGroup(QStringLiteral("prefs"));
    const int focusMin = m_prefs.value(QStringLiteral("focusMin"), 25).toInt();
    const int breakMin = m_prefs.value(QStringLiteral("breakMin"), 5).toInt();
    const int longBreakMin = m_prefs.value(QStringLiteral("longBreakMin"), 15).toInt();
    const int perLong = m_prefs.value(QStringLiteral("perLong"), 4).toInt();
    const bool autoBreak = m_prefs.value(QStringLiteral("autoBreak"), true).toBool();
    const bool eyeProtect = m_prefs.value(QStringLiteral("eyeProtect"), false).toBool();
    const int eyeTemp = m_prefs.value(QStringLiteral("eyeTemp"), 3500).toInt();
    const bool pinDesktop = m_prefs.value(QStringLiteral("pinDesktop"), false).toBool();
    m_prefs.endGroup();

    auto *focusBox = new QComboBox(&dlg);
    for (int v : { 15, 25, 45, 60 })
        focusBox->addItem(QStringLiteral("%1 分钟").arg(v), v);
    focusBox->setCurrentIndex(focusBox->findData(focusMin));

    auto *breakBox = new QComboBox(&dlg);
    for (int v : { 5, 10, 15 })
        breakBox->addItem(QStringLiteral("%1 分钟").arg(v), v);
    breakBox->setCurrentIndex(breakBox->findData(breakMin));

    auto *longBox = new QComboBox(&dlg);
    for (int v : { 10, 15, 20, 30 })
        longBox->addItem(QStringLiteral("%1 分钟").arg(v), v);
    longBox->setCurrentIndex(longBox->findData(longBreakMin));

    auto *perSpin = new QSpinBox(&dlg);
    perSpin->setRange(2, 6);
    perSpin->setValue(perLong);
    perSpin->setSuffix(QStringLiteral(" 次"));

    auto *autoBreakChk = new QCheckBox(QStringLiteral("专注结束后自动进入休息"), &dlg);
    autoBreakChk->setChecked(autoBreak);

    auto *pinChk = new QCheckBox(QStringLiteral("桌面常驻：不受「显示桌面」影响"), &dlg);
    pinChk->setChecked(pinDesktop);
    pinChk->setToolTip(QStringLiteral("开启后窗口以置顶方式常驻桌面，点击“显示桌面”时不会被隐藏（需要 X11 会话支持）。\n关闭后恢复普通窗口行为。"));

    auto *skinBox = new QComboBox(&dlg);
    skinBox->addItem(PetWidget::skinName(PetWidget::Skin::Classic), QStringLiteral("classic"));
    skinBox->addItem(PetWidget::skinName(PetWidget::Skin::Sunflower), QStringLiteral("sunflower"));
    skinBox->setCurrentIndex(skinBox->findData(PetWidget::skinKey(m_pet->skin())));

    auto *eyeChk = new QCheckBox(QStringLiteral("专注时联动系统护眼模式"), &dlg);
    eyeChk->setChecked(eyeProtect);
    eyeChk->setToolTip(QStringLiteral("通过系统 Display1 色温接口实现；开启后专注时屏幕会变暖，结束自动还原。若影响桌面显示可关闭。"));

    auto *tempBox = new QComboBox(&dlg);
    for (int v : { 3000, 3500, 4000, 4500, 5000 }) {
        QString label = v == 3500 ? QStringLiteral("%1 K（护眼推荐）").arg(v) : QStringLiteral("%1 K").arg(v);
        tempBox->addItem(label, v);
    }
    tempBox->setCurrentIndex(tempBox->findData(eyeTemp));

    form->addRow(QStringLiteral("单次专注"), focusBox);
    form->addRow(QStringLiteral("短休息"), breakBox);
    form->addRow(QStringLiteral("长休息(第 N 次后)"), longBox);
    form->addRow(QStringLiteral("长休息间隔"), perSpin);
    form->addRow(QStringLiteral("露露的皮肤"), skinBox);
    form->addRow(QString(), pinChk);
    form->addRow(QString(), autoBreakChk);
    form->addRow(QString(), eyeChk);
    form->addRow(QStringLiteral("护眼色温"), tempBox);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("保存"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(buttons);

    centerDialogOnScreen(&dlg, this);   // 屏幕中间呼出
    if (dlg.exec() != QDialog::Accepted)
        return;

    m_prefs.beginGroup(QStringLiteral("prefs"));
    m_prefs.setValue(QStringLiteral("focusMin"), focusBox->currentData().toInt());
    m_prefs.setValue(QStringLiteral("breakMin"), breakBox->currentData().toInt());
    m_prefs.setValue(QStringLiteral("longBreakMin"), longBox->currentData().toInt());
    m_prefs.setValue(QStringLiteral("perLong"), perSpin->value());
    m_prefs.setValue(QStringLiteral("autoBreak"), autoBreakChk->isChecked());
    m_prefs.setValue(QStringLiteral("pinDesktop"), pinChk->isChecked());
    m_prefs.setValue(QStringLiteral("skin"), skinBox->currentData().toString());
    m_prefs.setValue(QStringLiteral("eyeProtect"), eyeChk->isChecked());
    m_prefs.setValue(QStringLiteral("eyeTemp"), tempBox->currentData().toInt());
    m_prefs.endGroup();

    applyPrefs();
    updateTimeLabel();
    updateActions();
    onStatusMessage(QStringLiteral("设置已保存"), QStringLiteral("露露会按新的节奏陪你"));
}

void MainWindow::notify(const QString &t, const QString &b)
{
    if (m_linker)
        m_linker->notify(t, b);
}
