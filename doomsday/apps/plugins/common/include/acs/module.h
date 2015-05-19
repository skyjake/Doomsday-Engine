/** @file module.h  Action Code Script (ACS) module.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_ACS_MODULE_H
#define LIBCOMMON_ACS_MODULE_H

#include <functional>
#include <doomsday/filesys/file.h>
#include <de/Block>
#include <de/Error>
#include <de/String>

namespace acs {

/**
 * Models a loadable code module for the ACS scripting system.
 */
class Module
{
    DENG2_NO_COPY  (Module)
    DENG2_NO_ASSIGN(Module)

public:
    /// Base class for bytecode format errors. @ingroup errors
    DENG2_ERROR(FormatError);

    /// Required/referenced constant (string-)value is missing. @ingroup errors
    DENG2_ERROR(MissingConstantError);

    /// Required/referenced (script) entry point data is missing. @ingroup errors
    DENG2_ERROR(MissingEntryPointError);

    /**
     * Stores information about an ACS script entry point.
     */
    struct EntryPoint
    {
        int const *pcodePtr       = nullptr;
        bool startWhenMapBegins   = false;
        de::dint32 scriptNumber   = 0;
        de::dint32 scriptArgCount = 0;
    };

public:
    /**
     * Returns @c true if data @a file appears to be valid ACS code module.
     */
    static bool recognize(/*de::IByteArray const &data*/ de::File1 const &file);

    /**
     * Loads an ACS @a code module (a copy is made).
     */
    static Module *newFromBytecode(de::Block const &code);

    /**
     * Loads an ACS code module from the specified @a file.
     */
    static Module *newFromFile(de::File1 const &file);

    /**
     * Provides readonly access to a constant (string-)value from the loaded code module.
     */
    de::String constant(int stringNumber) const;

    /**
     * Returns the total number of script entry points in the loaded code module.
     */
    int entryPointCount() const;

    /**
     * Returns @c true iff @a scriptNumber is a known entry point.
     */
    bool hasEntryPoint(int scriptNumber) const;

    /**
     * Lookup the EntryPoint data for the given @a scriptNumber.
     */
    EntryPoint const &entryPoint(int scriptNumber) const;

    /**
     * Iterate through the EntryPoints of the loaded code module.
     *
     * @param func  Callback to make for each EntryPoint.
     */
    de::LoopResult forAllEntryPoints(std::function<de::LoopResult (EntryPoint &)> func) const;

    /**
     * Provides readonly access to the loaded bytecode.
     */
    de::Block const &pcode() const;

private:
    Module();

    DENG2_PRIVATE(d)
};

} // namespace acs

#endif  // LIBCOMMON_ACS_MODULE_H
