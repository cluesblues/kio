/*
 * Copyright (C) 2000 Alex Zepeda <zipzippy@sonic.net>
 * Copyright (C) 2001-2003 George Staikos <staikos@kde.org>
 * Copyright (C) 2001 Dawit Alemayehu <adawit@kde.org>
 * Copyright (C) 2007,2008 Andreas Hartmetz <ahartmetz@gmail.com>
 * Copyright (C) 2008 Roland Harnau <tau@gmx.eu>
 * Copyright (C) 2010 Richard Moore <rich@kde.org>
 *
 * This file is part of the KDE project
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "tcpslavebase.h"
#include "kiocoredebug.h"

#include <kconfiggroup.h>
#include <ksslcertificatemanager.h>
#include <ksslsettings.h>
#include <klocalizedstring.h>

#include <QSslCipher>
#include <QSslSocket>

#include <QDBusConnection>

using namespace KIO;
//using namespace KNetwork;

namespace KIO
{
Q_DECLARE_OPERATORS_FOR_FLAGS(TCPSlaveBase::SslResult)
}

//TODO Proxy support whichever way works; KPAC reportedly does *not* work.
//NOTE kded_proxyscout may or may not be interesting

//TODO resurrect SSL session recycling; this means save the session on disconnect and look
//for a reusable session on connect. Consider how HTTP persistent connections interact with that.

//TODO in case we support SSL-lessness we need static KTcpSocket::sslAvailable() and check it
//in most places we ATM check for d->isSSL.

//TODO check if d->isBlocking is honored everywhere it makes sense

//TODO fold KSSLSetting and KSSLCertificateHome into KSslSettings and use that everywhere.

//TODO recognize partially encrypted websites as "somewhat safe"

/* List of dialogs/messageboxes we need to use (current code location in parentheses)
   - Can the "dontAskAgainName" thing be improved?

   - "SSLCertDialog" [select client cert] (SlaveInterface)
   - Enter password for client certificate (inline)
   - Password for client cert was wrong. Please reenter. (inline)
   - Setting client cert failed. [doesn't give reason] (inline)
   - "SSLInfoDialog" [mostly server cert info] (SlaveInterface)
   - You are about to enter secure mode. Security information/Display SSL information/Connect (inline)
   - You are about to leave secure mode. Security information/Continue loading/Abort (inline)
   - Hostname mismatch: Continue/Details/Cancel (inline)
   - IP address mismatch: Continue/Details/Cancel (inline)
   - Certificate failed authenticity check: Continue/Details/Cancel (inline)
   - Would you like to accept this certificate forever: Yes/No/Current sessions only (inline)
 */

/** @internal */
class Q_DECL_HIDDEN TCPSlaveBase::TcpSlaveBasePrivate
{
public:
    explicit TcpSlaveBasePrivate(TCPSlaveBase *qq)
        : q(qq)
    {}

    void setSslMetaData()
    {
        sslMetaData.insert(QStringLiteral("ssl_in_use"), QStringLiteral("TRUE"));
        QSslCipher cipher = socket.sessionCipher();
        sslMetaData.insert(QStringLiteral("ssl_protocol_version"), cipher.protocolString());
        sslMetaData.insert(QStringLiteral("ssl_cipher"), cipher.name());
        sslMetaData.insert(QStringLiteral("ssl_cipher_used_bits"), QString::number(cipher.usedBits()));
        sslMetaData.insert(QStringLiteral("ssl_cipher_bits"), QString::number(cipher.supportedBits()));
        sslMetaData.insert(QStringLiteral("ssl_peer_ip"), ip);

        const QList<QSslCertificate> peerCertificateChain = socket.peerCertificateChain();
        // try to fill in the blanks, i.e. missing certificates, and just assume that
        // those belong to the peer (==website or similar) certificate.
        for (int i = 0; i < sslErrors.count(); i++) {
            if (sslErrors[i].certificate().isNull()) {
                sslErrors[i] = QSslError(sslErrors[i].error(), peerCertificateChain[0]);
            }
        }

        QString errorStr;
        // encode the two-dimensional numeric error list using '\n' and '\t' as outer and inner separators
        for (const QSslCertificate &cert : peerCertificateChain ) {
            for (const QSslError &error : qAsConst(sslErrors)) {
                if (error.certificate() == cert) {
                    errorStr += QString::number(static_cast<int>(error.error())) + QLatin1Char('\t');
                }
            }
            if (errorStr.endsWith(QLatin1Char('\t'))) {
                errorStr.chop(1);
            }
            errorStr += QLatin1Char('\n');
        }
        errorStr.chop(1);
        sslMetaData.insert(QStringLiteral("ssl_cert_errors"), errorStr);

        QString peerCertChain;
        for (const QSslCertificate &cert : peerCertificateChain) {
            peerCertChain += QString::fromUtf8(cert.toPem()) + QLatin1Char('\x01');
        }
        peerCertChain.chop(1);
        sslMetaData.insert(QStringLiteral("ssl_peer_chain"), peerCertChain);
        sendSslMetaData();
    }

