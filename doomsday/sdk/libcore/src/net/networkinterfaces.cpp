/** @file networkinterfaces.cpp  Information about network interfaces.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "../src/net/networkinterfaces.h"
#include "de/Task"
#include "de/TaskPool"

#include <QNetworkInterface>
#include <QTimer>

namespace de {
namespace internal {

DENG2_PIMPL(NetworkInterfaces), public Lockable
{
    struct AddressQueryTask : public Task
    {
        NetworkInterfaces::Impl *d;

        AddressQueryTask(NetworkInterfaces::Impl *d) : d(d) {}

        void runTask() override
        {
            QList<QHostAddress> ipv6;
            foreach (QHostAddress addr, QNetworkInterface::allAddresses())
            {
                ipv6 << QHostAddress(addr.toIPv6Address());
            }

            // Submit the updated information.
            {
                DENG2_GUARD(d);
                d->addresses = ipv6;
                d->gotAddresses = true;
            }

            qDebug() << "Local IP addresses:" << ipv6;
        }
    };

    QList<QHostAddress> addresses; // lock before access
    TaskPool tasks;
    QTimer updateTimer;
    bool gotAddresses = false;

    Impl(Public *i) : Base(i)
    {
        updateTimer.setInterval(1000 * 60 * 1);
        updateTimer.setSingleShot(false);
        QObject::connect(&updateTimer, &QTimer::timeout, [this] ()
        {
            tasks.start(new AddressQueryTask(this));
        });
        updateTimer.start();
        tasks.start(new AddressQueryTask(this));
    }
};

NetworkInterfaces::NetworkInterfaces()
    : d(new Impl(this))
{}

QList<QHostAddress> NetworkInterfaces::allAddresses() const
{
    if (!d->gotAddresses)
    {
        // Too early...
        d->tasks.waitForDone();
    }

    DENG2_GUARD(d);
    return d->addresses;
}

NetworkInterfaces &NetworkInterfaces::get() // static
{
    static NetworkInterfaces nif;
    return nif;
}

} // namespace internal
} // namespace de
