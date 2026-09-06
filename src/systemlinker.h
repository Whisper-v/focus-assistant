#ifndef SYSTEMLINKER_H
#define SYSTEMLINKER_H

#include <QObject>

// 与 DDE 系统能力的轻量联动：
//  - 专注开始 -> 自动开启护眼(色温), 结束/休息 -> 恢复原状
//  - 通过 org.freedesktop.Notifications 发送桌面提醒
class SystemLinker : public QObject
{
    Q_OBJECT
public:
    explicit SystemLinker(QObject *parent = nullptr);

    void setEyeProtectionEnabled(bool on) { m_eyeProtectionEnabled = on; }
    bool eyeProtectionEnabled() const { return m_eyeProtectionEnabled; }
    void setEyeTemp(int kelvin) { m_eyeTemp = kelvin; }
    int eyeTemp() const { return m_eyeTemp; }
    bool eyeProtectionActive() const { return m_active; }
    void recoverFromCrash();   // 上次异常退出可能残留我们开启的色温，启动时清理

public slots:
    void onFocusStarted();   // 进入专注
    void onFocusEnded();     // 专注结束/中止/进入休息
    void notify(const QString &title, const QString &body, int timeoutMs = 6000);

signals:
    void eyeProtectionChanged(bool active);

private:
    void applyEyeProtection(bool on);
    void setColorTemperature(int kelvin);
    bool readProperty(const QString &iface, const QString &prop, QVariant *out) const;

    bool m_eyeProtectionEnabled = false;
    int  m_eyeTemp = 3500;
    bool m_active = false;        // 本次会话中由我们开启的护眼
    bool m_sessionApplied = false;
    int  m_prevManual = 0;        // 进入专注前的系统色温值，结束时还原
};

#endif // SYSTEMLINKER_H