    void clearSslMetaData()
    {
        sslMetaData.clear();
        sslMetaData.insert(QStringLiteral("ssl_in_use"), QStringLiteral("FALSE"));
        sendSslMetaData();
    }

    void sendSslMetaData()
    {
        MetaData::ConstIterator it = sslMetaData.constBegin();
        for (; it != sslMetaData.constEnd(); ++it) {
            q->setMetaData(it.key(), it.value());
        }
    }

    SslResult startTLSInternal(QSsl::SslProtocol sslVersion,
                               int waitForEncryptedTimeout = -1);

    TCPSlaveBase * const q;

    bool isBlocking;

    QSslSocket socket;

    QString host;
    QString ip;
    quint16 port;
    QByteArray serviceName;

    KSSLSettings sslSettings;
    bool usingSSL;
    bool autoSSL;
    bool sslNoUi; // If true, we just drop the connection silently
    // if SSL certificate check fails in some way.
    QList<QSslError> sslErrors;

    MetaData sslMetaData;
};

//### uh, is this a good idea??
QIODevice *TCPSlaveBase::socket() const
{
    return &d->socket;
}

TCPSlaveBase::TCPSlaveBase(const QByteArray &protocol,
                           const QByteArray &poolSocket,
                           const QByteArray &appSocket,
                           bool autoSSL)
    : SlaveBase(protocol, poolSocket, appSocket),
      d(new TcpSlaveBasePrivate(this))
{
    d->isBlocking = true;
    d->port = 0;
    d->serviceName = protocol;
    d->usingSSL = false;
    d->autoSSL = autoSSL;
    d->sslNoUi = false;
    // Limit the read buffer size to 14 MB (14*1024*1024) (based on the upload limit
    // in TransferJob::slotDataReq). See the docs for QAbstractSocket::setReadBufferSize
    // and the BR# 187876 to understand why setting this limit is necessary.
    d->socket.setReadBufferSize(14680064);
}

TCPSlaveBase::~TCPSlaveBase()
{
    delete d;
}

ssize_t TCPSlaveBase::write(const char *data, ssize_t len)
{
    ssize_t written = d->socket.write(data, len);
    if (written == -1) {
        /*qDebug() << "d->socket.write() returned -1! Socket error is"
          << d->socket.error() << ", Socket state is" << d->socket.state();*/
    }

    bool success = false;
    if (d->isBlocking) {
        // Drain the tx buffer
        success = d->socket.waitForBytesWritten(-1);
    } else {
        // ### I don't know how to make sure that all data does get written at some point
        // without doing it now. There is no event loop to do it behind the scenes.
        // Polling in the dispatch() loop? Something timeout based?
        success = d->socket.waitForBytesWritten(0);
    }

    d->socket.flush();  //this is supposed to get the data on the wire faster

    if (d->socket.state() != QAbstractSocket::ConnectedState || !success) {
        /*qDebug() << "Write failed, will return -1! Socket error is"
          << d->socket.error() << ", Socket state is" << d->socket.state()
          << "Return value of waitForBytesWritten() is" << success;*/
        return -1;
    }

    return written;
}

