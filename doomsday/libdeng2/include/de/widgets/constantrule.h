/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBDENG2_CONSTANTRULE_H
#define LIBDENG2_CONSTANTRULE_H

#include "../Rule"

namespace de {

/**
 * The value of a constant rule never changes unless manually changed.
 * @ingroup widgets
 *
 * @see ScalarRule
 */
class DENG2_PUBLIC ConstantRule : public Rule
{
    Q_OBJECT

public:
    explicit ConstantRule(float constantValue);

    /**
     * Changes the value of the constant in the rule.
     */
    void set(float newValue);

protected:
    void update();

private:
    float _newValue;
};

} // namespace de

#endif // LIBDENG2_CONSTANTRULE_H
