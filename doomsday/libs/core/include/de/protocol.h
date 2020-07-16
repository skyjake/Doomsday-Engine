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

#ifndef LIBCORE_PROTOCOL_H
#define LIBCORE_PROTOCOL_H

#include "de/libcore.h"
#include "de/list.h"
#include "de/reader.h"

/**
 * @defgroup protocol Protocol
 *
 * Classes that define the protocol for network communications.
 *
 * @ingroup net
 */

namespace de {

class Block;
class CommandPacket;
class Transmitter;
class Packet;
class Record;
class RecordPacket;
class String;

/**
 * The protocol is responsible for recognizing an incoming data packet and
 * constructing a specialized packet object of the appropriate type.
 *
 * @ingroup protocol
 */
class DE_PUBLIC Protocol
{
public:
    /// The response was not success. @ingroup errors
    DE_ERROR(ResponseError);

    /// The response to a command, query, or other message was FAILURE. @ingroup errors
    DE_SUB_ERROR(ResponseError, FailureError);

    /// The response to a command, query, or other message was DENY. @ingroup errors
    DE_SUB_ERROR(ResponseError, DenyError);

    /**
     * A constructor function examines a block of data and determines
     * whether a specialized Packet can be constructed based on the data.
     *
     * @param block  Block of data.
     *
     * @return  Specialized Packet, or @c NULL.
     */
    typedef Packet *(*Constructor)(const Block &);

    /// Reply types. @see reply()
    enum Reply {
        OK,         ///< Command performed successfully.
        FAILURE,    ///< Command failed.
        DENY        ///< Permission denied. No rights to perform the command.
    };

public:
    Protocol();

    virtual ~Protocol();

    /**
     * Registers a new constructor function.
     *
     * @param constructor  Constructor.
     */
    void define(Constructor constructor);

    /**
     * Interprets a block of data.
     *
     * @param block  Block of data that should contain a Packet of some type.
     *
     * @return  Specialized Packet, or @c NULL.
     */
    Packet *interpret(const Block &block) const;

    /*
     * Sends a command packet and waits for reply. This is intended for issuing
     * synchronous commands that can be responded to immediately.
     *
     * @param to        Transceiver over which to converse.
     * @param command   Packet to send.
     * @param response  If not NULL, the reponse packet is returned to caller here.
     *                  Otherwise the response packet is deleted.
     */
    //void syncCommand(Transmitter& to, const CommandPacket& command, RecordPacket** response = 0);

    /**
     * Sends a reply via a transmitter. This is used as a general response to
     * commands or any other received messages.
     *
     * @param to      Transmitter where to send the reply.
     * @param type    Type of reply.
     * @param record  Optional data to send along the reply. Protocol takes
     *                ownership of the record.
     */
    void reply(Transmitter &to, Reply type = OK, Record *record = 0);

    /**
     * Sends a reply via a transmitter. This is used as a general response to
     * commands or any other received messages.
     *
     * @param to       Transmitter where to send the reply.
     * @param type     Type of reply.
     * @param message  Optional message (human readable).
     */
    void reply(Transmitter &to, Reply type, const String &message);

private:
    typedef List<Constructor> Constructors;
    Constructors _constructors;
};

} // namespace de

#endif /* LIBCORE_PROTOCOL_H */