ssize_t TCPSlaveBase::read(char *data, ssize_t len)
{
    if (d->usingSSL && (d->socket.mode() != QSslSocket::SslClientMode)) {
        d->clearSslMetaData();
        //qDebug() << "lost SSL connection.";
        return -1;
    }

    if (!d->socket.bytesAvailable()) {
        const int timeout = d->isBlocking ? -1 : (readTimeout() * 1000);
        d->socket.waitForReadyRead(timeout);
    }
#if 0
    // Do not do this because its only benefit is to cause a nasty side effect
    // upstream in Qt. See BR# 260769.
    else if (d->socket.mode() != QSslSocket::SslClientMode ||
             QNetworkProxy::applicationProxy().type() == QNetworkProxy::NoProxy) {
        // we only do this when it doesn't trigger Qt socket bugs. When it doesn't break anything
        // it seems to help performance.
        d->socket.waitForReadyRead(0);
    }
#endif
    return d->socket.read(data, len);
}

ssize_t TCPSlaveBase::readLine(char *data, ssize_t len)
{
    if (d->usingSSL && (d->socket.mode() != QSslSocket::SslClientMode)) {
        d->clearSslMetaData();
        //qDebug() << "lost SSL connection.";
        return -1;
    }

    const int timeout = (d->isBlocking ? -1 : (readTimeout() * 1000));
    ssize_t readTotal = 0;
    do {
        if (!d->socket.bytesAvailable()) {
            d->socket.waitForReadyRead(timeout);
        }
        ssize_t readStep = d->socket.readLine(&data[readTotal], len - readTotal);
        if (readStep == -1 || (readStep == 0 && d->socket.state() != QAbstractSocket::ConnectedState)) {
            return -1;
        }
        readTotal += readStep;
    } while (readTotal == 0 || data[readTotal - 1] != '\n');

    return readTotal;
}

bool TCPSlaveBase::connectToHost(const QString &/*protocol*/,
                                 const QString &host,
                                 quint16 port)
{
    QString errorString;
    const int errCode = connectToHost(host, port, &errorString);
    if (errCode == 0) {
        return true;
    }

    error(errCode, errorString);
    return false;
}

