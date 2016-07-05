/*
 * Copyright (C) by Klaas Freitag <freitag@kde.org>
 * Copyright (C) by Olivier Goffart <ogoffart@woboq.com>
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

#include <QInputDialog>
#include <QLabel>
#include <QTimer>
#include <QApplication>
#include "creds/httpcredentialsgui.h"
#include "theme.h"
#include "account.h"

using namespace QKeychain;

namespace OCC
{

class FocusChecker : public QObject
{
    Q_OBJECT
public slots:

    void check()
    {
        if (QApplication::activeWindow() != dialog) {
            lineEdit->clearFocus();

            // Stop cursor from blinking...
            // ... but it would need to be set writable once the window is activated
            // lineEdit->setReadOnly(true);
        }
    }

public:
    QInputDialog * dialog;
    QLineEdit * lineEdit;
};

void HttpCredentialsGui::askFromUser()
{
    // The rest of the code assumes that this will be done asynchronously
    QMetaObject::invokeMethod(this, "askFromUserAsync", Qt::QueuedConnection);
}

void HttpCredentialsGui::askFromUserAsync()
{
    QString msg = tr("Please enter %1 password:<br>"
                     "<br>"
                     "User: %2<br>"
                     "Account: %3<br>")
                  .arg(Utility::escape(Theme::instance()->appNameGUI()),
                       Utility::escape(_user),
                       Utility::escape(_account->displayName()));

    QString reqTxt = requestAppPasswordText(_account);
    if (!reqTxt.isEmpty()) {
        msg += QLatin1String("<br>") + reqTxt + QLatin1String("<br>");
    }
    if (!_fetchErrorString.isEmpty()) {
        msg += QLatin1String("<br>") + tr("Reading from keychain failed with error: '%1'").arg(
                    Utility::escape(_fetchErrorString)) + QLatin1String("<br>");
    }

    QInputDialog dialog;
    dialog.setWindowTitle(tr("Enter Password"));
    dialog.setLabelText(msg);
    dialog.setTextValue(_previousPassword);
    dialog.setTextEchoMode(QLineEdit::Password);
    if (QLabel *dialogLabel = dialog.findChild<QLabel *>()) {
        dialogLabel->setOpenExternalLinks(true);
        dialogLabel->setTextFormat(Qt::RichText);
    }
    FocusChecker checker;
    if (QLineEdit *lineEdit = dialog.findChild<QLineEdit *>()) {
        checker.dialog = &dialog;
        checker.lineEdit = lineEdit;
        QTimer::singleShot(50, &checker, SLOT(check()));
    }

    bool ok = dialog.exec();
    if (ok) {
        _password = dialog.textValue();
        _ready = true;
        persist();
    }
    emit asked();
}

QString HttpCredentialsGui::requestAppPasswordText(const Account* account)
{
    if (account->serverVersionInt() < 0x090100) {
        // Older server than 9.1 does not have trhe feature to request App Password
        return QString();
    }

    return tr("<a href=\"%1\">Click here</a> to request an app password from the web interface.")
        .arg(account->url().toString() + QLatin1String("/index.php/settings/personal?section=apppasswords"));
}


} // namespace OCC

#include "httpcredentialsgui.moc"
