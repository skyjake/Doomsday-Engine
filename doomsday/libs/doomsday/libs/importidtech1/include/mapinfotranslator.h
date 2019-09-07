/** @file mapinfotranslator.h  Hexen-format MAPINFO definition translator.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 Activision
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

#ifndef IMPORTIDTECH1_MAPINFOTRANSLATOR_H
#define IMPORTIDTECH1_MAPINFOTRANSLATOR_H

#include "importidtech1.h"

namespace idtech1 {

/**
 * Hexen MAPINFO => DED translator.
 */
class MapInfoTranslator
{
public:
    MapInfoTranslator();

    void reset();
    void merge(const ddstring_s &definitions, const de::String& sourcePath, bool sourceIsCustom);

    /**
     * Translate the current MAPINFO data set into DED syntax. Note that the internal
     * state of the definition database is modified in the process and will therefore
     * be reset automatically once translation has completed.
     *
     * @param translated        Definitions from non-custom sources are written here.
     * @param translatedCustom  Definitions from custom sources are written here.
     */
    void translate(de::String &translated, de::String &translatedCustom);

private:
    DE_PRIVATE(d)
};

} // namespace idtech1

#endif // IMPORTIDTECH1_MAPINFOTRANSLATOR_H
