#include "focusmanager.h"
#include <QtMath>

static const int kStageThresholdMinutes[4] = { 0, 15, 60, 200 }; // 成长阶段阈值(累计专注分钟)

FocusManager::FocusManager(QObject *parent)
    : QObject(parent)
    , m_settings(QStringLiteral("focus-assistant"), QStringLiteral("focus-assistant"))
{
    m_secondTimer.setInterval(1000);
    connect(&m_secondTimer, &QTimer::timeout, this, &FocusManager::onSecond);
    loadStats();
    m_loading = false;
}

void FocusManager::loadStats()
{
    m_loading = true;
    m_settings.beginGroup(QStringLiteral("stats"));
    m_completedSessions = m_settings.value(QStringLiteral("sessions"), 0).toInt();
    m_totalFocusSeconds = m_settings.value(QStringLiteral("totalSeconds"), 0).toLongLong();
    m_lastDay = m_settings.value(QStringLiteral("lastDay")).toString();
    m_sessionsInRow = m_settings.value(QStringLiteral("sessionsInRow"), 0).toInt();
    m_settings.endGroup();

    const QString today = todayKey();
    if (m_lastDay != today) {           // 跨天：重置连续专注
        m_lastDay = today;
        m_sessionsInRow = 0;
    }
    m_todayFocusSeconds = m_settings.value(QStringLiteral("days/") + today, 0).toInt();
    computeStage();
    m_loading = false;
    saveStats();
}

void FocusManager::saveStats()
{
    m_settings.beginGroup(QStringLiteral("stats"));
    m_settings.setValue(QStringLiteral("sessions"), m_completedSessions);
    m_settings.setValue(QStringLiteral("totalSeconds"), m_totalFocusSeconds);
    m_settings.setValue(QStringLiteral("lastDay"), m_lastDay);
    m_settings.setValue(QStringLiteral("sessionsInRow"), m_sessionsInRow);
    m_settings.endGroup();
    m_settings.setValue(QStringLiteral("days/") + todayKey(), m_todayFocusSeconds);
    m_settings.sync();
}

QList<FocusManager::DayStat> FocusManager::lastSevenDays() const
{
    QList<DayStat> list;
    for (int i = 6; i >= 0; --i) {
        const QDate d = QDate::currentDate().addDays(-i);
        const int secs = m_settings.value(QStringLiteral("days/") + d.toString(QStringLiteral("yyyy-MM-dd")), 0).toInt();
        list.append({ d.toString(QStringLiteral("M/d")), secs });
    }
    return list;
}

void FocusManager::setConfig(const Config &c)
{
    m_cfg = c;
}

void FocusManager::computeStage()
{
    const qint64 minutes = m_totalFocusSeconds / 60;
    int newStage = 0;
    if (minutes >= kStageThresholdMinutes[3])
        newStage = 3;
    else if (minutes >= kStageThresholdMinutes[2])
        newStage = 2;
    else if (minutes >= kStageThresholdMinutes[1])
        newStage = 1;
    if (newStage != m_stage) {
        m_stage = newStage;
        if (!m_loading)
            emit stageChanged(m_stage);
    }
}

QString FocusManager::stageName() const
{
    switch (m_stage) {
    case 0: return QStringLiteral("沉睡的种子");
    case 1: return QStringLiteral("青葱幼苗");
    case 2: return QStringLiteral("含苞待放");
    default: return QStringLiteral("花团锦簇");
    }
}

int FocusManager::nextStageRemainSessions() const
{
    if (m_stage >= 3)
        return -1;
    const qint64 targetSec = static_cast<qint64>(kStageThresholdMinutes[m_stage + 1]) * 60;
    const qint64 need = targetSec - m_totalFocusSeconds;
    if (need <= 0)
        return 0;
    const int secs = m_cfg.focusSeconds > 0 ? m_cfg.focusSeconds : 1500;
    return static_cast<int>(qCeil(static_cast<qreal>(need) / secs));
}

void FocusManager::startFocus()
{
    if (m_phase == Focus)
        return;
    if (m_phase == Pause) {              // 继续
        resumeFocus();
        return;
    }
    if (m_phase == Break) {              // 主动结束休息，进入下一轮
        if (m_isLongBreak)
            m_sessionsInRow = 0;
        m_isLongBreak = false;
        m_secondTimer.stop();
    }
    rolloverDay();
    m_total = m_cfg.focusSeconds;
    m_remaining = m_total;
    m_elapsed = 0;
    m_secondTimer.start();
    setPhase(Focus);
    emit statusMessage(QStringLiteral("一起专注吧"), QStringLiteral("露为你施了「心无旁骛」的魔法"));
}

