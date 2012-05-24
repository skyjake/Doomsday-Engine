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
#include <de/Log>
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QSettings>

static Updater* updater = 0;

#define WEBSITE_URL "http://dengine.net/"

struct Updater::Instance
{
    enum Frequency
    {
        Daily,
        Biweekly,   // 3.5 days
        Weekly,     // 7 days
        Monthly     // 30 days
    };
    enum Channel
    {
        Stable,
        Unstable
    };

    Updater* self;
    QNetworkAccessManager* network;
    Frequency checkFrequency;
    Channel channel;        ///< What kind of updates to check for.
    QDateTime lastCheckTime;///< Time of last check (automatic or manual).
    bool onlyCheckManually; ///< Should only check when manually requested.
    bool trashAfterUpdate;  ///< Downloaded file is moved to trash afterwards.
    QString downloadPath;   ///< Path where the downloaded file is saved.

    Instance(Updater* up) : self(up)
    {
        network = new QNetworkAccessManager(self);
    }

    ~Instance()
    {}

    void handleReply(QNetworkReply* reply)
    {
        reply->deleteLater(); // make sure it gets deleted


    }
};

Updater::Updater(QObject *parent) : QObject(parent)
{
    d = new Instance(this);
    connect(d->network, SIGNAL(finished(QNetworkReply*)), this, SLOT(gotReply(QNetworkReply*)));
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
