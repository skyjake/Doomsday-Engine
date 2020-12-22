/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/protocol.h"
#include "de/recordpacket.h"
#include "de/textvalue.h"
#include "de/record.h"
#include "de/transmitter.h"
#include "de/log.h"

namespace de {

Protocol::Protocol()
{
    define(RecordPacket::fromBlock);
}

Protocol::~Protocol()
{}

void Protocol::define(Constructor constructor)
{
    _constructors.append(constructor);
}

Packet *Protocol::interpret(const Block &block) const
{
    for (Constructor constructor : _constructors)
    {
        if (Packet *p = constructor(block))
        {
            return p;
        }
    }
    return nullptr;
}

/*
void Protocol::syncCommand(Transmitter& to, const CommandPacket& command, RecordPacket** response)
{
    LOG_AS("Protocol::syncCommand");
    LOG_DEBUG("Sending: '%s' with args:\n%s") << command.command() << command.arguments();

    to << command;
    QScopedPointer<RecordPacket> rep(to.receivePacket<RecordPacket>());

    // Check the answer.
    if (rep->label() == "failure")
    {
        /// @throw FailureError The response to @a command was FAILURE.
        throw FailureError("Protocol::decree", "Command '" + command.command() +
            "' failed: " + rep->valueAsText("message"));
    }
    else if (rep->label() == "deny")
    {
        /// @throw DenyError The response to @a command was DENY.
        throw DenyError("Protocol::decree", "Command '" + command.command() +
            "' was denied: " + rep->valueAsText("message"));
    }

    LOG_DEBUG("Reply to the command '%s' was '%s':") << command.command() << rep->label();
    LOG_DEBUG("") << rep->record();

    if (response)
    {
        *response = rep.take();
    }
}
*/

void Protocol::reply(Transmitter &to, Reply type, Record *record)
{
    String label;
    switch (type)
    {
    case OK:
        label = "ok";
        break;

    case FAILURE:
        label = "failure";
        break;

    case DENY:
        label = "deny";
        break;
    }
    RecordPacket packet(label);
    if (record)
    {
        packet.take(record);
    }
    to << packet;
}

void Protocol::reply(Transmitter &to, Reply type, const String &message)
{
    Record* rec = new Record();
    if (!message.empty())
    {
        rec->addText("message", message);
    }
    reply(to, type, rec);
}

} // namespace de
