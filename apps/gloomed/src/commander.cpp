/** @file commander.cpp  Controller for the Gloom viewer app.
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "commander.h"
#include "utils.h"

#include <de/Address>
#include <de/Beacon>
#include <de/CommandLine>
#include <de/Info>
#include <de/Waitable>
#include <the_Foundation/datagram.h>
#include <the_Foundation/process.h>

#include <QApplication>
#include <QDebug>

using namespace de;

static constexpr duint16 COMMAND_PORT = 14666;

DE_PIMPL(Commander)
, DE_OBSERVES(Beacon, Discovery)
, public Waitable
{
    tF::ref<iProcess>  proc;
    Beacon                beacon{{COMMAND_PORT, COMMAND_PORT + 4}};
    Address               address;
    tF::ref<iDatagram> commandSocket;

    Impl(Public *i) : Base(i)
    {
        beacon.audienceForDiscovery() += this;
        beacon.discover(0.0); // keep listening
    }

    void beaconFoundHost(const Address &host, const Block &message) override
    {
        if (self().isConnected()) return; // Ignore additional replies.

        qDebug("GloomEd beacon found:%s [%s]", host.asText().c_str(), message.c_str());

        if (message.beginsWith(DE_STR("GloomApp:")))
        {
            const Info    msg(message.mid(9));
            const duint16 commandPort = duint16(msg["port"].toUInt32());

            //beacon.stop();
            address = host;

            commandSocket.reset(new_Datagram());
            for (int i = 0; i < 10; ++i)
            {
                // Open a socket in the private port range.
                if (open_Datagram(commandSocket, duint16(Rangeui(0xc000, 0x10000).random())))
                {
                    break;
                }
            }
            if (!isOpen_Datagram(commandSocket))
            {
                qWarning("Failed to open UDP port for sending commands");
            }
            connect_Datagram(commandSocket, Address(host.hostName(), commandPort));

            post();
        }
    }
};

Commander::Commander()
    : d(new Impl(this))
{}

bool Commander::launch()
{
    d->address = Address();

    CommandLine cmd;
#if defined (MACOSX)
    cmd << convert(qApp->applicationDirPath() + "/../../../Gloom.app/Contents/MacOS/Gloom");
#endif
    d->proc.reset(cmd.executeProcess());
    return bool(d->proc);
}

void Commander::sendCommand(const String &command)
{
    if (!isConnected())
    {
        d->wait(10.0);
    }
    write_Datagram(d->commandSocket, command.toUtf8());
}

bool Commander::isLaunched() const
{
    if (!d->proc) return false;
    if (isRunning_Process(d->proc)) return true;
    return false;
}

bool Commander::isConnected() const
{
    return !d->address.isNull();
}
