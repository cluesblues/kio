/*
 * Copyright (c) 2000 Malte Starostik <malte@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "searchprovider.h"

#include <krandom.h>
#include <kservicetypetrader.h>
#include <QStandardPaths>
#include <KIO/Global> // KIO::iconNameForUrl
#include <QFileInfo>
#include <KDesktopFile>
#include <KConfigGroup>

SearchProvider::SearchProvider(const QString &servicePath)
    : m_dirty(false)
{
    setDesktopEntryName(QFileInfo(servicePath).baseName());
    KDesktopFile parser(servicePath);
    setName(parser.readName());
    KConfigGroup group(parser.desktopGroup());
    setKeys(group.readEntry(QStringLiteral("Keys"), QStringList()));

    m_query = group.readEntry(QStringLiteral("Query"));
    m_charset = group.readEntry(QStringLiteral("Charset"));
    m_iconName = group.readEntry(QStringLiteral("Icon"));
    m_isHidden = group.readEntry(QStringLiteral("Hidden"), false);
}

SearchProvider::~SearchProvider()
{
}

void SearchProvider::setName(const QString &name)
{
    if (KUriFilterSearchProvider::name() == name) {
        return;
    }

    KUriFilterSearchProvider::setName(name);
}

void SearchProvider::setQuery(const QString &query)
{
    if (m_query == query) {
        return;
    }

    m_query = query;
}

void SearchProvider::setKeys(const QStringList &keys)
{
    if (KUriFilterSearchProvider::keys() == keys) {
        return;
    }

    KUriFilterSearchProvider::setKeys(keys);

    QString name = desktopEntryName();
    if (!name.isEmpty()) {
        return;
    }

    // New provider. Set the desktopEntryName.
    // Take the longest search shortcut as filename,
    // if such a file already exists, append a number and increase it
    // until the name is unique
    for (const QString &key : keys) {
        if (key.length() > name.length()) {
            name = key.toLower();
        }
    }

    const QString path = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1String("/kservices5/searchproviders/");
    bool firstRun = true;

    while (true)
    {
        QString check(name);

        if (!firstRun) {
            check += KRandom::randomString(4);
        }

        const QString located = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QLatin1String("kservices5/searchproviders/") + check + QLatin1String(".desktop"));
        if (located.isEmpty()) {
            name = check;
            break;
        } else if (located.startsWith(path)) {
            // If it's a deleted (hidden) entry, overwrite it
            if (KService(located).isDeleted()) {
                break;
            }
        }
        firstRun = false;
    }

    setDesktopEntryName(name);
}

void SearchProvider::setCharset(const QString &charset)
{
    if (m_charset == charset) {
        return;
    }

    m_charset = charset;
}

QString SearchProvider::iconName() const
{
    if (!m_iconName.isEmpty()) {
        return m_iconName;
    }

    return KIO::iconNameForUrl(QUrl(m_query));
}

void SearchProvider::setDirty(bool dirty)
{
    m_dirty = dirty;
}
