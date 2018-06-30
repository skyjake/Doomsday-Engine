/** @file id1translator.h  Savegame translator for id Tech1 formats.
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

#ifndef SAVEGAMETOOL_ID1TRANSLATOR_H
#define SAVEGAMETOOL_ID1TRANSLATOR_H

#include "packageformatter.h"

/**
 * Specialized PackageFormatter suitable for translating id Tech 1 save formats.
 */
class Id1Translator : public PackageFormatter
{
public:
    /// The game identity key of the source data is ambigous. @ingroup errors
    DE_SUB_ERROR(ReadError, AmbigousGameIdError);

    /// Identifiers for supported save formats.
    enum FormatId {
        DoomV9,
        HereticV13
    };

public:
    Id1Translator(FormatId id, de::StringList knownExtensions, de::StringList baseGameIdKeys);
    virtual ~Id1Translator();

    de::String formatName() const;

    bool recognize(de::Path path);

    void convert(de::Path path);

private:
    DE_PRIVATE(d)
};

#endif // SAVEGAMETOOL_ID1TRANSLATOR_H
