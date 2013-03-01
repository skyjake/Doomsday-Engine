/**
 * @file dehreader.h
 * DeHackEd patch parser. @ingroup dehreader
 *
 * Parses DeHackEd patches and updates the engine's definition databases.
 *
 * @author Copyright &copy; 2013 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDEHREAD_DEHREADER_H
#define LIBDEHREAD_DEHREADER_H

#include "dehread.h"
#include <de/Block>

/**
 * Maximum number of nested patch file inclussions. Set to zero to disable.
 */
#define DEHREADER_INCLUDE_DEPTH_MAX         2

/// Flags used with @see readDehPatch() to alter read behavior.
enum DehReaderFlag
{
    NoInclude   = 0x1, ///< Including of other patch files is disabled.
    NoText      = 0x2, ///< Ignore Text patches.
    IgnoreEOF   = 0x4  ///< Ignore unexpected EOF characters in patches.
};
Q_DECLARE_FLAGS(DehReaderFlags, DehReaderFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(DehReaderFlags)

/**
 * Parses a text stream as a DeHackEd patch and updates the engine's definition
 * databases accordingly.
 *
 * @param patch  DeHackEd patch to parse.
 * @param flags  @ref DehReaderFlags
 */
void readDehPatch(const de::Block& patch, DehReaderFlags flags = 0);

#endif // LIBDEHREAD_DEHREADER_H
