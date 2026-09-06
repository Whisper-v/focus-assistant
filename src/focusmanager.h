#ifndef FOCUSMANAGER_H
#define FOCUSMANAGER_H

#include <QObject>
#include <QTimer>
#include <QSettings>
#include <QDateTime>

class FocusManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Phase phase READ phase NOTIFY phaseChanged)
public:
    enum Phase { Idle, Focus, Pause, Break };
    Q_ENUM(Phase)

    struct Config {
        int focusSeconds = 25 * 60;
        int breakSeconds = 5 * 60;
        int longBreakSeconds = 15 * 60;
        int sessionsPerLongBreak = 4;
        bool autoBreak = true;
    };

    explicit FocusManager(QObject *parent = nullptr);

    Phase phase() const { return m_phase; }
    int remaining() const { return m_remaining; }
    int totalSeconds() const { return m_total; }
    int elapsed() const { return m_elapsed; }
    bool isLongBreak() const { return m_isLongBreak; }

    int stage() const { return m_stage; }
    int sessionsInRow() const { return m_sessionsInRow; }
    int completedSessions() const { return m_completedSessions; }
    qint64 totalFocusSeconds() const { return m_totalFocusSeconds; }
    int todayFocusSeconds() const { return m_todayFocusSeconds; }
    QString stageName() const;
    int nextStageRemainSessions() const; // sessions needed to reach next stage (-1 if max)
    struct DayStat { QString label; int seconds; };
    QList<DayStat> lastSevenDays() const;

    const Config &config() const { return m_cfg; }
    void setConfig(const Config &c);

public slots:
    void startFocus();
    void pauseFocus();
    void resumeFocus();
    void abortFocus();          // 提前结束，本次不计入成长
    void skipBreak();
    void resetAll();

signals:
    void phaseChanged(Phase phase);
    void tick(int remaining);
    void focusCompleted(int sessionsInRow, int stage);
    void focusAborted();
    void breakFinished(bool wasLong);
    void stageChanged(int stage);
    void statsChanged();
    void statusMessage(const QString &text, const QString &subText);

private slots:
    void onSecond();

private:
    void setPhase(Phase p);
    void completeFocus();
    void finishBreak();
    void startBreak(bool longBreak);
    void rolloverDay();
    void computeStage();
    void saveStats();
    void loadStats();
    QString todayKey() const { return QDate::currentDate().toString("yyyy-MM-dd"); }

    Phase m_phase = Idle;
    Config m_cfg;
    QTimer m_secondTimer;
    int m_remaining = 0;
    int m_total = 0;
    int m_elapsed = 0;
    bool m_isLongBreak = false;

    // persisted stats
    QSettings m_settings;
    int m_stage = 0;
    int m_completedSessions = 0;
    qint64 m_totalFocusSeconds = 0;
    int m_todayFocusSeconds = 0;
    int m_sessionsInRow = 0;
    QString m_lastDay;
    bool m_loading = true;   // 加载阶段抑制 stageChanged
};

#endif // FOCUSMANAGER_H
