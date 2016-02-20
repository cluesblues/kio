// (c) Torben Weis 1998
// (c) David Faure 1998
/*
 * main.cpp for creating the konqueror kio kcm modules
 *
 *  Copyright (C) 2000,2001,2009 Alexander Neundorf <neundorf@kde.org>
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

// Qt
#include <QtCore/QFile>

// KDE
#include <kpluginfactory.h>

// Local
#include "kcookiesmain.h"
#include "netpref.h"
#include "smbrodlg.h"
#include "useragentdlg.h"
#include "kproxydlg.h"
#include "cache.h"

K_PLUGIN_FACTORY(KioConfigFactory,
        registerPlugin<UserAgentDlg>(QStringLiteral("useragent"));
        registerPlugin<SMBRoOptions>(QStringLiteral("smb"));
        registerPlugin<KIOPreferences>(QStringLiteral("netpref"));
        registerPlugin<KProxyDialog>(QStringLiteral("proxy"));
        registerPlugin<KCookiesMain>(QStringLiteral("cookie"));
        registerPlugin<CacheConfigModule>(QStringLiteral("cache"));
	)

#include "main.moc"
