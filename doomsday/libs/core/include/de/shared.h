/** @file shared.h  Reference-counted, shared object.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_SHARED_H
#define LIBCORE_SHARED_H

#include "de/counted.h"

namespace de {

/**
 * Template for a shared object. The object gets created when the first user
 * calls hold(), and it gets automatically destroyed when all users release
 * their references.
 *
 * You must use the DE_SHARED_INSTANCE() macro to define where the static
 * instance pointer is located.
 *
 * @par Thread-safety
 *
 * Shared is not thread safe. The shared object can only be accessed from a
 * single thread.
 *
 * @ingroup data
 */
template <typename Type>
class Shared : public Counted, public Type
{
protected:
    static Shared<Type> *instance; ///< Instance pointer.

public:
    virtual ~Shared() {
        instance = 0;
    }

    static Shared<Type> *hold() {
        if (!instance) {
            instance = new Shared<Type>;
        }
        else {
            // Hold a reference to the shared data.
            holdRef(instance);
        }
        return instance;
    }
};

/**
 * Define the static instance pointer of a shared type.
 * @note This macro must be invoked from the global namespace.
 */
#define DE_SHARED_INSTANCE(TypeName) \
    namespace de { \
        template <> \
        Shared<TypeName> *Shared<TypeName>::instance = 0; \
    } // namespace de

} // namespace de

#endif // LIBCORE_SHARED_H
