/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBDENG2_CHANNEL_H
#define LIBDENG2_CHANNEL_H

#include "../Transmitter"
#include "../Socket"

#include <QObject>

namespace de
{
    /**
     * Multiplexes messages on a socket.
     *
     * @ingroup net
     */
    class Channel : public QObject, public Transmitter
    {
        Q_OBJECT

        /// The link is no longer operable. @ingroup errors
        DEFINE_ERROR(SocketError);

    public:
        /**
         * Constructs a channel.
         *
         * @param channelNumber  Number of the channel.
         * @param socket         Socket over which the channel operates.
         * @param parent         Owner of the channel.
         */
        explicit Channel(duint channelNumber, Socket& socket, QObject *parent = 0);

        // Implements Transmitter.
        virtual void send(const IByteArray &data);

    signals:
        void messageReady();

    public slots:
        void checkMessageReady();
        void socketDestroyed();

    private:
        duint _channelNumber;
        Socket* _socket;
    };
}

#endif // LIBDENG2_CHANNEL_H
