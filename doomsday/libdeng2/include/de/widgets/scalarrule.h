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

#ifndef LIBDENG2_SCALARRULE_H
#define LIBDENG2_SCALARRULE_H

#include "rule.h"

#include "../Animation"
#include "../Time"

namespace de {

/**
 * Rule with a scalar value. The value is animated over time.
 */
class ScalarRule : public Rule
{
    Q_OBJECT

public:
    explicit ScalarRule(float initialValue, QObject *parent = 0);

    void set(float value, Time::Delta transition = 0);

    /**
     * Read-only access to the scalar animation.
     */
    Animation const &scalar() const {
        return _animation;
    }

protected:
    void update();

private:
    Animation _animation;
};

} // namespace de

#endif // LIBDENG2_SCALARRULE_H
