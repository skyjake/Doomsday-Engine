/** @file module.h  Action Code Script (ACS) module.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include <de/block.h>
#include <de/error.h>
#include <de/string.h>

namespace acs {

/**
 * Models a loadable code module for the ACS scripting system.
 */
class Module
{
    DE_NO_COPY  (Module)
    DE_NO_ASSIGN(Module)

public:
    /// Base class for bytecode format errors. @ingroup errors
    DE_ERROR(FormatError);

    /// Required/referenced constant (string-)value is missing. @ingroup errors
    DE_ERROR(MissingConstantError);

    /// Required/referenced (script) entry point data is missing. @ingroup errors
    DE_ERROR(MissingEntryPointError);

    /**
     * Stores information about an ACS script entry point.
     */
    struct EntryPoint
    {
        const int *pcodePtr       = nullptr;
        bool startWhenMapBegins   = false;
        de::dint32 scriptNumber   = 0;
        de::dint32 scriptArgCount = 0;
    };

public:
    /**
     * Returns @c true if data @a file appears to be valid ACS code module.
     */
    static bool recognize(/*const de::IByteArray &data*/ const res::File1 &file);

    /**
     * Loads an ACS @a code module (a copy is made).
     */
    static Module *newFromBytecode(const de::Block &code);

    /**
     * Loads an ACS code module from the specified @a file.
     */
    static Module *newFromFile(const res::File1 &file);

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
    const EntryPoint &entryPoint(int scriptNumber) const;

    /**
     * Iterate through the EntryPoints of the loaded code module.
     *
     * @param func  Callback to make for each EntryPoint.
     */
    de::LoopResult forAllEntryPoints(const std::function<de::LoopResult (EntryPoint &)>& func) const;

    /**
     * Provides readonly access to the loaded bytecode.
     */
    const de::Block &pcode() const;

private:
    Module();

    DE_PRIVATE(d)
};

} // namespace acs

#endif  // LIBCOMMON_ACS_MODULE_H
