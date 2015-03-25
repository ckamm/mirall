/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "accountstate.h"
#include "quotainfo.h"
#include "account.h"
#include "creds/abstractcredentials.h"

#include <QDebug>

namespace OCC {

Q_GLOBAL_STATIC(AccountStateManager, g_accountStateManager)

AccountStateManager *AccountStateManager::instance()
{
    return g_accountStateManager();
}

AccountStateManager::AccountStateManager()
    : _accountState(0)
{
    connect(AccountManager::instance(), SIGNAL(accountAdded(AccountPtr)),
            SLOT(slotAccountAdded(AccountPtr)));
}

AccountStateManager::~AccountStateManager()
{}

void AccountStateManager::setAccountState(AccountState *accountState)
{
    if (_accountState) {
        emit accountStateRemoved(_accountState);
    }
    _accountState = accountState;
    emit accountStateAdded(accountState);
}

void AccountStateManager::slotAccountAdded(AccountPtr account)
{
    setAccountState(new AccountState(account));
}

AccountState::AccountState(AccountPtr account)
    : QObject(account.data())
    , _account(account)
    , _quotaInfo(new QuotaInfo(this))
    , _state(AccountState::Disconnected)
    , _connectionStatus(ConnectionValidator::Undefined)
    , _waitingForNewCredentials(false)
{
    qRegisterMetaType<AccountState*>("AccountState*");

    connect(account.data(), SIGNAL(invalidCredentials(AbstractCredentials*)),
            SLOT(slotInvalidCredentials(AbstractCredentials*)));
    connect(account.data(), SIGNAL(credentialsFetched(AbstractCredentials*)),
            SLOT(slotCredentialsFetched(AbstractCredentials*)));
}

AccountState::~AccountState()
{
}

AccountPtr AccountState::account() const
{
    return _account.toStrongRef();
}

AccountState::ConnectionStatus AccountState::connectionStatus() const
{
    return _connectionStatus;
}

QStringList AccountState::connectionErrors() const
{
    return _connectionErrors;
}

QString AccountState::connectionStatusString(ConnectionStatus status)
{
    return ConnectionValidator::statusString(status);
}

AccountState::State AccountState::state() const
{
    return _state;
}

void AccountState::setState(State state)
{
    if (_state != state) {
        qDebug() << "AccountState state change: "
                 << stateString(_state) << "->" << stateString(state);
        State oldState = _state;
        _state = state;

        if (_state == SignedOut) {
            _connectionStatus = ConnectionValidator::Undefined;
            _connectionErrors.clear();
        } else if (_state == Disconnected
                   && (oldState == SignedOut || oldState == TemporaryCredentialError)) {
            checkConnectivity();
        }

        emit stateChanged(_state);
    }
}

QString AccountState::stateString(State state)
{
    switch (state)
    {
    case SignedOut:
        return QLatin1String("SignedOut");
    case Disconnected:
        return QLatin1String("Disconnected");
    case Connected:
        return QLatin1String("Connected");
    case ServerMaintenance:
        return QLatin1String("ServerMaintenance");
    case NetworkError:
        return QLatin1String("NetworkError");
    case ConfigurationError:
        return QLatin1String("ConfigurationError");
    case TemporaryCredentialError:
        return QLatin1String("TemporaryCredentialError");
    }
    return QLatin1String("Unknown");
}

bool AccountState::isSignedOut() const
{
    return _state == SignedOut;
}

void AccountState::setSignedOut(bool signedOut)
{
    if (signedOut) {
        setState(SignedOut);
    } else {
        setState(Disconnected);
    }
}

bool AccountState::isConnected() const
{
    return _state == Connected;
}

bool AccountState::isConnectedOrMaintenance() const
{
    return isConnected() || _state == ServerMaintenance;
}

QuotaInfo *AccountState::quotaInfo()
{
    return _quotaInfo;
}

void AccountState::checkConnectivity()
{
    if (isSignedOut() || _waitingForNewCredentials) {
        return;
    }

    ConnectionValidator * conValidator = new ConnectionValidator(account());
    connect(conValidator, SIGNAL(connectionResult(ConnectionValidator::Status,QStringList)),
            SLOT(slotConnectionValidatorResult(ConnectionValidator::Status,QStringList)));
    if (isConnected()) {
        // Use a small authed propfind as a minimal ping when we're
        // already connected.
        conValidator->checkAuthentication();
    } else {
        // Check the server and then the auth.

#ifdef Q_OS_WIN
        // There seems to be a bug in Qt on Windows where QNAM sometimes stops
        // working correctly after the computer woke up from sleep. See #2895 #2899
        // and #2973.
        // As an attempted workaround, reset the QNAM regularly if the account is
        // disconnected.
        account()->resetNetworkAccessManager();
#endif
        conValidator->checkServerAndAuth();
    }
}

void AccountState::slotConnectionValidatorResult(ConnectionValidator::Status status, const QStringList& errors)
{
    if (isSignedOut()) {
        return;
    }

    if (_connectionStatus != status) {
        qDebug() << "AccountState connection status change: "
                 << connectionStatusString(_connectionStatus) << "->"
                 << connectionStatusString(status);
        _connectionStatus = status;
    }
    _connectionErrors = errors;

    switch (status)
    {
    case ConnectionValidator::Connected:
        setState(Connected);
        break;
    case ConnectionValidator::Undefined:
    case ConnectionValidator::NotConfigured:
        setState(Disconnected);
        break;
    case ConnectionValidator::ServerVersionMismatch:
        setState(ConfigurationError);
        break;
    case ConnectionValidator::StatusNotFound:
        // This can happen either because the server does not exist
        // or because we are having network issues. The latter one is
        // much more likely, so keep trying to connect.
        setState(NetworkError);
        break;
    case ConnectionValidator::CredentialsWrong:
        account()->handleInvalidCredentials();
        break;
    case ConnectionValidator::UserCanceledCredentials:
        setState(SignedOut);
        break;
    case ConnectionValidator::ServerMaintenance:
        setState(ServerMaintenance);
        break;
    case ConnectionValidator::Timeout:
        setState(NetworkError);
        break;
    }
}

void AccountState::slotInvalidCredentials(AbstractCredentials* credentials)
{
    qDebug() << "credentials were invalid";

    // If we were connected and suddenly get a credential failure,
    // try again a couple of times to see whether it solves itself
    // before bothering the user.
    if (isConnected()) {
        _firstInvalidCredentialTimer.restart();
        setState(TemporaryCredentialError);
        return;
    }

    // If we get another credential error while we still think it
    // might solve itself, ignore the failure.
    // 35s lets one run of the checkConnectionTimer (every 32s) take place
    // and stops retrying on the third failure.
    const int msIgnoreCredentialError = 35 * 1000;
    if (_state == TemporaryCredentialError
            && _firstInvalidCredentialTimer.elapsed() < msIgnoreCredentialError) {
        return;
    }

    // invalidate & forget token/password
    // but try to re-sign in.
    if (credentials->ready()) {
        credentials->invalidateAndFetch();
    } else {
        credentials->fetch();
    }

    // Go into the ConfigurationError state unless the user signed
    // out explicitly.
    if (! isSignedOut()) {
        setState(ConfigurationError);
        _waitingForNewCredentials = true;
    }
}

void AccountState::slotCredentialsFetched(AbstractCredentials* credentials)
{
    _waitingForNewCredentials = false;

    if (!credentials->ready()) {
        // User canceled the connection or did not give a password
        setState(SignedOut);
        return;
    }

    checkConnectivity();
}

} // namespace OCC
