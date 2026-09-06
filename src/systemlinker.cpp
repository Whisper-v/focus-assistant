#include "systemlinker.h"

#include <QDBusInterface>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QVariant>
#include <QStringList>
#include <QDebug>
#include <QSettings>

static const QString kDisplayService = QStringLiteral("org.deepin.dde.Display1");
static const QString kDisplayPath    = QStringLiteral("/org/deepin/dde/Display1");
static const QString kDisplayIface   = QStringLiteral("org.deepin.dde.Display1");

SystemLinker::SystemLinker(QObject *parent)
    : QObject(parent)
{
}

bool SystemLinker::readProperty(const QString &iface, const QString &prop, QVariant *out) const
{
    QDBusInterface dbus(kDisplayService, kDisplayPath, kDisplayIface, QDBusConnection::sessionBus());
    if (!dbus.isValid()) {
        qWarning() << "[FocusAssistant] Display1 not available";
        return false;
    }
    const QVariant v = dbus.property(prop.toLatin1());
    if (!v.isValid()) {
        qWarning() << "[FocusAssistant] read property failed:" << prop;
        return false;
    }
    *out = v;
    return true;
}

void SystemLinker::setColorTemperature(int kelvin)
{
    // 正确姿势：ColorTemperatureManual 为只读属性，
    // 需调用 daemon 的 SetColorTemperature 方法设置色温值。
    QDBusInterface dbus(kDisplayService, kDisplayPath, kDisplayIface, QDBusConnection::sessionBus());
    if (!dbus.isValid())
        return;
    dbus.call(QStringLiteral("SetColorTemperature"), kelvin);
}

void SystemLinker::applyEyeProtection(bool on)
{
    QDBusInterface dbus(kDisplayService, kDisplayPath, kDisplayIface, QDBusConnection::sessionBus());
    if (!dbus.isValid())
        return;

    if (on) {
        if (m_sessionApplied)
            return;
        QVariant vEnabled, vManual;
        if (!readProperty(kDisplayIface, QStringLiteral("ColorTemperatureEnabled"), &vEnabled)
            || !readProperty(kDisplayIface, QStringLiteral("ColorTemperatureManual"), &vManual)) {
            return;
        }
        m_sessionApplied = true;
        if (vEnabled.toBool()) {   // 用户本就开启色温(夜间/自定义)：不打扰，也不改动其值
            m_active = false;
            return;
        }
        m_prevManual = vManual.toInt();
        setColorTemperature(m_eyeTemp);
        dbus.setProperty(QStringLiteral("ColorTemperatureEnabled").toLatin1(), true);
        m_active = true;
        // 记录残留标记：若此后进程被强杀，下次启动可自动清理
        QSettings mark(QStringLiteral("focus-assistant"), QStringLiteral("focus-assistant"));
        mark.setValue(QStringLiteral("eye/active"), true);
        mark.setValue(QStringLiteral("eye/prevManual"), m_prevManual);
        mark.sync();
        emit eyeProtectionChanged(true);
    } else {
        if (!m_sessionApplied)
            return;
        m_sessionApplied = false;
        if (m_active) {
            // 还原我们开启的色温，并把色温值恢复为进入前的数值
            dbus.setProperty(QStringLiteral("ColorTemperatureEnabled").toLatin1(), false);
            if (m_prevManual > 0 && m_prevManual != m_eyeTemp)
                setColorTemperature(m_prevManual);
            m_active = false;
            m_prevManual = 0;
            QSettings mark(QStringLiteral("focus-assistant"), QStringLiteral("focus-assistant"));
            mark.remove(QStringLiteral("eye"));
            mark.sync();
            emit eyeProtectionChanged(false);
        }
    }
}

void SystemLinker::onFocusStarted()
{
    if (m_eyeProtectionEnabled)
        applyEyeProtection(true);
}

void SystemLinker::onFocusEnded()
{
    applyEyeProtection(false);
}

void SystemLinker::notify(const QString &title, const QString &body, int timeoutMs)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("/org/freedesktop/Notifications"),
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("Notify"));
    QVariantList args;
    args << QStringLiteral("focus-assistant")          // app_name
         << QVariant(0U)                            // replaces_id
         << QStringLiteral("focus-assistant")          // app_icon
         << title
         << body
         << QStringList()                           // actions
         << QVariantMap()                           // hints
         << QVariant(timeoutMs);                    // expire_timeout
    msg.setArguments(args);
    QDBusConnection::sessionBus().send(msg);
}

void SystemLinker::recoverFromCrash()
{
    QSettings mark(QStringLiteral("focus-assistant"), QStringLiteral("focus-assistant"));
    if (!mark.contains(QStringLiteral("eye/active")))
        return;
    const int prevManual = mark.value(QStringLiteral("eye/prevManual"), 0).toInt();
    mark.remove(QStringLiteral("eye"));
    mark.sync();

    // 仅当系统当前确实开着色温且是我们留下的时才还原（避免干扰用户手动开启的场景）
    QVariant vEnabled;
    if (!readProperty(kDisplayIface, QStringLiteral("ColorTemperatureEnabled"), &vEnabled))
        return;
    if (!vEnabled.toBool())
        return;

    QDBusInterface dbus(kDisplayService, kDisplayPath, kDisplayIface, QDBusConnection::sessionBus());
    if (!dbus.isValid())
        return;
    dbus.setProperty(QStringLiteral("ColorTemperatureEnabled").toLatin1(), false);
    if (prevManual > 0)
        setColorTemperature(prevManual);
    qInfo() << "[FocusAssistant] 清理上次异常退出残留的护眼色温";
}
