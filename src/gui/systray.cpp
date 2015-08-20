/*
 * Copyright (C) by Cédric Bellegarde <gnumdk@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "systray.h"
#include "theme.h"
#include <QDebug>

#ifdef USE_FDO_NOTIFICATIONS
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#define NOTIFICATIONS_SERVICE "org.freedesktop.Notifications"
#define NOTIFICATIONS_PATH "/org/freedesktop/Notifications"
#define NOTIFICATIONS_IFACE "org.freedesktop.Notifications"
#endif

#if QT_VERSION == QT_VERSION_CHECK(5, 5, 0)
#include <QPointer>
#include <QMenu>

class QObjectPrivate : public QObjectData
{
public:
    static void* get(QObject* o)
    {
        return o->d_func();
    }

    void* extraData;
    void* threadData;
    void* connectionLists;
    void* senders;
    void* currentSender;
    quint32 connectedSignals[2];
    void* union_ptr;
    void* atomic_ptr;
};

class QSystemTrayIconPrivate : public QObjectPrivate
{
public:
    QPointer<QMenu> menu;
    QIcon icon;
    QString toolTip;
    QObject* sys;
    QObject* qpa_sys;
};

#endif

namespace OCC {

Systray::Systray()
{
#if QT_VERSION == QT_VERSION_CHECK(5, 5, 0)
    QSystemTrayIconPrivate* priv =
            reinterpret_cast<QSystemTrayIconPrivate*>(QObjectPrivate::get(this));
    delete priv->qpa_sys;
    priv->qpa_sys = 0;
#endif
}

void Systray::showMessage(const QString & title, const QString & message, MessageIcon icon, int millisecondsTimeoutHint)
{

#ifdef USE_FDO_NOTIFICATIONS
    if(QDBusInterface(NOTIFICATIONS_SERVICE, NOTIFICATIONS_PATH, NOTIFICATIONS_IFACE).isValid()) {
        QList<QVariant> args = QList<QVariant>() << "owncloud" << quint32(0) << "owncloud"
                                                 << title << message << QStringList () << QVariantMap() << qint32(-1);
        QDBusMessage method = QDBusMessage::createMethodCall(NOTIFICATIONS_SERVICE, NOTIFICATIONS_PATH, NOTIFICATIONS_IFACE, "Notify");
        method.setArguments(args);
        QDBusConnection::sessionBus().asyncCall(method);
    } else
#endif
#ifdef Q_OS_OSX
    if (canOsXSendUserNotification()) {
        sendOsXUserNotification(title, message);
    } else
#endif
    {
        QSystemTrayIcon::showMessage(title, message, icon, millisecondsTimeoutHint);
    }
}

void Systray::setToolTip(const QString &tip)
{
    QSystemTrayIcon::setToolTip(tr("%1: %2").arg(Theme::instance()->appNameGUI(), tip));
}

} // namespace OCC