int TCPSlaveBase::connectToHost(const QString &host, quint16 port, QString *errorString)
{
    d->clearSslMetaData(); //We have separate connection and SSL setup phases

    if (errorString) {
        errorString->clear();  // clear prior error messages.
    }

    d->socket.setPeerVerifyName(host); // Used for ssl certificate verification (SNI)

    //  - leaving SSL - warn before we even connect
    //### see if it makes sense to move this into the HTTP ioslave which is the only
    //    user.
    if (metaData(QStringLiteral("main_frame_request")) == QLatin1String("TRUE")  //### this looks *really* unreliable
            && metaData(QStringLiteral("ssl_activate_warnings")) == QLatin1String("TRUE")
            && metaData(QStringLiteral("ssl_was_in_use")) == QLatin1String("TRUE")
            && !d->autoSSL) {
        if (d->sslSettings.warnOnLeave()) {
            int result = messageBox(i18n("You are about to leave secure "
                                         "mode. Transmissions will no "
                                         "longer be encrypted.\nThis "
                                         "means that a third party could "
                                         "observe your data in transit."),
                                    WarningContinueCancel,
                                    i18n("Security Information"),
                                    i18n("C&ontinue Loading"), QString(),
                                    QStringLiteral("WarnOnLeaveSSLMode"));

            if (result == SlaveBase::Cancel) {
                if (errorString) {
                    *errorString = host;
                }
                return ERR_USER_CANCELED;
            }
        }
    }

    const int timeout = (connectTimeout() * 1000); // 20 sec timeout value

    disconnectFromHost();  //Reset some state, even if we are already disconnected
    d->host = host;

    d->socket.connectToHost(host, port);
    /*const bool connectOk = */d->socket.waitForConnected(timeout > -1 ? timeout : -1);

    /*qDebug() << "Socket: state=" << d->socket.state()
        << ", error=" << d->socket.error()
        << ", connected?" << connectOk;*/

    if (d->socket.state() != QAbstractSocket::ConnectedState) {
        if (errorString) {
            *errorString = host + QLatin1String(": ") + d->socket.errorString();
        }
#if (QT_VERSION < QT_VERSION_CHECK(5, 15, 0))
        switch (d->socket.error()) {
#else
        switch (d->socket.socketError()) {
#endif
        case QAbstractSocket::UnsupportedSocketOperationError:
            return ERR_UNSUPPORTED_ACTION;
        case QAbstractSocket::RemoteHostClosedError:
            return ERR_CONNECTION_BROKEN;
        case QAbstractSocket::SocketTimeoutError:
            return ERR_SERVER_TIMEOUT;
        case QAbstractSocket::HostNotFoundError:
            return ERR_UNKNOWN_HOST;
        default:
            return ERR_CANNOT_CONNECT;
        }
    }

    //### check for proxyAuthenticationRequiredError

    d->ip = d->socket.peerAddress().toString();
    d->port = d->socket.peerPort();

    if (d->autoSSL) {
        const SslResult res = d->startTLSInternal(QSsl::SecureProtocols, timeout);

        if (res & ResultFailed) {
            if (errorString) {
                *errorString = i18nc("%1 is a host name", "%1: SSL negotiation failed", host);
            }
            return ERR_CANNOT_CONNECT;
        }
    }
    return 0;
}

void TCPSlaveBase::disconnectFromHost()
{
    //qDebug();
    d->host.clear();
    d->ip.clear();
    d->usingSSL = false;

    if (d->socket.state() == QAbstractSocket::UnconnectedState) {
        // discard incoming data - the remote host might have disconnected us in the meantime
        // but the visible effect of disconnectFromHost() should stay the same.
        d->socket.close();
        return;
    }

    //### maybe save a session for reuse on SSL shutdown if and when QSslSocket
    //    does that. QCA::TLS can do it apparently but that is not enough if
    //    we want to present that as KDE API. Not a big loss in any case.
    d->socket.disconnectFromHost();
    if (d->socket.state() != QAbstractSocket::UnconnectedState) {
        d->socket.waitForDisconnected(-1);    // wait for unsent data to be sent
    }
    d->socket.close(); //whatever that means on a socket
}

bool TCPSlaveBase::isAutoSsl() const
{
    return d->autoSSL;
}

bool TCPSlaveBase::isUsingSsl() const
{
    return d->usingSSL;
}

quint16 TCPSlaveBase::port() const
{
    return d->port;
}

bool TCPSlaveBase::atEnd() const
{
    return d->socket.atEnd();
}

bool TCPSlaveBase::startSsl()
{
    if (d->usingSSL) {
        return false;
    }
    return d->startTLSInternal(QSsl::SecureProtocols) & ResultOk;
}

TCPSlaveBase::SslResult TCPSlaveBase::TcpSlaveBasePrivate::startTLSInternal(QSsl::SslProtocol sslVersion,
                                                                            int waitForEncryptedTimeout)
{
    q->selectClientCertificate();

    //setMetaData("ssl_session_id", d->kssl->session()->toString());
    //### we don't support session reuse for now...
    usingSSL = true;

    // Set the SSL protocol version to use...
    socket.setProtocol(sslVersion);

    /* Usually ignoreSslErrors() would be called in the slot invoked by the sslErrors()
       signal but that would mess up the flow of control. We will check for errors
       anyway to decide if we want to continue connecting. Otherwise ignoreSslErrors()
       before connecting would be very insecure. */
    socket.ignoreSslErrors();
    socket.startClientEncryption();
    const bool encryptionStarted = socket.waitForEncrypted(waitForEncryptedTimeout);

    //Set metadata, among other things for the "SSL Details" dialog
    QSslCipher cipher = socket.sessionCipher();

    if (!encryptionStarted || socket.mode() != QSslSocket::SslClientMode
            || cipher.isNull() || cipher.usedBits() == 0 || socket.peerCertificateChain().isEmpty()) {
        usingSSL = false;
        clearSslMetaData();
        /*qDebug() << "Initial SSL handshake failed. encryptionStarted is"
          << encryptionStarted << ", cipher.isNull() is" << cipher.isNull()
          << ", cipher.usedBits() is" << cipher.usedBits()
          << ", length of certificate chain is" << socket.peerCertificateChain().count()
          << ", the socket says:" << socket.errorString()
          << "and the list of SSL errors contains"
          << socket.sslErrors().count() << "items.";*/
        /*for (const QSslError &sslError : socket.sslErrors()) {
          qDebug() << "SSL ERROR: (" << sslError.error() << ")" << sslError.errorString();
          }*/
        return ResultFailed | ResultFailedEarly;
    }

    /*qDebug() << "Cipher info - "
      << " advertised SSL protocol version" << socket.protocol()
      << " negotiated SSL protocol version" << socket.sessionProtocol()
      << " authenticationMethod:" << cipher.authenticationMethod()
      << " encryptionMethod:" << cipher.encryptionMethod()
      << " keyExchangeMethod:" << cipher.keyExchangeMethod()
      << " name:" << cipher.name()
      << " supportedBits:" << cipher.supportedBits()
      << " usedBits:" << cipher.usedBits();*/

#if (QT_VERSION < QT_VERSION_CHECK(5, 15, 0))
    sslErrors = socket.sslErrors();
#else
    sslErrors = socket.sslHandshakeErrors();
#endif

    // TODO: review / rewrite / remove the comment
    // The app side needs the metadata now for the SSL error dialog (if any) but
    // the same metadata will be needed later, too. When "later" arrives the slave
    // may actually be connected to a different application that doesn't know
    // the metadata the slave sent to the previous application.
    // The quite important SSL indicator icon in Konqi's URL bar relies on metadata
    // from here, for example. And Konqi will be the second application to connect
    // to the slave.
    // Therefore we choose to have our metadata and send it, too :)
    setSslMetaData();
    q->sendAndKeepMetaData();

    SslResult rc = q->verifyServerCertificate();
    if (rc & ResultFailed) {
        usingSSL = false;
        clearSslMetaData();
        //qDebug() << "server certificate verification failed.";
        socket.disconnectFromHost();     //Make the connection fail (cf. ignoreSslErrors())
        return ResultFailed;
    } else if (rc & ResultOverridden) {
        //qDebug() << "server certificate verification failed but continuing at user's request.";
    }

    //"warn" when starting SSL/TLS
    if (q->metaData(QStringLiteral("ssl_activate_warnings")) == QLatin1String("TRUE")
            && q->metaData(QStringLiteral("ssl_was_in_use")) == QLatin1String("FALSE")
            && sslSettings.warnOnEnter()) {

        int msgResult = q->messageBox(i18n("You are about to enter secure mode. "
                                           "All transmissions will be encrypted "
                                           "unless otherwise noted.\nThis means "
                                           "that no third party will be able to "
                                           "easily observe your data in transit."),
                                      WarningYesNo,
                                      i18n("Security Information"),
                                      i18n("Display SSL &Information"),
                                      i18n("C&onnect"),
                                      QStringLiteral("WarnOnEnterSSLMode"));
        if (msgResult == SlaveBase::Yes) {
            q->messageBox(SSLMessageBox /*==the SSL info dialog*/, host);
        }
    }

    return rc;
}

void TCPSlaveBase::selectClientCertificate()
{
#if 0 //hehe
    QString certname;   // the cert to use this session
    bool send = false, prompt = false, save = false, forcePrompt = false;
    KSSLCertificateHome::KSSLAuthAction aa;

    setMetaData("ssl_using_client_cert", "FALSE"); // we change this if needed

    if (metaData("ssl_no_client_cert") == "TRUE") {
        return;
    }
    forcePrompt = (metaData("ssl_force_cert_prompt") == "TRUE");

    // Delete the old cert since we're certainly done with it now
    if (d->pkcs) {
        delete d->pkcs;
        d->pkcs = NULL;
    }

    if (!d->kssl) {
        return;
    }

    // Look for a general certificate
    if (!forcePrompt) {
        certname = KSSLCertificateHome::getDefaultCertificateName(&aa);
        switch (aa) {
        case KSSLCertificateHome::AuthSend:
            send = true; prompt = false;
            break;
        case KSSLCertificateHome::AuthDont:
            send = false; prompt = false;
            certname.clear();
            break;
        case KSSLCertificateHome::AuthPrompt:
            send = false; prompt = true;
            break;
        default:
            break;
        }
    }

    // Look for a certificate on a per-host basis as an override
    QString tmpcn = KSSLCertificateHome::getDefaultCertificateName(d->host, &aa);
    if (aa != KSSLCertificateHome::AuthNone) {   // we must override
        switch (aa) {
        case KSSLCertificateHome::AuthSend:
            send = true;
            prompt = false;
            certname = tmpcn;
            break;
        case KSSLCertificateHome::AuthDont:
            send = false;
            prompt = false;
            certname.clear();
            break;
        case KSSLCertificateHome::AuthPrompt:
            send = false;
            prompt = true;
            certname = tmpcn;
            break;
        default:
            break;
        }
    }

    // Finally, we allow the application to override anything.
    if (hasMetaData("ssl_demand_certificate")) {
        certname = metaData("ssl_demand_certificate");
        if (!certname.isEmpty()) {
            forcePrompt = false;
            prompt = false;
            send = true;
        }
    }

    if (certname.isEmpty() && !prompt && !forcePrompt) {
        return;
    }

    // Ok, we're supposed to prompt the user....
    if (prompt || forcePrompt) {
        QStringList certs = KSSLCertificateHome::getCertificateList();

        QStringList::const_iterator it = certs.begin();
        while (it != certs.end()) {
            KSSLPKCS12 *pkcs = KSSLCertificateHome::getCertificateByName(*it);
            if (pkcs && (!pkcs->getCertificate() ||
                         !pkcs->getCertificate()->x509V3Extensions().certTypeSSLClient())) {
                it = certs.erase(it);
            } else {
                ++it;
            }
            delete pkcs;
        }

        if (certs.isEmpty()) {
            return;    // we had nothing else, and prompt failed
        }

        QDBusConnectionInterface *bus = QDBusConnection::sessionBus().interface();
        if (!bus->isServiceRegistered("org.kde.kio.uiserver")) {
            bus->startService("org.kde.kuiserver");
        }

        QDBusInterface uis("org.kde.kio.uiserver", "/UIServer", "org.kde.KIO.UIServer");

        QDBusMessage retVal = uis.call("showSSLCertDialog", d->host, certs, metaData("window-id").toLongLong());
        if (retVal.type() == QDBusMessage::ReplyMessage) {
            if (retVal.arguments().at(0).toBool()) {
                send = retVal.arguments().at(1).toBool();
                save = retVal.arguments().at(2).toBool();
                certname = retVal.arguments().at(3).toString();
            }
        }
    }

    // The user may have said to not send the certificate,
    // but to save the choice
    if (!send) {
        if (save) {
            KSSLCertificateHome::setDefaultCertificate(certname, d->host,
                    false, false);
        }
        return;
    }

    // We're almost committed.  If we can read the cert, we'll send it now.
    KSSLPKCS12 *pkcs = KSSLCertificateHome::getCertificateByName(certname);
    if (!pkcs && KSSLCertificateHome::hasCertificateByName(certname)) {           // We need the password
        KIO::AuthInfo ai;
        bool first = true;
        do {
            ai.prompt = i18n("Enter the certificate password:");
            ai.caption = i18n("SSL Certificate Password");
            ai.url.setScheme("kssl");
            ai.url.setHost(certname);
            ai.username = certname;
            ai.keepPassword = true;

            bool showprompt;
            if (first) {
                showprompt = !checkCachedAuthentication(ai);
            } else {
                showprompt = true;
            }
            if (showprompt) {
                if (!openPasswordDialog(ai, first ? QString() :
                                        i18n("Unable to open the certificate. Try a new password?"))) {
                    break;
                }
            }

            first = false;
            pkcs = KSSLCertificateHome::getCertificateByName(certname, ai.password);
        } while (!pkcs);

    }

    // If we could open the certificate, let's send it
    if (pkcs) {
        if (!d->kssl->setClientCertificate(pkcs)) {
            messageBox(Information, i18n("The procedure to set the "
                                         "client certificate for the session "
                                         "failed."), i18n("SSL"));
            delete pkcs;  // we don't need this anymore
            pkcs = 0L;
        } else {
            //qDebug() << "Client SSL certificate is being used.";
            setMetaData("ssl_using_client_cert", "TRUE");
            if (save) {
                KSSLCertificateHome::setDefaultCertificate(certname, d->host,
                        true, false);
            }
        }
        d->pkcs = pkcs;
    }
#endif
}

TCPSlaveBase::SslResult TCPSlaveBase::verifyServerCertificate()
{
    d->sslNoUi = hasMetaData(QStringLiteral("ssl_no_ui")) && (metaData(QStringLiteral("ssl_no_ui")) != QLatin1String("FALSE"));

    if (d->sslErrors.isEmpty()) {
        return ResultOk;
    } else if (d->sslNoUi) {
        return ResultFailed;
    }

    const QList<QSslError> fatalErrors = KSslCertificateManager::nonIgnorableErrors(d->sslErrors);
    if (!fatalErrors.isEmpty()) {
        //TODO message "sorry, fatal error, you can't override it"
        return ResultFailed;
    }
    QList<QSslCertificate> peerCertificationChain = d->socket.peerCertificateChain();
    KSslCertificateManager *const cm = KSslCertificateManager::self();
    KSslCertificateRule rule = cm->rule(peerCertificationChain.first(), d->host);

    // remove previously seen and acknowledged errors
    const QList<QSslError> remainingErrors = rule.filterErrors(d->sslErrors);
    if (remainingErrors.isEmpty()) {
        //qDebug() << "Error list empty after removing errors to be ignored. Continuing.";
        return ResultOk | ResultOverridden;
    }

    //### We don't ask to permanently reject the certificate

    QString message = i18n("The server failed the authenticity check (%1).\n\n", d->host);
    for (const QSslError &err : qAsConst(d->sslErrors)) {
        message += err.errorString() + QLatin1Char('\n');
    }
    message = message.trimmed();

    int msgResult;
    QDateTime ruleExpiry = QDateTime::currentDateTime();
    do {
        msgResult = messageBox(WarningYesNoCancel, message,
                               i18n("Server Authentication"),
                               i18n("&Details"), i18n("Co&ntinue"));
        switch (msgResult) {
        case SlaveBase::Yes:
            //Details was chosen- show the certificate and error details
            messageBox(SSLMessageBox /*the SSL info dialog*/, d->host);
            break;
        case SlaveBase::No: {
                        //fall through on SlaveBase::No
            const int result = messageBox(WarningYesNoCancel,
                                    i18n("Would you like to accept this "
                                        "certificate forever without "
                                        "being prompted?"),
                                    i18n("Server Authentication"),
                                    i18n("&Forever"),
                                    i18n("&Current Session only"));
            if (result == SlaveBase::Yes) {
                //accept forever ("for a very long time")
                ruleExpiry = ruleExpiry.addYears(1000);
            } else if (result == SlaveBase::No) {
                //accept "for a short time", half an hour.
                ruleExpiry = ruleExpiry.addSecs(30*60);
            } else {
                msgResult = SlaveBase::Yes;
            }
            break;
        }
        case SlaveBase::Cancel:
            return ResultFailed;
        default:
            qCWarning(KIO_CORE) << "Unexpected MessageBox response received:" << msgResult;
            return ResultFailed;
        }
    } while (msgResult == SlaveBase::Yes);

    //TODO special cases for wildcard domain name in the certificate!
    //rule = KSslCertificateRule(d->socket.peerCertificateChain().first(), whatever);

    rule.setExpiryDateTime(ruleExpiry);
    rule.setIgnoredErrors(d->sslErrors);
    cm->setRule(rule);

    return ResultOk | ResultOverridden;
#if 0 //### need to do something like the old code about the main and subframe stuff
    //qDebug() << "SSL HTTP frame the parent? " << metaData("main_frame_request");
    if (!hasMetaData("main_frame_request") || metaData("main_frame_request") == "TRUE") {
        // Since we're the parent, we need to teach the child.
        setMetaData("ssl_parent_ip", d->ip);
        setMetaData("ssl_parent_cert", pc.toString());
        //  - Read from cache and see if there is a policy for this
        KSSLCertificateCache::KSSLCertificatePolicy cp =
            d->certCache->getPolicyByCertificate(pc);

        //  - validation code
        if (ksv != KSSLCertificate::Ok) {
            if (d->sslNoUi) {
                return -1;
            }

            if (cp == KSSLCertificateCache::Unknown ||
                    cp == KSSLCertificateCache::Ambiguous) {
                cp = KSSLCertificateCache::Prompt;
            } else {
                // A policy was already set so let's honor that.
                permacache = d->certCache->isPermanent(pc);
            }

            if (!_IPmatchesCN && cp == KSSLCertificateCache::Accept) {
                cp = KSSLCertificateCache::Prompt;
                //            ksv = KSSLCertificate::Ok;
            }

            ////// SNIP SNIP //////////

            //  - cache the results
            d->certCache->addCertificate(pc, cp, permacache);
            if (doAddHost) {
                d->certCache->addHost(pc, d->host);
            }
        } else {    // Child frame
            //  - Read from cache and see if there is a policy for this
            KSSLCertificateCache::KSSLCertificatePolicy cp =
                d->certCache->getPolicyByCertificate(pc);
            isChild = true;

            // Check the cert and IP to make sure they're the same
            // as the parent frame
            bool certAndIPTheSame = (d->ip == metaData("ssl_parent_ip") &&
                                     pc.toString() == metaData("ssl_parent_cert"));

            if (ksv == KSSLCertificate::Ok) {
                if (certAndIPTheSame) {       // success
                    rc = 1;
                    setMetaData("ssl_action", "accept");
                } else {
                    /*
                       if (d->sslNoUi) {
                       return -1;
                       }
                       result = messageBox(WarningYesNo,
                       i18n("The certificate is valid but does not appear to have been assigned to this server.  Do you wish to continue loading?"),
                       i18n("Server Authentication"));
                       if (result == SlaveBase::Yes) {     // success
                       rc = 1;
                       setMetaData("ssl_action", "accept");
                       } else {    // fail
                       rc = -1;
                       setMetaData("ssl_action", "reject");
                       }
                     */
                    setMetaData("ssl_action", "accept");
                    rc = 1;   // Let's accept this now.  It's bad, but at least the user
                    // will see potential attacks in KDE3 with the pseudo-lock
                    // icon on the toolbar, and can investigate with the RMB
                }
            } else {
                if (d->sslNoUi) {
                    return -1;
                }

                if (cp == KSSLCertificateCache::Accept) {
                    if (certAndIPTheSame) {    // success
                        rc = 1;
                        setMetaData("ssl_action", "accept");
                    } else {   // fail
                        result = messageBox(WarningYesNo,
                                            i18n("You have indicated that you wish to accept this certificate, but it is not issued to the server who is presenting it. Do you wish to continue loading?"),
                                            i18n("Server Authentication"));
                        if (result == SlaveBase::Yes) {
                            rc = 1;
                            setMetaData("ssl_action", "accept");
                            d->certCache->addHost(pc, d->host);
                        } else {
                            rc = -1;
                            setMetaData("ssl_action", "reject");
                        }
                    }
                } else if (cp == KSSLCertificateCache::Reject) {      // fail
                    messageBox(Information, i18n("SSL certificate is being rejected as requested. You can disable this in the KDE System Settings."),
                               i18n("Server Authentication"));
                    rc = -1;
                    setMetaData("ssl_action", "reject");
                } else {

                    //////// SNIP SNIP //////////

                    return rc;
                }
            }
        }
    }
#endif //#if 0
}

bool TCPSlaveBase::isConnected() const
{
    // QSslSocket::isValid() is shady...
    return d->socket.state() == QAbstractSocket::ConnectedState;
}

bool TCPSlaveBase::waitForResponse(int t)
{
    if (d->socket.bytesAvailable()) {
        return true;
    }
    return d->socket.waitForReadyRead(t * 1000);
}

void TCPSlaveBase::setBlocking(bool b)
{
    if (!b) {
        qCWarning(KIO_CORE) << "Caller requested non-blocking mode, but that doesn't work";
        return;
    }
    d->isBlocking = b;
}

void TCPSlaveBase::virtual_hook(int id, void *data)
{
    if (id == SlaveBase::AppConnectionMade) {
        d->sendSslMetaData();
    } else {
        SlaveBase::virtual_hook(id, data);
    }
}
