/**
 * @file protocol.h
 * Network protocol. @ingroup network
 *
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

#ifndef LIBDENG_NETWORK_PROTOCOL_H
#define LIBDENG_NETWORK_PROTOCOL_H

#include "sys_network.h"

/// Largest message sendable using the protocol.
#define PROTOCOL_MAX_DATAGRAM_SIZE (1 << 22) // 4 MB

/**
 * Send the data buffer over a TCP connection.
 * The data may be compressed with zlib.
 */
void Protocol_Send(void *data, size_t size, nodeid_t destination);

/**
 * Read a packet from the TCP connection and put it in the incoming
 * packet queue.  This function blocks until the entire packet has
 * been read, so large packets should be avoided during normal
 * gameplay.
 *
 * @return @c true, if a packet was successfully received.
 */
boolean Protocol_Receive(nodeid_t from);

/**
 * Free a message buffer.
 *
 * @param handle  Message buffer received with Protocol_Receive().
 */
void Protocol_FreeBuffer(void *handle);

#endif // LIBDENG_NETWORK_PROTOCOL_H
