/**
 * @file monitor.h
 * Network traffic monitoring. @ingroup network
 *
 * Utilities for monitoring network traffic for development and debugging
 * purposes.
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG_NETWORK_MONITOR_H
#define LIBDENG_NETWORK_MONITOR_H

#include "dd_share.h"

#ifdef _DEBUG

/**
 * Updates monitored byte frequency counts with @a bytes.
 *
 * @param bytes  Buffer whose contents to count.
 * @param size   Size of the buffer.
 */
void Monitor_Add(const uint8_t* bytes, size_t size);

D_CMD(NetFreqs);

#endif

#endif // LIBDENG_NETWORK_MONITOR_H
