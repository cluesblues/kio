/* This file is part of the KDE project
 *
 * Copyright (C) 2007, 2008, 2010 Andreas Hartmetz <ahartmetz@gmail.com>
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

#ifndef _INCLUDE_KSSLCERTIFICATEMANAGER_H
#define _INCLUDE_KSSLCERTIFICATEMANAGER_H

#include "ktcpsocket.h"

#include <QSslCertificate>
#include <QSslError>
#include <QString>
#include <QStringList>
#include <QDate>

class QSslCertificate;
class KSslCertificateRulePrivate;
class KSslCertificateManagerPrivate;

//### document this... :/
class KIOCORE_EXPORT KSslCertificateRule
{
public:
    KSslCertificateRule(const QSslCertificate &cert = QSslCertificate(),
                        const QString &hostName = QString());
    KSslCertificateRule(const KSslCertificateRule &other);
    ~KSslCertificateRule();
    KSslCertificateRule &operator=(const KSslCertificateRule &other);

    QSslCertificate certificate() const;
    QString hostName() const;
    void setExpiryDateTime(const QDateTime &dateTime);
    QDateTime expiryDateTime() const;
    void setRejected(bool rejected);
    bool isRejected() const;
    bool isErrorIgnored(KSslError::Error error) const;
    void setIgnoredErrors(const QList<KSslError::Error> &errors);
    void setIgnoredErrors(const QList<KSslError> &errors);
    QList<KSslError::Error> ignoredErrors() const;
    QList<KSslError::Error> filterErrors(const QList<KSslError::Error> &errors) const;
    QList<KSslError> filterErrors(const QList<KSslError> &errors) const;
private:
    KSslCertificateRulePrivate *const d;
};

//### document this too... :/
/** Certificate manager. */
class KIOCORE_EXPORT KSslCertificateManager
{
public:
    static KSslCertificateManager *self();
    void setRule(const KSslCertificateRule &rule);
    void clearRule(const KSslCertificateRule &rule);
    void clearRule(const QSslCertificate &cert, const QString &hostName);
    KSslCertificateRule rule(const QSslCertificate &cert, const QString &hostName) const;

    // use caCertificates() instead
#ifndef KIOCORE_NO_DEPRECATED
    KIOCORE_DEPRECATED QList<QSslCertificate> rootCertificates() const
    {
        return caCertificates();
    }
#endif

    QList<QSslCertificate> caCertificates() const;

    /** @deprecated since 5.64, use the corresponding QSslError variant. */
    static KIOCORE_DEPRECATED QList<KSslError> nonIgnorableErrors(const QList<KSslError> &);
    /** @deprecated since 5.64, use the corresponding QSslError variant. */
    static KIOCORE_DEPRECATED QList<KSslError::Error> nonIgnorableErrors(const QList<KSslError::Error> &);
    /**
     * Returns the subset of @p errors that cannot be ignored, ie. that is considered fatal.
     * @since 5.64
     */
    static QList<QSslError> nonIgnorableErrors(const QList<QSslError> &errors);

private:
    friend class KSslCertificateManagerContainer;
    friend class KSslCertificateManagerPrivate;
    KSslCertificateManager();
    ~KSslCertificateManager();

    KSslCertificateManagerPrivate *const d;
};

#endif
