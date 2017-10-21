/** @file isizerule.h  Interface for objects providing size rules.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_ISIZERULE_H
#define LIBDENG2_ISIZERULE_H

#include "../Rule"

namespace de {

/**
 * Interface for objects providing size rules.
 * @ingroup widgets
 */
class DENG2_PUBLIC ISizeRule
{
public:
    virtual ~ISizeRule() {}

    virtual Rule const &width() const = 0;
    virtual Rule const &height() const = 0;
};

} // namespace de

#endif // LIBDENG2_ISIZERULE_H