void FocusManager::pauseFocus()
{
    if (m_phase != Focus)
        return;
    m_secondTimer.stop();
    setPhase(Pause);
    emit statusMessage(QStringLiteral("专注暂停中"), QStringLiteral("别走太远，露会等你回来"));
}

void FocusManager::resumeFocus()
{
    if (m_phase != Pause)
        return;
    m_secondTimer.start();
    setPhase(Focus);
}

void FocusManager::abortFocus()
{
    if (m_phase != Focus && m_phase != Pause)
        return;
    m_secondTimer.stop();
    m_remaining = 0;
    m_elapsed = 0;
    m_isLongBreak = false;
    setPhase(Idle);
    emit focusAborted();
    emit statusMessage(QStringLiteral("本次专注未完成"), QStringLiteral("没关系，休息一下再出发"));
}

void FocusManager::skipBreak()
{
    if (m_phase != Break)
        return;
    m_secondTimer.stop();
    const bool wasLong = m_isLongBreak;
    if (wasLong)
        m_sessionsInRow = 0;
    m_isLongBreak = false;
    m_remaining = 0;
    m_elapsed = 0;
    setPhase(Idle);
    emit breakFinished(wasLong);
}

void FocusManager::resetAll()
{
    m_secondTimer.stop();
    m_remaining = 0;
    m_elapsed = 0;
    m_isLongBreak = false;
    m_completedSessions = 0;
    m_totalFocusSeconds = 0;
    m_sessionsInRow = 0;
    m_todayFocusSeconds = 0;
    m_stage = 0;
    saveStats();
    setPhase(Idle);
    emit statsChanged();
    emit stageChanged(0);
}

void FocusManager::onSecond()
{
    if (m_phase != Focus && m_phase != Break)
        return;
    if (m_remaining <= 0) {              // 理论不可达，防御性处理
        if (m_phase == Focus)
            completeFocus();
        else
            finishBreak();
        return;
    }
    --m_remaining;
    ++m_elapsed;
    emit tick(m_remaining);
    if (m_remaining == 0) {
        if (m_phase == Focus)
            completeFocus();
        else
            finishBreak();
    }
}

void FocusManager::completeFocus()
{
    m_secondTimer.stop();
    m_totalFocusSeconds += m_elapsed;    // m_elapsed == 本次完整专注秒数
    m_todayFocusSeconds += m_elapsed;
    ++m_completedSessions;
    ++m_sessionsInRow;
    saveStats();
    emit statsChanged();
    emit focusCompleted(m_sessionsInRow, m_stage);
    computeStage();                       // 可能有阶段提升 -> stageChanged

    m_remaining = 0;
    m_elapsed = 0;
    if (!m_cfg.autoBreak) {
        m_isLongBreak = false;
        setPhase(Idle);
        return;
    }
    startBreak(m_sessionsInRow % m_cfg.sessionsPerLongBreak == 0);
}

void FocusManager::finishBreak()
{
    m_secondTimer.stop();
    const bool wasLong = m_isLongBreak;
    if (wasLong)
        m_sessionsInRow = 0;
    m_isLongBreak = false;
    m_remaining = 0;
    m_elapsed = 0;
    setPhase(Idle);
    emit breakFinished(wasLong);
    emit statusMessage(QStringLiteral("休息结束啦"),
                       wasLong ? QStringLiteral("长休完毕，露满血复活！") : QStringLiteral("放松一下，效率更高"));
}

void FocusManager::startBreak(bool longBreak)
{
    m_isLongBreak = longBreak;
    m_total = longBreak ? m_cfg.longBreakSeconds : m_cfg.breakSeconds;
    m_remaining = m_total;
    m_elapsed = 0;
    m_secondTimer.start();
    setPhase(Break);
}

void FocusManager::setPhase(Phase p)
{
    if (m_phase == p)
        return;
    m_phase = p;
    emit phaseChanged(m_phase);
}

void FocusManager::rolloverDay()
{
    const QString today = todayKey();
    if (m_lastDay == today)
        return;
    m_lastDay = today;
    m_sessionsInRow = 0;
    m_todayFocusSeconds = 0;
    saveStats();
}
