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
    setFixedWidth(360);

    buildUi();
    buildTray();
    applyPrefs();

    connect(m_mgr, &FocusManager::phaseChanged, this, &MainWindow::onPhaseChanged);
    connect(m_mgr, &FocusManager::tick, this, &MainWindow::onTick);
    connect(m_mgr, &FocusManager::focusCompleted, this, &MainWindow::onFocusCompleted);
    connect(m_mgr, &FocusManager::focusAborted, this, &MainWindow::onFocusAborted);
    connect(m_mgr, &FocusManager::stageChanged, this, &MainWindow::onStageChanged);
    connect(m_mgr, &FocusManager::breakFinished, this, &MainWindow::onBreakFinished);
    connect(m_mgr, &FocusManager::statusMessage, this, &MainWindow::onStatusMessage);

    updateMoodAndText();
    updateActions();
    updateStageLabel();
    updateTimeLabel();

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
}

MainWindow::~MainWindow()
{
    m_prefs.setValue(QStringLiteral("window/pos"), pos());
    m_prefs.sync();
}

void MainWindow::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 14, 24, 16);
    root->setSpacing(2);

    // ---- 顶栏 ----
    auto *head = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("🌱 专注助手"), this);
    QFont tf = font();
    tf.setPixelSize(15);
    tf.setBold(true);
    title->setFont(tf);
    title->setObjectName(QStringLiteral("headTitle"));
    head->addWidget(title);
    head->addStretch();
    m_statsBtn = new QPushButton(QStringLiteral("统计"), this);
    m_settingsBtn = new QPushButton(QStringLiteral("设置"), this);
    for (auto *b : { m_statsBtn, m_settingsBtn }) {
        b->setFixedSize(46, 24);
        b->setCursor(Qt::PointingHandCursor);
        QFont bf = font(); bf.setPixelSize(11);
        b->setFont(bf);
    }
    head->addWidget(m_statsBtn);
    head->addSpacing(4);
    head->addWidget(m_settingsBtn);
    root->addLayout(head);

    // ---- 露露 ----
    auto *petWrap = new QHBoxLayout;
    petWrap->addStretch();
    m_pet = new PetWidget(this);
    petWrap->addWidget(m_pet);
    petWrap->addStretch();
    root->addLayout(petWrap);

    // ---- 状态与时间 ----
    m_stateLabel = new QLabel(this);
    QFont sf = font(); sf.setPixelSize(13); sf.setBold(true);
    m_stateLabel->setFont(sf);
    m_stateLabel->setAlignment(Qt::AlignCenter);
    root->addWidget(m_stateLabel);

    m_timeLabel = new QLabel(this);
    QFont clock = font();
    clock.setPixelSize(42);
    clock.setWeight(QFont::DemiBold);
    m_timeLabel->setFont(clock);
    m_timeLabel->setAlignment(Qt::AlignCenter);
    root->addWidget(m_timeLabel);

    m_stageLabel = new QLabel(this);
    QFont stf = font(); stf.setPixelSize(11);
    m_stageLabel->setFont(stf);
    m_stageLabel->setAlignment(Qt::AlignCenter);
    m_stageLabel->setStyleSheet(QStringLiteral("color: rgba(130,130,130,0.9);"));
    root->addWidget(m_stageLabel);

    root->addSpacing(2);

    // ---- 操作按钮 ----
    auto *actions = new QHBoxLayout;
    actions->setSpacing(10);
    m_primary = new DSuggestButton(this);
    m_secondary = new DPushButton(this);
    m_primary->setMinimumHeight(34);
    m_secondary->setMinimumHeight(34);
    actions->addWidget(m_primary, 1);
    actions->addWidget(m_secondary, 1);
    root->addSpacing(6);
    root->addLayout(actions);

    // ---- 消息气泡 ----
    m_msgTitle = new QLabel(this);
    QFont mtf = font(); mtf.setPixelSize(12); mtf.setBold(true);
    m_msgTitle->setFont(mtf);
    m_msgTitle->setAlignment(Qt::AlignCenter);
    m_msgTitle->setWordWrap(true);
    root->addWidget(m_msgTitle);

    m_msgSub = new QLabel(this);
    QFont msf = font(); msf.setPixelSize(11);
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
    QAction *statsAct = m_trayMenu->addAction(QStringLiteral("成长记录"));
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

    const bool eyeOn = m_prefs.value(QStringLiteral("eyeProtect"), true).toBool();
    const int eyeTemp = m_prefs.value(QStringLiteral("eyeTemp"), 3500).toInt();
    m_linker->setEyeProtectionEnabled(eyeOn);
    m_linker->setEyeTemp(eyeTemp);
    m_prefs.endGroup();
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
    if (e->button() == Qt::LeftButton && childAt(e->pos()) == this) {
        m_dragging = true;
        m_dragOffset = e->globalPosition().toPoint() - frameGeometry().topLeft();
        e->accept();
    }
    QWidget::mousePressEvent(e);
}

void MainWindow::mouseMoveEvent(QMouseEvent *e)
{
    if (m_dragging) {
        move(e->globalPosition().toPoint() - m_dragOffset);
        e->accept();
        return;
    }
    QWidget::mouseMoveEvent(e);
}

void MainWindow::mouseReleaseEvent(QMouseEvent *e)
{
    m_dragging = false;
    QWidget::mouseReleaseEvent(e);
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

void MainWindow::onStageChanged(int)
{
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
    const bool eyeProtect = m_prefs.value(QStringLiteral("eyeProtect"), true).toBool();
    const int eyeTemp = m_prefs.value(QStringLiteral("eyeTemp"), 3500).toInt();
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

    auto *eyeChk = new QCheckBox(QStringLiteral("专注时自动开启护眼模式"), &dlg);
    eyeChk->setChecked(eyeProtect);

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
    form->addRow(QString(), autoBreakChk);
    form->addRow(QString(), eyeChk);
    form->addRow(QStringLiteral("护眼色温"), tempBox);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("保存"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(buttons);

    if (dlg.exec() != QDialog::Accepted)
        return;

    m_prefs.beginGroup(QStringLiteral("prefs"));
    m_prefs.setValue(QStringLiteral("focusMin"), focusBox->currentData().toInt());
    m_prefs.setValue(QStringLiteral("breakMin"), breakBox->currentData().toInt());
    m_prefs.setValue(QStringLiteral("longBreakMin"), longBox->currentData().toInt());
    m_prefs.setValue(QStringLiteral("perLong"), perSpin->value());
    m_prefs.setValue(QStringLiteral("autoBreak"), autoBreakChk->isChecked());
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
