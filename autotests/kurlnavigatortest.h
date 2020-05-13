/***************************************************************************
 *   Copyright (C) 2008 by Peter Penz <peter.penz@gmx.at>                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#ifndef URLNAVIGATORTEST_H
#define URLNAVIGATORTEST_H

#include <QObject>
#include <kiofilewidgets_export.h>
class KUrlNavigator;

class KUrlNavigatorTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    void testHistorySizeAndIndex();
    void testGoBack();
    void testGoForward();
    void testHistoryInsert();

    void bug251553_goUpFromArchive();

    void testUrlParsing_data();
    void testUrlParsing();

#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(4, 5)
    void testButtonUrl_data();
    void testButtonUrl();
#endif
    void testButtonText();

    void testInitWithRedundantPathSeparators();


private:
    KUrlNavigator *m_navigator;
};

#endif
