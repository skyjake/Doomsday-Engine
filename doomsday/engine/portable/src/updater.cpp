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
#include <de/Log>
#include <QDateTime>
#include <QDesktopServices>
#include <QNetworkAccessManager>
#include <QSettings>
#include <QDebug>

static Updater* updater = 0;

#define STK_FREQUENCY       "updater/frequency"
#define STK_CHANNEL         "updater/channel"
#define STK_LAST_CHECKED    "updater/lastChecked"
#define STK_ONLY_MANUAL     "updater/onlyManually"
#define STK_DELETE          "updater/delete"
#define STK_DOWNLOAD_PATH   "updater/downloadPath"

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
    enum Frequency
    {
        Daily    = 0,
        Biweekly = 1,   // 3.5 days
        Weekly   = 2,   // 7 days
        Monthly  = 3    // 30 days
    };
    enum Channel
    {
        Stable   = 0,
        Unstable = 1
    };

    Updater* self;
    QNetworkAccessManager* network;

    Frequency checkFrequency;
    Channel channel;        ///< What kind of updates to check for.
    QDateTime lastCheckTime;///< Time of last check (automatic or manual).
    bool onlyCheckManually; ///< Should only check when manually requested.
    bool deleteAfterUpdate;  ///< Downloaded file is deleted afterwards.
    QString downloadPath;   ///< Path where the downloaded file is saved.

    Instance(Updater* up) : self(up)
    {
        // Fetch the current settings.
        QSettings st;
        checkFrequency = Frequency(st.value(STK_FREQUENCY, Weekly).toInt());
        channel = Channel(st.value(STK_CHANNEL, QString(DOOMSDAY_RELEASE_TYPE) == "Stable"? Stable : Unstable).toInt());
        lastCheckTime = st.value(STK_LAST_CHECKED).toDateTime();
        onlyCheckManually = st.value(STK_ONLY_MANUAL, false).toBool();
        deleteAfterUpdate = st.value(STK_DELETE, true).toBool();
        downloadPath = st.value(STK_DOWNLOAD_PATH,
                QDesktopServices::storageLocation(QDesktopServices::TempLocation)).toString();

        network = new QNetworkAccessManager(self);
    }

    ~Instance()
    {
        // Save settings.
        QSettings st;
        st.setValue(STK_FREQUENCY, int(checkFrequency));
        st.setValue(STK_CHANNEL, int(channel));
        st.setValue(STK_LAST_CHECKED, lastCheckTime);
        st.setValue(STK_ONLY_MANUAL, onlyCheckManually);
        st.setValue(STK_DELETE, deleteAfterUpdate);
        st.setValue(STK_DOWNLOAD_PATH, downloadPath);
    }

    QString composeCheckUri()
    {
        QString uri = QString(DOOMSDAY_HOMEURL) + "/latestbuild?";
        uri += QString("platform=") + PLATFORM_ID;
        uri += (channel == Stable? "&stable" : "&unstable");
        uri += "&graph";

        LOG_DEBUG("Check URI: ") << uri;
        return uri;
    }

    void queryLatestVersion()
    {
        lastCheckTime = QDateTime::currentDateTime();
        network->get(QNetworkRequest(composeCheckUri()));
    }

    void handleReply(QNetworkReply* reply)
    {
        reply->deleteLater(); // make sure it gets deleted

        QVariant result = parseJSON(QString::fromUtf8(reply->readAll()));
        qDebug() << result;
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
