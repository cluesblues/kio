/*  This file is part of the KDE project
    Copyright (C) 2017 Renato Araujo Oliveira Filho <renato.araujo@kdab.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2
    as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301, USA.

*/

#include <QObject>
#include <QStandardPaths>
#include <QFile>
#include <QTemporaryDir>

#include <kfileplacesview.h>
#include <kfileplacesmodel.h>
#include <KConfig>
#include <KConfigGroup>
#include <KProtocolInfo>

#include <QTest>
#include <QSignalSpy>

static QString bookmarksFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/user-places.xbel";
}

class KFilePlacesViewTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    void testUrlChanged_data();
    void testUrlChanged();
private:
    QTemporaryDir m_tmpHome;
};

void KFilePlacesViewTest::initTestCase()
{
    QVERIFY(m_tmpHome.isValid());
    qputenv("HOME", m_tmpHome.path().toUtf8());
    qputenv("KDE_FORK_SLAVES", "yes"); // to avoid a runtime dependency on klauncher
    qputenv("KDE_FULL_SESSION", "1"); // attempt to enable recentlyused:/ if present, so we only need to test for isKnownProtocol below
    QStandardPaths::setTestModeEnabled(true);

    cleanupTestCase();

    KConfig config(QStringLiteral("baloofilerc"));
    KConfigGroup basicSettings = config.group("Basic Settings");
    basicSettings.writeEntry("Indexing-Enabled", true);
    config.sync();

    qRegisterMetaType<QModelIndex>();

    // Debug code, to help understanding the actual test
    KFilePlacesModel model;
    for (int row = 0; row < model.rowCount(); ++row) {
        const QModelIndex index = model.index(row, 0);
        qDebug() << model.url(index);
    }
}

void KFilePlacesViewTest::cleanupTestCase()
{
    QFile::remove(bookmarksFile());
}

void KFilePlacesViewTest::testUrlChanged_data()
{
    QTest::addColumn<int>("row");
    QTest::addColumn<QString>("expectedUrl");

    int idx = 3; // skip home, trash, remote
    if (KProtocolInfo::isKnownProtocol(QStringLiteral("recentlyused"))) {
        QTest::newRow("Recent Files") << idx++ << QStringLiteral("recentlyused:/files");
        QTest::newRow("Recent Locations") << idx++ << QStringLiteral("recentlyused:/locations");
    } else {
        QTest::newRow("Modified Today") << idx++ << QStringLiteral("timeline:/today");
        ++idx; // Modified Yesterday gets turned into "timeline:/2020-06/2020-06-05"
    }

    // search
    QTest::newRow("Documents") << idx++ << QStringLiteral("baloosearch:/documents");
    QTest::newRow("Images") << idx++ << QStringLiteral("baloosearch:/images");
    QTest::newRow("Audio Files") << idx++ << QStringLiteral("baloosearch:/audio");
    QTest::newRow("Videos") << idx++ << QStringLiteral("baloosearch:/videos");
}

void KFilePlacesViewTest::testUrlChanged()
{
    QFETCH(int, row);
    QFETCH(QString, expectedUrl);

    KFilePlacesView pv;
    pv.show();
    pv.activateWindow();
    pv.setModel(new KFilePlacesModel());
    QVERIFY(QTest::qWaitForWindowActive(&pv));

    QSignalSpy urlChangedSpy(&pv, &KFilePlacesView::urlChanged);
    const QModelIndex targetIndex = pv.model()->index(row, 0);
    pv.scrollTo(targetIndex);
    pv.clicked(targetIndex);
    QTRY_COMPARE(urlChangedSpy.count(), 1);
    const QList<QVariant> args = urlChangedSpy.takeFirst();
    QCOMPARE(args.at(0).toUrl().toString(), expectedUrl);
}

QTEST_MAIN(KFilePlacesViewTest)

#include "kfileplacesviewtest.moc"
