/**
 * @file updater.cpp
 * Automatic updater that works with dengine.net. @ingroup base
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2012 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "updater.h"
#include "dd_version.h"
#include "dd_types.h"
#include "json.h"
#include "updater/updateavailabledialog.h"
#include "updater/updatersettings.h"
#include "updater/versioninfo.h"
#include <de/Time>
#include <de/Log>
#include <QStringList>
#include <QDateTime>
#include <QDesktopServices>
#include <QNetworkAccessManager>
#include <QSettings>
#include <QDebug>

static Updater* updater = 0;

/// @todo The platform ID should come from the Builder.
#if defined(WIN32)
#  define PLATFORM_ID       "win-x86"

#elif defined(MACOSX)
#  if defined(__64BIT__)
#    define PLATFORM_ID     "mac10_6-x86-x86_64"
#  else
#    define PLATFORM_ID     "mac10_4-x86-ppc"
#  endif

#else
#  if defined(__64BIT__)
#    define PLATFORM_ID     "linux-x86_64"
#  else
#    define PLATFORM_ID     "linux-x86"
#  endif
#endif

struct Updater::Instance
{
    Updater* self;
    QNetworkAccessManager* network;

    VersionInfo latestVersion;
    QString latestPackageUri;
    QString latestLogUri;

    Instance(Updater* up) : self(up)
    {
        network = new QNetworkAccessManager(self);
    }

    ~Instance()
    {
    }

    QString composeCheckUri()
    {
        UpdaterSettings st;
        QString uri = QString(DOOMSDAY_HOMEURL) + "/latestbuild?";
        uri += QString("platform=") + PLATFORM_ID;
        uri += (st.channel() == UpdaterSettings::Stable? "&stable" : "&unstable");
        uri += "&graph";

        LOG_DEBUG("Check URI: ") << uri;
        return uri;
    }

    void queryLatestVersion()
    {
        UpdaterSettings().setLastCheckTime(de::Time());
        network->get(QNetworkRequest(composeCheckUri()));
    }

    void handleReply(QNetworkReply* reply)
    {
        reply->deleteLater(); // make sure it gets deleted

        QVariant result = parseJSON(QString::fromUtf8(reply->readAll()));
        if(!result.isValid()) return;

        QVariantMap map = result.toMap();
        latestPackageUri = map["direct_download_uri"].toString();
        latestLogUri = map["release_changeloguri"].toString();

        latestVersion = VersionInfo(map["version"].toString(), map["build_uniqueid"].toInt());

        LOG_VERBOSE("Received latest version information:\n"
                    " - version: %s (running %s)\n"
                    " - package: %s\n"
                    " - change log: %s")
                << latestVersion.asText()
                << VersionInfo().asText()
                << latestPackageUri << latestLogUri;

        // Is this newer than what we're running?
        UpdateAvailableDialog* dlg = new UpdateAvailableDialog;

        dlg->exec();

        delete dlg;
    }
};

Updater::Updater(QObject *parent) : QObject(parent)
{
    d = new Instance(this);
    connect(d->network, SIGNAL(finished(QNetworkReply*)), this, SLOT(gotReply(QNetworkReply*)));

    d->queryLatestVersion();
}

Updater::~Updater()
{
    delete d;
}

void Updater::gotReply(QNetworkReply* reply)
{
    d->handleReply(reply);
}

void Updater_Init(void)
{
    updater = new Updater;
}

void Updater_Shutdown(void)
{
    delete updater;
}
