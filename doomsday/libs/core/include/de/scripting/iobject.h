/** @file iobject.h  Interface for a Doomsday Script object.
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_IOBJECT_H
#define LIBCORE_IOBJECT_H

#include "de/record.h"

namespace de {

class Variable;

/**
 * Interface for a Doomsday Script object.
 *
 * Classes that implement this interface can be manipulated in scripts as
 * objects, because they have a Record that corresponds the native instance.
 *
 * Objects that implement IObject can be passed as arguments in
 * Process::scriptCall().
 *
 * @ingroup script
 */
class DE_PUBLIC IObject
{
public:
    virtual ~IObject() = default;

    /**
     * Returns the Record that contains the instance namespace of the object.
     */
    virtual Record &objectNamespace() = 0;

    /// @copydoc objectNamespace()
    virtual const Record &objectNamespace() const = 0;

    /**
     * Looks up a variable in the object namespace. Variables in subrecords can
     * be accessed using the member notation: `subrecordName.variableName`
     *
     * If the variable does not exist, a Record::NotFoundError is thrown.
     *
     * @param name  Variable name.
     *
     * @return  Variable.
     */
    inline Variable &operator [] (const String &name) {
        return objectNamespace()[name];
    }

    /**
     * Looks up a variable in the object namespace. Variables in subrecords can
     * be accessed using the member notation: `subrecordName.variableName`
     *
     * If the variable does not exist, a Record::NotFoundError is thrown.
     *
     * @param name  Variable name.
     *
     * @return  Variable.
     */
    inline const Variable &operator [] (const String &name) const {
        return objectNamespace()[name];
    }
};

} // namespace de

#endif // LIBCORE_IOBJECT_H

