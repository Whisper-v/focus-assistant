#include "systemlinker.h"

#include <QDBusInterface>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QVariant>
#include <QStringList>
#include <QDebug>

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
        qWarning() << "[FocusGarden] Display1 not available";
        return false;
    }
    const QVariant v = dbus.property(prop.toLatin1());
    if (!v.isValid()) {
        qWarning() << "[FocusGarden] read property failed:" << prop;
        return false;
    }
    *out = v;
    return true;
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
        if (vEnabled.toBool()) {       // 用户本就开启色温(夜间/自定义)，不打扰
            m_active = false;
            return;
        }
        dbus.setProperty(QStringLiteral("ColorTemperatureManual").toLatin1(), m_eyeTemp);
        dbus.setProperty(QStringLiteral("ColorTemperatureEnabled").toLatin1(), true);
        m_active = true;
        emit eyeProtectionChanged(true);
    } else {
        if (!m_sessionApplied)
            return;
        m_sessionApplied = false;
        if (m_active) {
            dbus.setProperty(QStringLiteral("ColorTemperatureEnabled").toLatin1(), false);
            m_active = false;
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
    args << QStringLiteral("focus-garden")          // app_name
         << QVariant(0U)                            // replaces_id
         << QStringLiteral("focus-garden")          // app_icon
         << title
         << body
         << QStringList()                           // actions
         << QVariantMap()                           // hints
         << QVariant(timeoutMs);                    // expire_timeout
    msg.setArguments(args);
    QDBusConnection::sessionBus().send(msg);
}
