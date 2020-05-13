/* This file is part of the KDE project
 *
 * Copyright (C) 2000,2001 George Staikos <staikos@kde.org>
 * Copyright (C) 2000 Malte Starostik <malte@kde.org>
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

#include "ksslinfodialog.h"
#include "ui_sslinfo.h"
#include "ksslcertificatebox.h"
#include "ksslerror_p.h"


#include <QDialogButtonBox>

#include <QSslCertificate>

#include <klocalizedstring.h>
#include <kiconloader.h> // BarIcon

#include "ktcpsocket.h"

class Q_DECL_HIDDEN KSslInfoDialog::KSslInfoDialogPrivate
{
public:
    QList<QSslCertificate> certificateChain;
    QList<QList<QSslError::SslError>> certificateErrors;

    bool isMainPartEncrypted;
    bool auxPartsEncrypted;

    Ui::SslInfo ui;
    KSslCertificateBox *subject;
    KSslCertificateBox *issuer;
};

KSslInfoDialog::KSslInfoDialog(QWidget *parent)
    : QDialog(parent),
      d(new KSslInfoDialogPrivate)
{
    setWindowTitle(i18n("KDE SSL Information"));
    setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout *layout = new QVBoxLayout;
    setLayout(layout);

    QWidget *mainWidget = new QWidget(this);
    d->ui.setupUi(mainWidget);
    layout->addWidget(mainWidget);

    d->subject = new KSslCertificateBox(d->ui.certParties);
    d->issuer = new KSslCertificateBox(d->ui.certParties);
    d->ui.certParties->addTab(d->subject, i18nc("The receiver of the SSL certificate", "Subject"));
    d->ui.certParties->addTab(d->issuer, i18nc("The authority that issued the SSL certificate", "Issuer"));

    d->isMainPartEncrypted = true;
    d->auxPartsEncrypted = true;
    updateWhichPartsEncrypted();

    QDialogButtonBox *buttonBox = new QDialogButtonBox(this);
    buttonBox->setStandardButtons(QDialogButtonBox::Close);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);
}

KSslInfoDialog::~KSslInfoDialog()
{
    delete d;
}

void KSslInfoDialog::setMainPartEncrypted(bool mainEncrypted)
{
    d->isMainPartEncrypted = mainEncrypted;
    updateWhichPartsEncrypted();
}

void KSslInfoDialog::setAuxiliaryPartsEncrypted(bool auxEncrypted)
{
    d->auxPartsEncrypted = auxEncrypted;
    updateWhichPartsEncrypted();
}

void KSslInfoDialog::updateWhichPartsEncrypted()
{
    if (d->isMainPartEncrypted) {
        if (d->auxPartsEncrypted) {
            d->ui.encryptionIndicator->setPixmap(QIcon::fromTheme(QStringLiteral("security-high"))
                .pixmap(KIconLoader::SizeSmallMedium));
            d->ui.explanation->setText(i18n("Current connection is secured with SSL."));
        } else {
            d->ui.encryptionIndicator->setPixmap(QIcon::fromTheme(QStringLiteral("security-medium"))
                .pixmap(KIconLoader::SizeSmallMedium));
            d->ui.explanation->setText(i18n("The main part of this document is secured "
                                            "with SSL, but some parts are not."));
        }
    } else {
        if (d->auxPartsEncrypted) {
            d->ui.encryptionIndicator->setPixmap(QIcon::fromTheme(QStringLiteral("security-medium"))
                .pixmap(KIconLoader::SizeSmallMedium));
            d->ui.explanation->setText(i18n("Some of this document is secured with SSL, "
                                            "but the main part is not."));
        } else {
            d->ui.encryptionIndicator->setPixmap(QIcon::fromTheme(QStringLiteral("security-low"))
                .pixmap(KIconLoader::SizeSmallMedium));
            d->ui.explanation->setText(i18n("Current connection is not secured with SSL."));
        }
    }
}

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 64)
void KSslInfoDialog::setSslInfo(const QList<QSslCertificate> &certificateChain,
                                const QString &ip, const QString &host,
                                const QString &sslProtocol, const QString &cipher,
                                int usedBits, int bits,
                                const QList<QList<KSslError::Error> > &validationErrors)
{
    QList<QList<QSslError::SslError>> qValidationErrors;
    qValidationErrors.reserve(validationErrors.size());
    for (const auto &l : validationErrors) {
        QList<QSslError::SslError> qErrors;
        qErrors.reserve(l.size());
        for (const KSslError::Error e : l) {
            qErrors.push_back(KSslErrorPrivate::errorFromKSslError(e));
        }
        qValidationErrors.push_back(qErrors);
    }
    setSslInfo(certificateChain, ip, host, sslProtocol, cipher, usedBits, bits, qValidationErrors);
}
#endif

void KSslInfoDialog::setSslInfo(const QList<QSslCertificate> &certificateChain,
                                const QString &ip, const QString &host,
                                const QString &sslProtocol, const QString &cipher,
                                int usedBits, int bits,
                                const QList<QList<QSslError::SslError>> &validationErrors)
{

    d->certificateChain = certificateChain;
    d->certificateErrors = validationErrors;

    d->ui.certSelector->clear();
    for (const QSslCertificate &cert : certificateChain) {
        QString name;
        static const QSslCertificate::SubjectInfo si[] = {
            QSslCertificate::CommonName,
            QSslCertificate::Organization,
            QSslCertificate::OrganizationalUnitName
        };
        for (int j = 0; j < 3 && name.isEmpty(); j++)
        {
            name = cert.subjectInfo(si[j]).join(QLatin1String(", "));
        }
        d->ui.certSelector->addItem(name);
    }
    if (certificateChain.size() < 2) {
        d->ui.certSelector->setEnabled(false);
    }
    connect(d->ui.certSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &KSslInfoDialog::displayFromChain);
    if (d->certificateChain.isEmpty()) {
        d->certificateChain.append(QSslCertificate());
    }
    displayFromChain(0);

    d->ui.ip->setText(ip);
    d->ui.address->setText(host);
    d->ui.sslVersion->setText(sslProtocol);

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    const QStringList cipherInfo = cipher.split(QLatin1Char('\n'), QString::SkipEmptyParts);
#else
    const QStringList cipherInfo = cipher.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
#endif
    if (cipherInfo.size() >= 4) {
        d->ui.encryption->setText(i18nc("%1, using %2 bits of a %3 bit key", "%1, %2 %3", cipherInfo[0],
                                        i18ncp("Part of: %1, using %2 bits of a %3 bit key",
                                               "using %1 bit", "using %1 bits", usedBits),
                                        i18ncp("Part of: %1, using %2 bits of a %3 bit key",
                                               "of a %1 bit key", "of a %1 bit key", bits)));
        d->ui.details->setText(QStringLiteral("Auth = %1, Kx = %2, MAC = %3")
                               .arg(cipherInfo[1], cipherInfo[2],
                                    cipherInfo[3]));
    } else {
        d->ui.encryption->setText(QString());
        d->ui.details->setText(QString());
    }
}

void KSslInfoDialog::displayFromChain(int i)
{
    const QSslCertificate &cert = d->certificateChain[i];

    QString trusted;
    const QList<QSslError::SslError> errorsList = d->certificateErrors[i];
    if (!errorsList.isEmpty()) {
        trusted = i18nc("The certificate is not trusted", "NO, there were errors:");
        for (QSslError::SslError e : errorsList) {
            QSslError classError(e);
            trusted += QLatin1Char('\n') + classError.errorString();
        }
    } else {
        trusted = i18nc("The certificate is trusted", "Yes");
    }
    d->ui.trusted->setText(trusted);

    QString vp = i18nc("%1 is the effective date of the certificate, %2 is the expiry date", "%1 to %2",
                       cert.effectiveDate().toString(),
                       cert.expiryDate().toString());
    d->ui.validityPeriod->setText(vp);

    d->ui.serial->setText(QString::fromUtf8(cert.serialNumber()));
    d->ui.digest->setText(QString::fromUtf8(cert.digest().toHex()));
    d->ui.sha1Digest->setText(QString::fromUtf8(cert.digest(QCryptographicHash::Sha1).toHex()));

    d->subject->setCertificate(cert, KSslCertificateBox::Subject);
    d->issuer->setCertificate(cert, KSslCertificateBox::Issuer);
}

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 65)
//static
QList<QList<KSslError::Error> > KSslInfoDialog::errorsFromString(const QString &es)
{
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    const QStringList sl = es.split(QLatin1Char('\n'), QString::KeepEmptyParts);
#else
    const QStringList sl = es.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
#endif
    QList<QList<KSslError::Error> > ret;
    ret.reserve(sl.size());
    for (const QString &s : sl) {
        QList<KSslError::Error> certErrors;
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
        const QStringList sl2 = s.split(QLatin1Char('\t'), QString::SkipEmptyParts);
#else
        const QStringList sl2 = s.split(QLatin1Char('\t'), Qt::SkipEmptyParts);
#endif
        for (const QString &s2 : sl2) {
            bool didConvert;
            KSslError::Error error = KSslErrorPrivate::errorFromQSslError(static_cast<QSslError::SslError>(s2.toInt(&didConvert)));
            if (didConvert) {
                certErrors.append(error);
            }
        }
        ret.append(certErrors);
    }
    return ret;
}
#endif

//static
QList<QList<QSslError::SslError>> KSslInfoDialog::certificateErrorsFromString(const QString &errorsString)
{
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    const QStringList sl = errorsString.split(QLatin1Char('\n'), QString::KeepEmptyParts);
#else
    const QStringList sl = errorsString.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
#endif
    QList<QList<QSslError::SslError>> ret;
    ret.reserve(sl.size());
    for (const QString &s : sl) {
        QList<QSslError::SslError> certErrors;
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
        const QStringList sl2 = s.split(QLatin1Char('\t'), QString::SkipEmptyParts);
#else
        const QStringList sl2 = s.split(QLatin1Char('\t'), Qt::SkipEmptyParts);
#endif
        for (const QString &s2 : sl2) {
            bool didConvert;
            QSslError::SslError error = static_cast<QSslError::SslError>(s2.toInt(&didConvert));
            if (didConvert) {
                certErrors.append(error);
            }
        }
        ret.append(certErrors);
    }
    return ret;
}
