/** @file nativetranslator.h  Savegame translator for old Doomsday-native formats.
 *
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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

#ifndef SAVEGAMETOOl_NATIVETRANSLATOR_H
#define SAVEGAMETOOl_NATIVETRANSLATOR_H

#include "packageformatter.h"

/**
 * Specialized PackageFormatter suitable for translating (old) Doomsday-native save formats.
 */
class NativeTranslator : public PackageFormatter
{
public:
    /// The game identity key of the source data is ambious. @ingroup errors
    DENG2_SUB_ERROR(ReadError, AmbigousGameIdError);

    /// Logical identifiers for supported save formats.
    enum FormatId {
        Doom,
        Heretic,
        Hexen
    };

public:
    NativeTranslator(FormatId formatId, de::String textualId, int magic,
                     QStringList knownExtensions, QStringList baseGameIdKeys);
    virtual ~NativeTranslator();

    bool recognize(de::Path path);

    void convert(de::Path oldSavePath);

private:
    DENG2_PRIVATE(d)
};

#endif // SAVEGAMETOOl_NATIVETRANSLATOR_H
